// ============================================================
// File: isotp.ino
// Mô tả: ISO-TP transport layer (ISO 15765-2)
//        Handle FF/CF/FC + NRC 0x78 retry
// ============================================================
#include "config.h"

extern bool canSendRaw(uint32_t id, unsigned char* buf, uint8_t len);

// Gửi Flow Control sau khi nhận First Frame
static void isotp_send_fc(uint16_t tx_id) {
  unsigned char fc[8] = {
    0x30, ISOTP_FLOW_CONTROL_BS, ISOTP_FLOW_CONTROL_ST,
    0, 0, 0, 0, 0
  };
  canSendRaw(tx_id, fc, 8);
}

// Gửi Single Frame (payload ≤ 7 bytes)
bool isotp_send(uint16_t tx_id, const uint8_t* data, uint8_t len) {
  if (len > 7) return false;
  unsigned char buf[8] = {0};
  buf[0] = len & 0x0F;
  memcpy(&buf[1], data, len);
  return canSendRaw(tx_id, buf, 8);
}

// Nhận ISO-TP message (tự xử lý FF/CF reassembly, NRC 0x78)
bool isotp_receive(uint16_t tx_id, uint16_t rx_id,
                   uint8_t* buf, uint16_t* outLen, uint32_t timeoutMs) {
  unsigned long start = millis();
  uint16_t totalLen = 0, received = 0;
  uint8_t  expectSeq = 1, pendingCount = 0;
  bool     gotFF = false;
  *outLen = 0;

  while (millis() - start < timeoutMs) {
    if (!digitalRead(CAN_INT_PIN)) {
      long unsigned int rxId;
      unsigned char len = 0, rxBuf[8];
      CAN.readMsgBuf(&rxId, &len, rxBuf);
      if (rxId != rx_id) continue;

      uint8_t pci = rxBuf[0] & 0xF0;

      // Single Frame (bao gồm NRC)
      if (pci == 0x00) {
        uint8_t sfLen = rxBuf[0] & 0x0F;
        if (sfLen >= 3 && rxBuf[1] == 0x7F && rxBuf[3] == 0x78) {
          pendingCount++;
          Serial.printf("[ISOTP] NRC 0x78 pending %d/%d\n",
                        pendingCount, ISOTP_MAX_PENDING_RETRY);
          if (pendingCount >= ISOTP_MAX_PENDING_RETRY) return false;
          start = millis();
          continue;
        }
        if (sfLen >= 3 && rxBuf[1] == 0x7F) {
          Serial.printf("[ISOTP] NRC SID=%02X code=%02X\n", rxBuf[2], rxBuf[3]);
          return false;
        }
        memcpy(buf, &rxBuf[1], sfLen);
        *outLen = sfLen;
        return true;
      }

      // First Frame
      else if (pci == 0x10) {
        totalLen = ((rxBuf[0] & 0x0F) << 8) | rxBuf[1];
        if (totalLen == 0 || totalLen > 256) return false;
        uint8_t copy = (totalLen < 6) ? totalLen : 6;
        memcpy(buf, &rxBuf[2], copy);
        received = copy;
        expectSeq = 1;
        gotFF = true;
        isotp_send_fc(tx_id);
        Serial.printf("[ISOTP] FF: total=%d bytes, sent FC\n", totalLen);
      }

      // Consecutive Frame
      else if (pci == 0x20 && gotFF) {
        uint8_t seq = rxBuf[0] & 0x0F;
        if (seq != (expectSeq & 0x0F)) {
          Serial.printf("[ISOTP] CF seq error: %d != %d\n", seq, expectSeq & 0x0F);
          return false;
        }
        uint16_t remaining = totalLen - received;
        uint8_t  toCopy    = (remaining > 7) ? 7 : (uint8_t)remaining;
        memcpy(&buf[received], &rxBuf[1], toCopy);
        received += toCopy;
        expectSeq++;
        if (received >= totalLen) {
          *outLen = totalLen;
          return true;
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }

  Serial.println("[ISOTP] RX Timeout");
  return false;
}

// Request + Response trong 1 lần gọi
bool isotp_request(uint16_t tx_id, uint16_t rx_id,
                   const uint8_t* req, uint8_t reqLen,
                   uint8_t* resp, uint16_t* respLen, uint32_t timeoutMs) {
  if (!isotp_send(tx_id, req, reqLen)) return false;
  return isotp_receive(tx_id, rx_id, resp, respLen, timeoutMs);
}

static int isotp_check_expected_sid(uint8_t expectedSid, uint8_t originalSid,
                                    uint8_t* resp, uint16_t respLen) {
  if (respLen == 0) return 0;

  if (resp[0] == expectedSid) {
    Serial.printf("[DTC ECM] Positive response 0x%02X len=%d\n",
                  expectedSid, respLen);
    return 1;
  }

  if (respLen >= 3 && resp[0] == 0x7F && resp[1] == originalSid) {
    Serial.printf("[DTC ECM] Negative response NRC=0x%02X\n", resp[2]);
    if (resp[2] == 0x78) return 0;
    return -1;
  }

  uint8_t second = (respLen > 1) ? resp[1] : 0;
  Serial.printf("[DTC ECM] Ignore unrelated SID=0x%02X second=0x%02X len=%d\n",
                resp[0], second, respLen);
  return 0;
}

bool isotp_request_wait_sid(uint16_t tx_id, uint16_t rx_id,
                            const uint8_t* req, uint8_t reqLen,
                            uint8_t expectedSid, uint8_t originalSid,
                            uint8_t* resp, uint16_t* respLen,
                            uint32_t timeoutMs) {
  Serial.print("[DTC ECM] Send request:");
  for (uint8_t i = 0; i < reqLen; i++) {
    Serial.printf(" %02X", req[i]);
  }
  Serial.println();

  if (!isotp_send(tx_id, req, reqLen)) return false;

  unsigned long start = millis();
  uint16_t totalLen = 0, received = 0;
  uint8_t expectSeq = 1;
  bool gotFF = false;
  *respLen = 0;

  while (millis() - start < timeoutMs) {
    if (CAN_MSGAVAIL == CAN.checkReceive()) {
      long unsigned int rxId;
      unsigned char len = 0, rxBuf[8];
      CAN.readMsgBuf(&rxId, &len, rxBuf);
      if (rxId != rx_id) continue;

      uint8_t pci = rxBuf[0] & 0xF0;

      if (pci == 0x00) {
        uint8_t sfLen = rxBuf[0] & 0x0F;
        memcpy(resp, &rxBuf[1], sfLen);
        *respLen = sfLen;

        int check = isotp_check_expected_sid(expectedSid, originalSid, resp, *respLen);
        if (check == 1) return true;
        if (check < 0) return false;
      } else if (pci == 0x10) {
        totalLen = ((rxBuf[0] & 0x0F) << 8) | rxBuf[1];
        if (totalLen == 0 || totalLen > 64) return false;
        uint8_t copy = (totalLen < 6) ? totalLen : 6;
        memcpy(resp, &rxBuf[2], copy);
        received = copy;
        expectSeq = 1;
        gotFF = true;
        isotp_send_fc(tx_id);
      } else if (pci == 0x20 && gotFF) {
        uint8_t seq = rxBuf[0] & 0x0F;
        if (seq != (expectSeq & 0x0F)) {
          Serial.printf("[ISOTP] CF seq error: %d != %d\n", seq, expectSeq & 0x0F);
          return false;
        }

        uint16_t remaining = totalLen - received;
        uint8_t toCopy = (remaining > 7) ? 7 : (uint8_t)remaining;
        memcpy(&resp[received], &rxBuf[1], toCopy);
        received += toCopy;
        expectSeq++;

        if (received >= totalLen) {
          *respLen = totalLen;
          gotFF = false;

          int check = isotp_check_expected_sid(expectedSid, originalSid, resp, *respLen);
          if (check == 1) return true;
          if (check < 0) return false;
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }

  Serial.printf("[DTC ECM] Timeout waiting for 0x%02X\n", expectedSid);
  return false;
}
