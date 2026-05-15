// ============================================================
// File: can_handler.ino
// Mô tả: Khởi tạo MCP2515, gửi/nhận CAN frame
// ============================================================
#include <mcp_can.h>
#include <SPI.h>
#include "config.h"
#include "vehicle_data.h"

MCP_CAN CAN(CAN_CS_PIN);

// ============================================================
// KHỞI TẠO CAN BUS
// ============================================================
void setupCAN() {
  Serial.println("[CAN] Dang khoi tao MCP2515...");

  uint8_t retry = 0;
  while (CAN.begin(MCP_STDEXT, CAN_BAUDRATE, CAN_OSC_FREQ) != CAN_OK) {
    retry++;
    Serial.printf("[CAN] Init that bai! Lan thu %d/5\n", retry);
    if (retry >= 5) {
      Serial.println("[CAN] LỖII NGHIÊM TRỌNG: Không thể khởi tạo MCP2515!");
      Serial.println("[CAN] Kiểm tra: dây SPI, CS pin, nguồn 3.3V, thạch anh 8MHz");
      while (1) vTaskDelay(pdMS_TO_TICKS(1000)); // Dừng hệ thống
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }

  // --- CÀI FILTER: Chỉ nhận 7E8 (ECM) và 7E9 (TCM) ---
// --- CÀI FILTER: Chỉ nhận 7E8 (ECM), 7E9 (TCM), 484 (SAS) và 797 (EPS) ---
  CAN.init_Mask(0, 0, 0x07FF0000);
  CAN.init_Mask(1, 0, 0x07FF0000);
  
  CAN.init_Filt(0, 0, 0x07E80000); // ECM response
  CAN.init_Filt(1, 0, 0x07E90000); // TCM response
  CAN.init_Filt(2, 0, 0x04840000); // SAS response <-- Dòng Claude kêu sửa
  CAN.init_Filt(3, 0, 0x07970000); // EPS response <-- Bổ sung để đọc góc lái
  
  CAN.init_Filt(4, 0, 0x07E80000); // Dư thì cứ để 7E8
  CAN.init_Filt(5, 0, 0x07E80000); // Dư thì cứ để 7E8

  CAN.setMode(MCP_NORMAL);
  pinMode(CAN_INT_PIN, INPUT);

  Serial.println("[CAN] Khoi tao OK! Filter: 0x7E8 + 0x7E9");
}

// ============================================================
// GỬI REQUEST
// ============================================================

// Mode 01 - OBD2 chuẩn
bool canSendMode01(uint8_t pid) {
  unsigned char data[8] = {0x02, 0x01, pid, 0, 0, 0, 0, 0};
  return CAN.sendMsgBuf(CAN_ID_BROADCAST, 0, 8, data) == CAN_OK;
}

// Mode 21 - Mitsubishi specific
bool canSendMode21(uint8_t lid) {
  unsigned char data[8] = {0x02, 0x21, lid, 0, 0, 0, 0, 0};
  return CAN.sendMsgBuf(CAN_ID_ECM_REQ, 0, 8, data) == CAN_OK;
}

// Mode 09 - Vehicle info (VIN, calibration)
bool canSendMode09(uint8_t pid) {
  unsigned char data[8] = {0x02, 0x09, pid, 0, 0, 0, 0, 0};
  return CAN.sendMsgBuf(CAN_ID_ECM_REQ, 0, 8, data) == CAN_OK;
}

// Gửi frame tùy ý (dùng cho DTC, UDS)
bool canSendRaw(uint32_t id, unsigned char* buf, uint8_t len) {
  return CAN.sendMsgBuf(id, 0, len, buf) == CAN_OK;
}

// ============================================================
// NHẬN FRAME CÓ TIMEOUT
// Trả về true nếu nhận được frame hợp lệ từ ECM/TCM
// ============================================================
bool canReceive(long unsigned int* rxId, unsigned char* len,
                unsigned char* rxBuf, uint32_t timeoutMs) {
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    if (!digitalRead(CAN_INT_PIN)) {
      CAN.readMsgBuf(rxId, len, rxBuf);
      // Chỉ chấp nhận phản hồi từ ECM hoặc TCM
      if (*rxId == CAN_ID_ECM_RESP || *rxId == CAN_ID_TCM_RESP) {
        return true;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1)); // Nhường CPU 1ms mỗi vòng
  }
  return false; // Timeout
}

// ============================================================
// XẢ BUFFER CAN (Dùng trước khi gửi lệnh quan trọng)
// ============================================================
void canFlushBuffer() {
  long unsigned int id;
  unsigned char len = 0, buf[8];
  uint8_t count = 0;
  while (!digitalRead(CAN_INT_PIN) && count < 10) {
    CAN.readMsgBuf(&id, &len, buf);
    count++;
  }
}