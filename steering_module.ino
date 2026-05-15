// ============================================================
// File: steering_module.ino
// Mô tả: EPS Steering Angle reading + FreeRTOS task
// ============================================================
#include "config.h"
#include "vehicle_data.h"

extern volatile int currentPage;
extern void updateSteeringPage();
extern bool kwp_read_data_by_lid(uint16_t tx_id, uint16_t rx_id,
                                  uint8_t lid, uint8_t* buf, uint16_t* len);
extern TaskHandle_t hTaskCAN;

// Định nghĩa biến toàn cục
SteeringData steering;
extern SasInfo sasInfo; // <--- THÊM DÒNG NÀY VÀO ĐÂY
// ============================================================
// ĐỌC GÓC ĐÁNH LÁI TỪ EPS (LID 0x33)
// ============================================================
bool eps_read_steering_angle() {
  uint8_t  buf[64];
  uint16_t len;

  if (!kwp_read_data_by_lid(EPS_REQ_ID, EPS_RESP_ID, 0x33, buf, &len)) {
    steering.valid = false;
    return false;
  }

  if (len < 8) {
    Serial.printf("[EPS] Response qua ngan: %d bytes\n", len);
    steering.valid = false;
    return false;
  }

  // buf[0]=0x61, buf[1]=0x33, buf[2..3]=reserved
  // buf[4..5] = angle raw, buf[6..7] = velocity raw
  uint16_t rawA = ((uint16_t)buf[4] << 8) | buf[5];

  uint8_t statusFlags = (rawA >> 14) & 0x03;   // 2 MSB - quan sát dần
  steering.angleDeg   = ((rawA & 0x3FFF) * 0.5f) - 2048.0f;
  steering.lastUpdate = millis();
  steering.valid      = true;

  Serial.printf("[EPS] Angle = %.1f deg | status_flags = %d\n",
                steering.angleDeg, statusFlags);
  return true;
}

// ============================================================
// TASK: Poll EPS 10Hz khi user đang xem trang monitor(sas)
// ============================================================
void taskSteering(void* pvParameters) {
  for (;;) {
    if (currentPage == 9) {
      vTaskSuspend(hTaskCAN);
      vTaskDelay(pdMS_TO_TICKS(10));
      bool ok = eps_read_steering_angle();
      vTaskResume(hTaskCAN);
      if (ok) updateSteeringPage();
      vTaskDelay(pdMS_TO_TICKS(100));   // 10 Hz
    } else {
      steering.valid = false;
      vTaskDelay(pdMS_TO_TICKS(500));
    }
  }
}
//
// ============================================================
// SAS MODULE INFO — CHỈ LID 0x87 (0x9C trả NRC 0x12, bỏ)
// payload[0]=0x5A, [1]=0x87, [2..11]=header
// payload[12..19] = ASCII part number "8600A732"
// payload[20..21] = padding (spaces)
// ============================================================
bool sas_read_module_info() {
  vTaskSuspend(hTaskCAN);
  vTaskDelay(pdMS_TO_TICKS(50));

  uint8_t buf[32];
  uint16_t len;
  bool ok = false;

  if (kwp_read_ecu_id(SAS_REQ_ID, SAS_RESP_ID, 0x87, buf, &len)) {
    if (len >= 20 && buf[0] == 0x5A && buf[1] == 0x87) {
      // Part number ở buf[12..19] (8 bytes ASCII)
      char pn[9] = {0};
      memcpy(pn, &buf[12], 8);
      pn[8] = '\0';
      // Trim trailing spaces
      for (int i = 7; i >= 0 && pn[i] == ' '; i--) pn[i] = '\0';

      strncpy(sasInfo.partNumber87, pn, sizeof(sasInfo.partNumber87) - 1);
      sasInfo.partNumber87[sizeof(sasInfo.partNumber87) - 1] = '\0';
      Serial.printf("[SAS] Part Number: %s\n", sasInfo.partNumber87);
      ok = true;
    } else {
      Serial.printf("[SAS] Bad response: buf[0]=%02X buf[1]=%02X len=%d\n",
                    buf[0], buf[1], len);
    }
  } else {
    Serial.println("[SAS] Module Info: no response");
  }

  sasInfo.valid = ok;
  vTaskResume(hTaskCAN);
  return ok;
}
