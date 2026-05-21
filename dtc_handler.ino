// ============================================================
// File: dtc_handler.ino
// Mô tả: Read/Clear DTC cho ECM + SAS
//        Dùng SID 0x18 (KWP) qua ISO-TP
// ============================================================
#include "config.h"
#include "vehicle_data.h"

extern bool isotp_request(uint16_t tx_id, uint16_t rx_id,
                           const uint8_t* req, uint8_t reqLen,
                           uint8_t* resp, uint16_t* respLen, uint32_t timeoutMs);
extern TaskHandle_t hTaskCAN;
extern TaskHandle_t hTaskTP;
extern String lookupDTC(const String& code);

// ============================================================
// DECODE 2 BYTE → CHUỖI MÃ LỖI (P0010, C0123, ...)
// ============================================================
String decodeDTC(uint8_t hi, uint8_t lo) {
  if (hi == 0 && lo == 0) return "";
  uint16_t raw = ((uint16_t)hi << 8) | lo;
  char letter = "PCBU"[raw >> 14];
  char buf[8];
  sprintf(buf, "%c%d%03X", letter, (raw >> 12) & 3, raw & 0x0FFF);
  String code = String(buf);
  String desc = lookupDTC(code);
  if (desc.length() > 0) return code + ": " + desc;
  return code;
}

// ============================================================
// PARSE PAYLOAD SID 0x18 RESPONSE
// payload[0]=0x58, payload[1]=count, sau đó DTC triplets
// ============================================================
String parseDTCPayload(uint8_t* resp, uint16_t len) {
  if (len < 2)            return "Timeout";
  if (resp[0] != 0x58)    return "Bad Response";
  uint8_t count = resp[1];
  if (count == 0)          return "No Error";

  String result = "";
  for (int i = 0; i < count; i++) {
    uint8_t idx = 2 + i * 3;
    if (idx + 1 >= len) break;
    String dtc = decodeDTC(resp[idx], resp[idx + 1]);
    if (dtc != "") {
      if (result != "") result += "\r\n";
      result += dtc;
    }
  }
  return (result == "") ? "No Error" : result;
}

// ============================================================
// ECM READ DTC — SID 0x18 (KWP)
// Request:  04 18 00 FF 00 00 00 00
// ============================================================
String readECMDTC() {
  vTaskSuspend(hTaskCAN);
  if (hTaskTP != NULL) vTaskSuspend(hTaskTP);
  vTaskDelay(pdMS_TO_TICKS(30));
  vTaskDelay(pdMS_TO_TICKS(50));

  uint8_t req[4]  = {0x18, 0x00, 0xFF, 0x00};
  uint8_t resp[64];
  uint16_t respLen;
  String result = "Timeout";

  if (isotp_request(CAN_ID_ECM_REQ, CAN_ID_ECM_RESP,
                    req, 4, resp, &respLen, ISOTP_TIMEOUT_MS)) {
    result = parseDTCPayload(resp, respLen);
  }

  Serial.printf("[DTC ECM] %s\n", result.c_str());
  if (hTaskTP != NULL) vTaskResume(hTaskTP);
  vTaskResume(hTaskCAN);
  return result;
}

// ============================================================
// ECM CLEAR DTC — SID 0x14
// Request:  03 14 FF 00 00 00 00 00
// Response: resp[0] == 0x54 → success
// ============================================================
bool clearECMDTC() {
  vTaskSuspend(hTaskCAN);
  if (hTaskTP != NULL) vTaskSuspend(hTaskTP);
  vTaskDelay(pdMS_TO_TICKS(30));
  vTaskDelay(pdMS_TO_TICKS(50));

  uint8_t req[3]  = {0x14, 0xFF, 0x00};
  uint8_t resp[16];
  uint16_t respLen;
  bool success = false;

  if (isotp_request(CAN_ID_ECM_REQ, CAN_ID_ECM_RESP,
                    req, 3, resp, &respLen, ISOTP_TIMEOUT_MS)) {
    success = (respLen >= 1 && resp[0] == 0x54);
  }

  Serial.printf("[DTC ECM] Clear %s\n", success ? "OK" : "FAIL");
  if (hTaskTP != NULL) vTaskResume(hTaskTP);
  vTaskResume(hTaskCAN);
  return success;
}

// ============================================================
// SAS READ DTC — SID 0x18 (KWP), ID 0x622/0x484
// ============================================================
String readSASDTC() {
  vTaskSuspend(hTaskCAN);
  if (hTaskTP != NULL) vTaskSuspend(hTaskTP);
  vTaskDelay(pdMS_TO_TICKS(30));
  vTaskDelay(pdMS_TO_TICKS(50));

  uint8_t req[4]  = {0x18, 0x00, 0xFF, 0x00};
  uint8_t resp[64];
  uint16_t respLen;
  String result = "Timeout";

  if (isotp_request(SAS_REQ_ID, SAS_RESP_ID,
                    req, 4, resp, &respLen, ISOTP_TIMEOUT_MS)) {
    result = parseDTCPayload(resp, respLen);
  }

  Serial.printf("[DTC SAS] %s\n", result.c_str());
  if (hTaskTP != NULL) vTaskResume(hTaskTP);
  vTaskResume(hTaskCAN);
  return result;
}

// ============================================================
// SAS CLEAR DTC — SID 0x14
// ============================================================
bool clearSASDTC() {
  vTaskSuspend(hTaskCAN);
  if (hTaskTP != NULL) vTaskSuspend(hTaskTP);
  vTaskDelay(pdMS_TO_TICKS(30));
  vTaskDelay(pdMS_TO_TICKS(50));

  uint8_t req[3]  = {0x14, 0xFF, 0x00};
  uint8_t resp[16];
  uint16_t respLen;
  bool success = false;

  if (isotp_request(SAS_REQ_ID, SAS_RESP_ID,
                    req, 3, resp, &respLen, ISOTP_TIMEOUT_MS)) {
    success = (respLen >= 1 && resp[0] == 0x54);
  }

  Serial.printf("[DTC SAS] Clear %s\n", success ? "OK" : "FAIL");
  if (hTaskTP != NULL) vTaskResume(hTaskTP);
  vTaskResume(hTaskCAN);
  return success;
}

// ============================================================
// LEGACY WRAPPERS (nextion_pages.ino gọi)
// ============================================================
String readDTC(uint8_t mode) { return readECMDTC(); }
void   clearDTC()             { clearECMDTC(); }
void   printDTC() {
  Serial.println("\n======= MÃ LỖI ECM =======");
  Serial.println(readECMDTC());
  Serial.println("===========================");
}
