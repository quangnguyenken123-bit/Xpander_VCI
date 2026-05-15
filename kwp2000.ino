// ============================================================
// File: kwp2000.ino
// Mô tả: KWP2000 service commands (SID 0x1A, 0x21, 0x3E)
// ============================================================
#include "config.h"

extern bool isotp_request(uint16_t tx_id, uint16_t rx_id,
                          const uint8_t* req, uint8_t reqLen,
                          uint8_t* resp, uint16_t* respLen, uint32_t timeoutMs);
extern bool canSendRaw(uint32_t id, unsigned char* buf, uint8_t len);

// SID 0x1A — ReadECUIdentification (dùng cho SAS module info sau này)
bool kwp_read_ecu_id(uint16_t tx_id, uint16_t rx_id,
                     uint8_t lid, uint8_t* buf, uint16_t* len) {
  uint8_t req[2] = {0x1A, lid};
  return isotp_request(tx_id, rx_id, req, 2, buf, len, ISOTP_TIMEOUT_MS);
}

// SID 0x21 — ReadDataByLocalIdentifier (dùng cho EPS steering)
bool kwp_read_data_by_lid(uint16_t tx_id, uint16_t rx_id,
                          uint8_t lid, uint8_t* buf, uint16_t* len) {
  uint8_t req[2] = {0x21, lid};
  return isotp_request(tx_id, rx_id, req, 2, buf, len, ISOTP_TIMEOUT_MS);
}

// SID 0x3E — TesterPresent (broadcast, không cần chờ response)
void kwp_send_tester_present() {
  unsigned char data[8] = {0x02, 0x3E, 0x02, 0, 0, 0, 0, 0};
  canSendRaw(0x7DF, data, 8);
}