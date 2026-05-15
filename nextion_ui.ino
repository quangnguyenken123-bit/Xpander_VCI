// ============================================================
// File: nextion_ui.ino
// Mô tả: Low-level UART communication với Nextion
// ============================================================
#include "config.h"
#include "vehicle_data.h"

// === Trạng thái Nextion ===
volatile int  currentPage      = 0;
volatile int  scrollOffset     = 0;
volatile bool nextionConnected = false;

// === Buffer RX ===
#define NEXTION_RX_BUF_SIZE 64
char nxRxBuf[NEXTION_RX_BUF_SIZE];
uint8_t nxRxIdx = 0;
unsigned long nxLastByteTime = 0;

// === Flag yêu cầu từ Nextion ===
volatile bool flagReadDTC       = false;
volatile bool flagClearDTC      = false;
volatile bool flagResetConn     = false;
volatile bool flagScrollChanged = false;

// ============================================================
// GỬI COMMAND ĐẾN NEXTION (tự thêm 3 byte 0xFF kết thúc)
// ============================================================
void nxSendCmd(const String& cmd) {
  NEXTION_SERIAL.print(cmd);
  NEXTION_SERIAL.write(0xFF);
  NEXTION_SERIAL.write(0xFF);
  NEXTION_SERIAL.write(0xFF);
}

// ============================================================
// HELPER: Set text cho component (xử lý cả page name có space)
// ============================================================
void nxSetText(const String& pageObj, const String& compName, const String& value) {
  String cmd;
  if (pageObj.indexOf(' ') >= 0) {
    cmd = "\"" + pageObj + "\"." + compName + ".txt=\"" + value + "\"";
  } else {
    cmd = pageObj + "." + compName + ".txt=\"" + value + "\"";
  }
  nxSendCmd(cmd);
}

void nxSetVal(const String& pageObj, const String& compName, int value) {
  String cmd;
  if (pageObj.indexOf(' ') >= 0) {
    cmd = "\"" + pageObj + "\"." + compName + ".val=" + String(value);
  } else {
    cmd = pageObj + "." + compName + ".val=" + String(value);
  }
  nxSendCmd(cmd);
}

// ============================================================
// KHỞI TẠO NEXTION
// ============================================================
void setupNextion() {
  NEXTION_SERIAL.begin(NEXTION_BAUDRATE, SERIAL_8N1,
                       NEXTION_RX_PIN, NEXTION_TX_PIN);
  delay(200);
  nxSendCmd("page page0");
  delay(50);
  Serial.println("[NX] Nextion UART2 khoi tao xong");
  nextionConnected = true;
}

// ============================================================
// PARSE MESSAGE TỪ NEXTION
// ============================================================
void nxProcessMessage() {
  if (nxRxIdx == 0) return;

  // Debug raw bytes
  Serial.printf("[NX RX raw] %d bytes: ", nxRxIdx);
  for (int i = 0; i < nxRxIdx; i++) Serial.printf("%02X ", (uint8_t)nxRxBuf[i]);
  Serial.println();

  // === "scroll:" + 4 byte binary little-endian ===
  if (nxRxIdx >= 11 && memcmp(nxRxBuf, "scroll:", 7) == 0) {
    uint8_t b0 = (uint8_t)nxRxBuf[7];
    uint8_t b1 = (uint8_t)nxRxBuf[8];
    uint8_t b2 = (uint8_t)nxRxBuf[9];
    uint8_t b3 = (uint8_t)nxRxBuf[10];
    uint32_t val = b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
    if (val > 20) val = 20;
    scrollOffset = val;
    flagScrollChanged = true;
    currentPage = 11;
    Serial.printf("[NX] Scroll offset = %d\n", scrollOffset);
    nxRxIdx = 0;
    return;
  }

  // Null-terminate để dùng String cho các lệnh khác
  nxRxBuf[nxRxIdx] = '\0';
  String msg(nxRxBuf);
  msg.trim();
  Serial.printf("[NX RX] '%s'\n", msg.c_str());

  // === "p:XX" → page change notification ===
  if (msg.startsWith("p:")) {
    int newPage = msg.substring(2).toInt();
    currentPage = newPage;
    Serial.printf("[NX] User dang xem page %d\n", currentPage);
  }
  // === "read_dtc" ===
  else if (msg.indexOf("read_dtc") >= 0) {
    flagReadDTC = true;
    currentPage = 12;
    Serial.println("[NX] Yeu cau: Read DTC");
  }
  // === "clear_dtc" ===
  else if (msg.indexOf("clear_dtc") >= 0) {
    flagClearDTC = true;
    currentPage = 12;
    Serial.println("[NX] Yeu cau: Clear DTC");
  }
  // === "reset_conn" ===
  else if (msg.indexOf("reset_conn") >= 0) {
    flagResetConn = true;
    currentPage = 11;
    Serial.println("[NX] Yeu cau: Reset Connection");
  }

  nxRxIdx = 0;
}

// ============================================================
// TASK: Đọc UART từ Nextion liên tục
// ============================================================
void taskNextionRX(void* pvParameters) {
  for (;;) {
    while (NEXTION_SERIAL.available()) {
      char c = NEXTION_SERIAL.read();
      nxLastByteTime = millis();

      if (c == 0x0A) {
        nxProcessMessage();
      } else if (nxRxIdx < NEXTION_RX_BUF_SIZE - 1) {
        nxRxBuf[nxRxIdx++] = c;
      }
    }

    if (nxRxIdx > 0 && (millis() - nxLastByteTime > 50)) {
      nxProcessMessage();
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}