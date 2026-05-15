#include "config.h"
#include "vehicle_data.h"

extern bool canSendRaw(uint32_t id, unsigned char* buf, uint8_t len);
extern bool canReceive(long unsigned int* rxId, unsigned char* len,
                       unsigned char* rxBuf, uint32_t timeoutMs);
extern TaskHandle_t hTaskCAN;
extern void nxSendCmd(const String& cmd);

// Bien trang thai
volatile uint8_t flagInjectorTest = 0;   // 0=idle, 1-4=test injector N
volatile bool actuatorRunning = false;

static float getActuatorRpm() {
  float rpm = 0.0f;
  LOCK_DATA {
    rpm = xData.rpm;
  }
  UNLOCK_DATA;
  return rpm;
}

// Reset button ve OFF sau khi test xong
void resetInjectorButton(uint8_t injNum) {
  String cmd = "bt" + String(injNum - 1) + ".val=0";
  nxSendCmd(cmd);
}

bool ecm_injector_cutoff_test(uint8_t injNum) {
  if (injNum < 1 || injNum > 4) return false;
  if (actuatorRunning) {
    Serial.println("[ACT] Test dang chay, bo qua");
    resetInjectorButton(injNum);
    return false;
  }

  Serial.printf("[ACT] === Test Injector %d ===\n", injNum);

  // Safety: RPM > 600
  if (getActuatorRpm() < 600) {
    Serial.println("[ACT] RPM < 600, can no may truoc");
    nxSendCmd("t_warn.txt=\"RPM too low!\"");
    resetInjectorButton(injNum);
    return false;
  }

  actuatorRunning = true;
  vTaskSuspend(hTaskCAN);
  vTaskDelay(pdMS_TO_TICKS(50));

  // Disable 3 button con lai
  for (int i = 0; i < 4; i++) {
    if (i != injNum - 1) {
      nxSendCmd("bt" + String(i) + ".val=0");
    }
  }

  // STEP 1: START
  unsigned char req_start[8] = {0x04,0x30,0x10,0x07,injNum,0,0,0};
  unsigned char resp[8];
  long unsigned int rxId;
  unsigned char rxLen;

  if (!canSendRaw(CAN_ID_ECM_REQ, req_start, 8)) {
    Serial.println("[ACT] Send START fail");
    actuatorRunning = false;
    vTaskResume(hTaskCAN);
    resetInjectorButton(injNum);
    return false;
  }

  // Cho ACK 200ms
  bool gotAck = false;
  unsigned long ackStart = millis();
  while (millis() - ackStart < 200) {
    if (canReceive(&rxId, &rxLen, resp, 50)) {
      if (rxId == CAN_ID_ECM_RESP && resp[1] == 0x70 &&
          resp[2] == 0x10 && resp[3] == 0x07) {
        gotAck = true;
        break;
      }
      if (rxId == CAN_ID_ECM_RESP && resp[1] == 0x7F) {
        Serial.printf("[ACT] NRC=0x%02X\n", resp[3]);
        actuatorRunning = false;
        vTaskResume(hTaskCAN);
        resetInjectorButton(injNum);
        return false;
      }
    }
  }

  if (!gotAck) {
    Serial.println("[ACT] Khong nhan ACK");
    actuatorRunning = false;
    vTaskResume(hTaskCAN);
    resetInjectorButton(injNum);
    return false;
  }

  Serial.printf("[ACT] Injector %d DANG NGAT\n", injNum);

  // STEP 2: POLL 150ms
  unsigned char req_poll[8] = {0x03,0x30,0x10,0x01,0,0,0,0};
  unsigned long testStart = millis();
  unsigned long lastPoll = 0;
  int missed = 0;
  bool completed = false;

  while (millis() - testStart < 15000) {
    // Safety: ABORT neu RPM < 400
    if (getActuatorRpm() < 400) {
      Serial.println("[ACT] RPM tut, ABORT");
      break;
    }

    if (millis() - lastPoll >= 150) {
      lastPoll = millis();
      canSendRaw(CAN_ID_ECM_REQ, req_poll, 8);

      bool gotResp = false;
      unsigned long pollStart = millis();
      while (millis() - pollStart < 300) {
        if (canReceive(&rxId, &rxLen, resp, 50)) {
          if (rxId == CAN_ID_ECM_RESP && resp[1] == 0x70 &&
              resp[3] == 0x01) {
            gotResp = true;
            break;
          }
        }
      }

      if (!gotResp) {
        if (++missed >= 2) {
          Serial.println("[ACT] Poll x2 fail");
          break;
        }
        continue;
      }
      missed = 0;

      int elapsed = millis() - testStart;
      Serial.printf("[ACT] Status=0x%02X (%ds)\n", resp[4], elapsed / 1000);

      if (resp[4] == 0x00) {
        Serial.printf("[ACT] COMPLETE (%dms)\n", elapsed);
        completed = true;
        break;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  actuatorRunning = false;
  vTaskResume(hTaskCAN);
  resetInjectorButton(injNum);
  Serial.printf("[ACT] Test ket thuc: %s\n", completed ? "OK" : "FAIL/ABORT");
  return completed;
}

void taskActuator(void* pvParameters) {
  for (;;) {
    if (flagInjectorTest >= 1 && flagInjectorTest <= 4) {
      uint8_t inj = flagInjectorTest;
      flagInjectorTest = 0;
      ecm_injector_cutoff_test(inj);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
