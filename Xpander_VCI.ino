// ============================================================
// File: Xpander_VCI.ino
// Mô tả: Entry point - setup(), loop(), FreeRTOS tasks
// Platform: ESP32 + MCP2515
// ============================================================
#include <mcp_can.h>
#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "config.h"
#include "vehicle_data.h"

// ============================================================
// ĐỊNH NGHĨA BIẾN TOÀN CỤC (khai báo extern ở vehicle_data.h)
// ============================================================
VehicleData xData;
SasInfo sasInfo; // <--- THÊM DÒNG NÀY VÀO ĐÂY
SemaphoreHandle_t xDataMutex = NULL;
volatile uint32_t lastLiveDataUpdateMs = 0;
volatile uint32_t lastRpmUpdateMs = 0;
volatile bool flagForceLiveRefresh = false;

// Task handles
TaskHandle_t hTaskCAN  = NULL;
TaskHandle_t hTaskTP   = NULL;
TaskHandle_t hTaskPrint = NULL;

// ============================================================
// KHAI BÁO HÀM TỪCÁC FILE KHÁC
// ============================================================
extern MCP_CAN CAN;
extern void setupCAN();
extern bool canCheckBusOff();
extern void canFlushBuffer();
extern volatile int currentPage;   // ← THÊM vào đây
extern bool canSendMode01(uint8_t pid);
extern bool canSendMode21(uint8_t lid);
extern bool canReceive(long unsigned int* rxId, unsigned char* len,
                       unsigned char* rxBuf, uint32_t timeoutMs);
extern void parseMode01(uint8_t pid, unsigned char* rxBuf, uint8_t len);
extern void parseMode21(uint8_t lid, unsigned char* rxBuf, uint8_t len);
extern void printLiveData();
extern void printModuleInfo();
extern void printDTC();
extern String readVIN();
extern void setupNextion();
extern void taskNextionRX(void* pvParameters);
extern void taskNextionTX(void* pvParameters);
extern bool eps_read_steering_angle();
extern void taskSteering(void* pvParameters);
extern void taskActuator(void* pvParameters);
extern bool sas_read_module_info();
extern void updateSASInfoPage();
extern void updateAboutPage();
extern bool initSD();

// Live data polling intervals. Keep RPM independent from the slow PID scan.
static const uint32_t LIVE_PID_TIMEOUT_MS     = 50;
static const uint32_t LIVE_RPM_INTERVAL_MS    = 100;
static const uint32_t LIVE_FAST_SLOT_MS       = 100;
static const uint32_t LIVE_PEDAL_SLOT_MS      = 300;
static const uint32_t LIVE_SLOW_INTERVAL_MS   = 400;
static const uint32_t LIVE_MODE21_INTERVAL_MS = 800;

bool pollMode01PID(uint8_t pid, uint32_t timeoutMs) {
  if (!canSendMode01(pid)) return false;

  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    if (CAN_MSGAVAIL == CAN.checkReceive()) {
      long unsigned int rxId;
      unsigned char len = 0;
      unsigned char rxBuf[8];
      CAN.readMsgBuf(&rxId, &len, rxBuf);

      if (rxId != CAN_ID_ECM_RESP && rxId != CAN_ID_TCM_RESP) continue;

      if (len >= 3 && rxBuf[1] == 0x41 && rxBuf[2] == pid) {
        parseMode01(pid, rxBuf, len);
        return true;
      }

      if (len >= 4 && rxBuf[1] == 0x7F && rxBuf[2] == 0x01) {
        return false;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  return false;
}

static bool pollMode21LID(uint8_t lid, uint32_t timeoutMs) {
  if (!canSendMode21(lid)) return false;

  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    if (CAN_MSGAVAIL == CAN.checkReceive()) {
      long unsigned int rxId;
      unsigned char len = 0;
      unsigned char rxBuf[8];
      CAN.readMsgBuf(&rxId, &len, rxBuf);

      if (rxId != CAN_ID_ECM_RESP) continue;
      if (len >= 3 && rxBuf[1] == 0x61 && rxBuf[2] == lid) {
        parseMode21(lid, rxBuf, len);
        Serial.printf("[CAN] Mode21 LID=0x%02X OK\n", lid);
        return true;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  return false;
}

// ============================================================
// TASK 1: QUÉT PID LIÊN TỤCg*
// ============================================================
void taskReadCAN(void* pvParameters) {
  const uint8_t fastCorePids[] = {
    0x0D, // Speed
    0x11, // TPS
  };

  const uint8_t pedalPids[] = {
    0x49, // Pedal D
    0x4A, // Pedal E
  };

  const uint8_t slowPids[] = {
    0x05, // ECT
    0x0F, // IAT
    0x43, // Engine Load
    0x04, // Calc Load
    0x0E, // Timing Advance
    0x44, // Lambda
    0x06, // STF
    0x07, // LTF
    0x0B, // MAP
    0x33, // Baro
    0x14, // O2 Front
    0x15, // O2 Rear
    0x42, // Control Volt
    0x1F, // Run Time
    0x51, // Fuel Type
    0x46, // Ambient Temp
    0x03, // Fuel Sys Status
  };

  const uint8_t mode21_lids[] = {0x1D, 0x1E};
  const uint8_t totalFast  = sizeof(fastCorePids) / sizeof(fastCorePids[0]);
  const uint8_t totalPedal = sizeof(pedalPids) / sizeof(pedalPids[0]);
  const uint8_t totalSlow  = sizeof(slowPids) / sizeof(slowPids[0]);
  const uint8_t total21    = sizeof(mode21_lids) / sizeof(mode21_lids[0]);

  LOCK_DATA {
    xData.injectorMs     = FIX_INJECTOR_MS;
    xData.throttleActPct = FIX_THROTTLE_PCT;
    xData.brakeBoostV    = FIX_BRAKE_VOLT;
  }
  UNLOCK_DATA;

  uint8_t idxFast = 0;
  uint8_t idxPedal = 0;
  uint8_t idxSlow = 0;
  uint8_t idx21 = 0;
  uint32_t lastRpmPoll = 0;
  uint32_t lastFastPoll = 0;
  uint32_t lastPedalPoll = 0;
  uint32_t lastSlowPoll = 0;
  uint32_t lastMode21Poll = 0;
  uint32_t lastLiveLog = 0;
  uint32_t lastStaleWatchdog = 0;

  for (;;) {
    uint32_t now = millis();

    static uint32_t lastErrorCheck = 0;
    if (now - lastErrorCheck > 5000) {
      lastErrorCheck = now;
      if (canCheckBusOff()) {
        Serial.println("[CAN] Bus-Off! Auto-reset...");
        setupCAN();
        Serial.println("[CAN] Bus-Off recovered.");
      }
    }

    if (currentPage == 11 && now - lastLiveLog >= 1000) {
      float rpmSnapshot = 0;
      uint32_t rpmAge = (lastRpmUpdateMs == 0) ? now : (now - lastRpmUpdateMs);
      uint32_t liveAge = (lastLiveDataUpdateMs == 0) ? now : (now - lastLiveDataUpdateMs);
      LOCK_DATA {
        rpmSnapshot = xData.rpm;
      }
      UNLOCK_DATA;
      Serial.printf("[LIVE] rpm=%.0f rpmAge=%lums liveAge=%lums\n",
                    rpmSnapshot, (unsigned long)rpmAge, (unsigned long)liveAge);
      lastLiveLog = now;
    }

    if (currentPage == 11) {
      uint32_t rpmAge = (lastRpmUpdateMs == 0) ? now : (now - lastRpmUpdateMs);
      uint32_t liveAge = (lastLiveDataUpdateMs == 0) ? now : (now - lastLiveDataUpdateMs);
      if ((rpmAge > 2000 || liveAge > 3000) && now - lastStaleWatchdog >= 1000) {
        Serial.printf("[LIVE] Warning: RPM stale for %lums, liveAge=%lums, flushing CAN RX buffer\n",
                      (unsigned long)rpmAge, (unsigned long)liveAge);
        canFlushBuffer();
        idxFast = 0;
        idxPedal = 0;
        idxSlow = 0;
        lastRpmPoll = now - LIVE_RPM_INTERVAL_MS;
        lastFastPoll = now - LIVE_FAST_SLOT_MS;
        lastPedalPoll = now - LIVE_PEDAL_SLOT_MS;
        lastSlowPoll = now - LIVE_SLOW_INTERVAL_MS;
        lastMode21Poll = now - LIVE_MODE21_INTERVAL_MS;
        flagForceLiveRefresh = true;
        lastStaleWatchdog = now;
      }
    }

    if (now - lastRpmPoll >= LIVE_RPM_INTERVAL_MS) {
      lastRpmPoll = now;
      pollMode01PID(0x0C, LIVE_PID_TIMEOUT_MS);
    } else if (now - lastFastPoll >= LIVE_FAST_SLOT_MS) {
      lastFastPoll = now;
      pollMode01PID(fastCorePids[idxFast], LIVE_PID_TIMEOUT_MS);
      idxFast = (idxFast + 1) % totalFast;
    } else if (now - lastPedalPoll >= LIVE_PEDAL_SLOT_MS) {
      lastPedalPoll = now;
      pollMode01PID(pedalPids[idxPedal], LIVE_PID_TIMEOUT_MS);
      idxPedal = (idxPedal + 1) % totalPedal;
    } else if (now - lastSlowPoll >= LIVE_SLOW_INTERVAL_MS) {
      lastSlowPoll = now;
      pollMode01PID(slowPids[idxSlow], LIVE_PID_TIMEOUT_MS);
      idxSlow = (idxSlow + 1) % totalSlow;
    } else if (now - lastMode21Poll >= LIVE_MODE21_INTERVAL_MS) {
      lastMode21Poll = now;
      uint8_t lid = mode21_lids[idx21];
      idx21 = (idx21 + 1) % total21;
      pollMode21LID(lid, LIVE_PID_TIMEOUT_MS);

      LOCK_DATA {
        bool engineOn = (xData.rpm > 400);
        xData.ignitionSw = engineOn;
        xData.fuelPumpRelay = engineOn;
        xData.crankingSignal = (xData.rpm > 50 && xData.rpm < 400);
      }
      UNLOCK_DATA;
    }

    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

// ============================================================
// TASK 2: IN DỮ LIỆU RA SERIAL (Thay thế Nextion tạm thời)
// ============================================================
void taskPrintSerial(void* pvParameters) {
  // Chờ data hợp lệ trước khi in
  Serial.println("[PRINT] Cho data tu ECU...");
  while (!xData.dataValid) {
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  // Chờ thêm 1s cho ECU ổn định rồi mới đọc DTC
  vTaskDelay(pdMS_TO_TICKS(1000));
  printDTC();

  uint8_t printCount = 0;
  for (;;) {
    printLiveData();
    printCount++;
    if (printCount % 10 == 0) {
      printModuleInfo();
    }
    static uint32_t lastHeapLog = 0;
    if (millis() - lastHeapLog > 30000) {
      Serial.printf("[HEAP] Free=%u Min=%u\n",
        ESP.getFreeHeap(), ESP.getMinFreeHeap());
      lastHeapLog = millis();
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

// ============================================================
// ĐỌC VIN (ISO-TP) - Giữ từ code cũ, đã kiểm chứng
// ============================================================
String readVIN() {
  const String fallbackVIN = CACHED_VIN;
  Serial.println("[VIN] Dang doc VIN tu ECU...");

  // Xả buffer
  long unsigned int tid; unsigned char tlen, tbuf[8];
  while (!digitalRead(CAN_INT_PIN)) CAN.readMsgBuf(&tid, &tlen, tbuf);

  unsigned char txBuf[8] = {0x02, 0x09, 0x02, 0, 0, 0, 0, 0};
  if (CAN.sendMsgBuf(CAN_ID_ECM_REQ, 0, 8, txBuf) != CAN_OK) {
    Serial.println("[VIN] Gui lenh that bai -> Dung VIN cache");
    return fallbackVIN;
  }

  String vin = "";
  long start = millis();
  bool firstFrame = false;
  int  expectSeq  = 1;
  uint32_t targetId = CAN_ID_ECM_REQ;

  while (millis() - start < 2000) {
    if (!digitalRead(CAN_INT_PIN)) {
      long unsigned int rxId;
      unsigned char len = 0, rxBuf[8];
      CAN.readMsgBuf(&rxId, &len, rxBuf);

      if (rxId == CAN_ID_ECM_RESP || rxId == CAN_ID_TCM_RESP) {

        if (!firstFrame && (rxBuf[0] & 0xF0) == 0x10) {
          targetId = (rxId == CAN_ID_ECM_RESP) ? CAN_ID_ECM_REQ : CAN_ID_TCM_REQ;
          for (int i = 5; i < 8; i++) vin += (char)rxBuf[i];
          firstFrame = true;
          expectSeq  = 1;
          // Gửi Flow Control
          unsigned char fc[8] = {0x30, 0x00, 0x0A, 0, 0, 0, 0, 0};
          CAN.sendMsgBuf(targetId, 0, 8, fc);
          start = millis(); // Reset timeout

        } else if (firstFrame && (rxBuf[0] & 0xF0) == 0x20) {
          int seq = rxBuf[0] & 0x0F;
          if (seq == expectSeq) {
            for (int i = 1; i < 8; i++) {
              if (rxBuf[i] >= 32 && rxBuf[i] <= 126) vin += (char)rxBuf[i];
            }
            expectSeq++;
          }
        }

        if (vin.length() >= 17) {
          String result = vin.substring(0, 17);
          LOCK_DATA { xData.vin = result; } UNLOCK_DATA;
          Serial.printf("[VIN] Doc thanh cong: %s\n", result.c_str());
          return result;
        }
      }
    }
  }

  Serial.println("[VIN] Timeout -> Dung VIN cache");
  LOCK_DATA { xData.vin = fallbackVIN; } UNLOCK_DATA;
  return fallbackVIN;
}
// Thêm task này
void taskTesterPresent(void* pvParameters) {
  unsigned char tpFrame[8] = {0x02, 0x3E, 0x02, 0, 0, 0, 0, 0};
  for (;;) {
    CAN.sendMsgBuf(0x7DF, 0, 8, tpFrame);
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}
// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(SERIAL_DEBUG_BAUDRATE);
  delay(1000);
  Serial.println("\n=== Xpander VCI - Khoi dong ===");
  // Tạo Mutex TRƯỚC KHI tạo task
  xDataMutex = xSemaphoreCreateMutex();
  if (xDataMutex == NULL) {
    Serial.println("[ERR] Khong tao duoc Mutex! Dung chuong trinh.");
    while(1);
  }

  setupCAN();

  // Đọc VIN ngay khi khởi động
  readVIN();
  sas_read_module_info();
  setupNextion();
  if (SD_ENABLED) {
    initSD();
  }
  // Đọc DTC khi mới bật máy
  // DTC sẽ được đọc trong taskPrintSerial sau khi có data

  // Tạo FreeRTOS Tasks
  xTaskCreate(taskReadCAN,    "CAN",   STACK_CAN_TASK,    NULL, PRIORITY_CAN_TASK,    &hTaskCAN);
  xTaskCreate(taskTesterPresent, "TP", 2048, NULL, 1, &hTaskTP);
  xTaskCreate(taskSteering, "STEER", 6144, NULL, 1, NULL);
  xTaskCreate(taskActuator, "ACT", 6144, NULL, 1, NULL);
  xTaskCreate(taskNextionRX, "NX_RX", 4096, NULL, 2, NULL);
  xTaskCreate(taskNextionTX, "NX_TX", 12288, NULL, 1, NULL);
  xTaskCreate(taskPrintSerial,"PRINT", STACK_NEXTION_TASK, NULL, PRIORITY_NEXTION_TASK, &hTaskPrint);

  Serial.println("[INIT] San sang! Dang doc du lieu...\n");
}

// ============================================================
// LOOP - Trống vì dùng FreeRTOS
// ============================================================
void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
