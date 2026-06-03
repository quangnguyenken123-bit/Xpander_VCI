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
extern bool isotp_request_wait_sid(uint16_t tx_id, uint16_t rx_id,
                                   const uint8_t* req, uint8_t reqLen,
                                   uint8_t expectedSid, uint8_t originalSid,
                                   uint8_t* resp, uint16_t* respLen,
                                   uint32_t timeoutMs);
extern void canFlushBuffer();
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
  if (resp[0] != 0x58) {
    Serial.printf("[DTC] Bad: resp[0]=0x%02X resp[1]=0x%02X len=%d\n",
                  resp[0], resp[1], len);
    return "Bad Response";
  }
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

String parseECMMode03DTCPayload(uint8_t* resp, uint16_t len) {
  if (len < 1) return "Timeout";

  if (resp[0] != 0x43) {
    Serial.printf("[DTC ECM] Bad SID: resp[0]=0x%02X len=%d\n", resp[0], len);
    return "Bad Response";
  }

  String result = "";

  for (uint16_t i = 1; i + 1 < len; i += 2) {
    if (resp[i] == 0x00 && resp[i + 1] == 0x00) continue;

    String dtc = decodeDTC(resp[i], resp[i + 1]);
    if (dtc.length() > 0) {
      if (result.length() > 0) result += "\r\n";
      result += dtc;
      Serial.printf("[DTC ECM] Parsed: %s\n", dtc.c_str());
    }
  }

  if (result.length() == 0) {
    Serial.println("[DTC ECM] No Error");
    return "No Error";
  }
  return result;
}

// ============================================================
// ECM READ DTC — OBD-II Mode 03
// Request:  01 03 00 00 00 00 00 00
// ============================================================
String readECMDTC() {
  Serial.println("[DTC ECM] Start Mode 03 Read DTC");

  vTaskSuspend(hTaskCAN);
  if (hTaskTP != NULL) vTaskSuspend(hTaskTP);
  vTaskDelay(pdMS_TO_TICKS(100));
  canFlushBuffer();

  uint8_t req[1] = {0x03};
  uint8_t resp[64];
  uint16_t respLen = 0;
  String result = "Timeout";

  if (isotp_request_wait_sid(CAN_ID_ECM_REQ, CAN_ID_ECM_RESP,
                             req, 1, 0x43, 0x03,
                             resp, &respLen, ISOTP_TIMEOUT_MS)) {
    result = parseECMMode03DTCPayload(resp, respLen);
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
