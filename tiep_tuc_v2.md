# TIẾP TỤC v2 — Xpander VCI Session Handoff
# Tạo: 22/05/2026 | ĐỌC KỸ TRƯỚC KHI LÀM BẤT CỨ THỨ GÌ

---

## 1. TỔNG QUAN DỰ ÁN

**Mục tiêu**: Bộ VCI tự chế cho Mitsubishi Xpander 2020.
Chức năng: Live Data (30 PIDs), Module Info ECM+SAS, Steering Angle, Actuator Test, DTC Read/Clear ECM+SAS.

### Hardware
- ESP32 Dev Module
- MCP2515 (8MHz): CS=GPIO5, INT=GPIO21, MOSI=23, MISO=19, SCK=18
- Nextion NX8048P050_011 (5"): UART2 TX=GPIO17, RX=GPIO16
- SD Card: CS=GPIO13, SPI chung với MCP2515
- CAN: ISO 15765-4, 500kbps

### GitHub
- Repo: https://github.com/quangnguyenken123-bit/Xpander_VCI
- Branch: **master** (default)
- Local: `D:\DATNNNN\Xpander_VCI`

---

## 2. TRẠNG THÁI HIỆN TẠI

### Vừa gửi Codex 8 fixes (CHƯA BIẾT KẾT QUẢ)

Codex đang/vừa xử lý prompt 8 fixes. Khi vào session mới:
1. Hỏi user: "Codex đã apply và push chưa?"
2. Nếu chưa: dùng prompt bên dưới gửi lại
3. Nếu rồi: clone repo verify, rồi compile test

### 8 Fixes đã thảo luận (cần verify trong repo)

| Fix | File | Nội dung |
|-----|------|----------|
| 1 | Nhiều file | hTaskTP suspend — fix Bad Response DTC/Actuator |
| 2 | nextion_ui + nextion_pages + Xpander_VCI | 10-phút crash: TX buffer 2048, snprintf, heap log |
| 3 | config.h | NEXTION_BAUDRATE 9600 → 115200 |
| 4 | can_handler + Xpander_VCI | MCP2515 Bus-Off auto-recovery mỗi 5s |
| 5 | dtc_handler | DTC separator ", " → "\r\n" |
| 6 | nextion_pages | updateSASInfoPage() dùng t_ecu/t_part/t_hard/t_soft/t_diag |
| 7 | config.h | SD_CS_PIN=13, SD_ENABLED=true |
| 8 | Xpander_VCI | Mode 21 LID 0x1D/0x1E scan + RPM fallback |

---

## 3. PROMPT CODEX ĐẦY ĐỦ (dùng lại nếu cần)

```
Đọc AGENTS.md và STATUS.md trước khi bắt đầu.

=== FIX 1: hTaskTP — Tester Present can thiệp ===

**Xpander_VCI.ino:**
Khai báo thêm cạnh hTaskCAN:
  TaskHandle_t hTaskTP = NULL;
Trong setup(), sửa xTaskCreate taskTesterPresent:
  xTaskCreate(taskTesterPresent, "TP", 2048, NULL, 1, &hTaskTP);
Tăng stack:
  taskSteering  : 4096 → 6144
  taskActuator  : 4096 → 6144
  taskNextionTX : 8192 → 12288

**dtc_handler.ino:**
Thêm extern: extern TaskHandle_t hTaskTP;
Trong readECMDTC(), readSASDTC(), clearECMDTC(), clearSASDTC():
  SAU vTaskSuspend(hTaskCAN):
    if (hTaskTP != NULL) vTaskSuspend(hTaskTP);
    vTaskDelay(pdMS_TO_TICKS(30));
  TRƯỚC mỗi vTaskResume(hTaskCAN):
    if (hTaskTP != NULL) vTaskResume(hTaskTP);

**actuator_test.ino:**
Thêm extern: extern TaskHandle_t hTaskTP;
Trong ecm_injector_cutoff_test():
  SAU vTaskSuspend(hTaskCAN):
    if (hTaskTP != NULL) vTaskSuspend(hTaskTP);
    vTaskDelay(pdMS_TO_TICKS(30));
  TRƯỚC mỗi vTaskResume(hTaskCAN):
    if (hTaskTP != NULL) vTaskResume(hTaskTP);

**steering_module.ino:**
Thêm extern: extern TaskHandle_t hTaskTP;
Trong eps_read_steering_angle() và sas_read_module_info():
  SAU vTaskSuspend(hTaskCAN):
    if (hTaskTP != NULL) vTaskSuspend(hTaskTP);
    vTaskDelay(pdMS_TO_TICKS(30));
  TRƯỚC mỗi vTaskResume(hTaskCAN):
    if (hTaskTP != NULL) vTaskResume(hTaskTP);

=== FIX 2: 10 phút crash ===

**nextion_ui.ino — setupNextion():**
Sau NEXTION_SERIAL.begin(...):
  NEXTION_SERIAL.setTxBufferSize(2048);
  delay(100);

**nextion_pages.ino — updateLiveDataPage():**
Thay toàn bộ hàm:
void updateLiveDataPage() {
  char cmd[80];
  for (int i = 0; i < 10; i++) {
    int pidIdx = scrollOffset + i;
    if (pidIdx >= 30) pidIdx = 29;
    snprintf(cmd, sizeof(cmd), "tname_%d.txt=\"%s\"", i, PID_NAMES[pidIdx]);
    nxSendCmd(String(cmd));
    String val = getPIDValueString(pidIdx);
    snprintf(cmd, sizeof(cmd), "t%d.txt=\"%s\"", i, val.c_str());
    nxSendCmd(String(cmd));
    snprintf(cmd, sizeof(cmd), "t%d.txt=\"%s\"", i + 10, PID_UNITS[pidIdx]);
    nxSendCmd(String(cmd));
  }
}

**nextion_pages.ino — taskNextionTX:**
  vTaskDelay(pdMS_TO_TICKS(800)); → vTaskDelay(pdMS_TO_TICKS(1000));

**Xpander_VCI.ino — taskPrintSerial, trong for(;;):**
  static uint32_t lastHeapLog = 0;
  if (millis() - lastHeapLog > 30000) {
    Serial.printf("[HEAP] Free=%u Min=%u\n",
      ESP.getFreeHeap(), ESP.getMinFreeHeap());
    lastHeapLog = millis();
  }

=== FIX 3: Nextion baud rate ===
**config.h:**
  #define NEXTION_BAUDRATE  9600 → 115200

=== FIX 4: MCP2515 Bus-Off auto-recovery ===
**can_handler.ino — thêm hàm:**
  bool canCheckBusOff() {
    return (CAN.checkError() != CAN_OK);
  }

**Xpander_VCI.ino — taskReadCAN, trong for(;;):**
  static uint32_t lastErrorCheck = 0;
  if (millis() - lastErrorCheck > 5000) {
    lastErrorCheck = millis();
    if (canCheckBusOff()) {
      Serial.println("[CAN] Bus-Off! Auto-reset...");
      setupCAN();
    }
  }

=== FIX 5: DTC separator ===
**dtc_handler.ino — parseDTCPayload():**
  if (result != "") result += ", ";
→ if (result != "") result += "\r\n";

=== FIX 6: SAS Module Info component names mới ===
**nextion_pages.ino — updateSASInfoPage():**
void updateSASInfoPage() {
  extern SasInfo sasInfo;
  String pn = (sasInfo.valid && strlen(sasInfo.partNumber87) > 0)
              ? String(sasInfo.partNumber87) : "B600A732";
  nxSendCmd("t_ecu.txt=\"04 / 9C / 00\"");
  nxSendCmd(String("t_part.txt=\"") + pn + "\"");
  nxSendCmd("t_hard.txt=\"030100\"");
  nxSendCmd("t_soft.txt=\"0100\"");
  nxSendCmd("t_diag.txt=\"80\"");
}

=== FIX 7: SD card enable ===
**config.h:**
  // #define SD_CS_PIN  xx → #define SD_CS_PIN  13
  #define SD_ENABLED  false → #define SD_ENABLED  true

=== FIX 8: Mode 21 re-enable + RPM fallback ===
**Xpander_VCI.ino — taskReadCAN:**
Sau khai báo mode01_pids[], thêm:
  const uint8_t mode21_lids[] = {0x1D, 0x1E};
  const uint8_t total21 = 2;
  static uint8_t idx21 = 0;
  static uint8_t m01_count = 0;

Trong for(;;), sau vTaskDelay cuối:
  m01_count++;
  if (m01_count >= 10) {
    m01_count = 0;
    uint8_t lid = mode21_lids[idx21];
    idx21 = (idx21 + 1) % total21;
    if (canSendMode21(lid)) {
      if (canReceive(&rxId, &len, rxBuf, CAN_TIMEOUT_MS)) {
        if (rxBuf[1] == 0x61 && rxBuf[2] == lid) {
          parseMode21(lid, rxBuf, len);
        }
      }
    }
    LOCK_DATA {
      bool engineOn = (xData.rpm > 400);
      xData.ignitionSw     = engineOn;
      xData.fuelPumpRelay  = engineOn;
      xData.crankingSignal = (xData.rpm > 50 && xData.rpm < 400);
    }
    UNLOCK_DATA;
  }
```

---

## 4. VIỆC TIẾP THEO SAU KHI CODEX APPLY

### Ngay lập tức
```
[ ] 1. Codex apply xong → git add . → git commit → git push
[ ] 2. Compile Arduino IDE → kiểm tra không có lỗi
[ ] 3. Nextion Editor:
        - Program.s: thêm bauds=115200
        - Page i4(sas): confirm component names t_ecu/t_part/t_hard/t_soft/t_diag
        - Page 13 (dtc search): thêm Hotspot m2 lên icon kính lúp:
              Touch Press: print "search:" + print t_search.txt + printh 0a
[ ] 4. Compile Nextion → flash SD
[ ] 5. Test xe: DTC read/clear, Steering, Actuator, Live Data >10 phút
```

### Sau khi có database DTC (CSV từ nguồn OBD-II public)
```
[ ] 6. Implement sd_logger.ino:
        - SD.begin(SD_CS_PIN)
        - Đọc DTC_LIST.CSV → dtcDatabase[] khi boot
        - lookupDTC(String code) → trả description
[ ] 7. Sửa dtc_handler.ino: ghép code + description
        "P0201" + lookup → "P0201: Inj 1 Circuit"
[ ] 8. Page 13 dtc search: xử lý "search:PXXXX" command
```

---

## 5. QUY TẮC QUAN TRỌNG

### Code rules
- `vTaskSuspend(hTaskCAN)` + `vTaskSuspend(hTaskTP)` TRƯỚC mọi ISO-TP
- `vTaskResume` cả 2 TRƯỚC MỌI return
- `LOCK_DATA { } UNLOCK_DATA` cho mọi xData access
- Page có space/ngoặc → dùng bare command (KHÔNG cross-page)
- DTC fallback Part Number SAS: **"B600A732"** (chữ B, không phải 8)

### Nextion
- DTC t0: txt_maxl=200, isbr=True (đã verify page 12 và 14)
- vaPageBack.val: 3=setting, 4=diag, 5=i4, 6=act test, 7=i4(sas), 8=diag(saas)
- Baud: 115200 (sau khi fix)

### DTC Protocol
- ECM + SAS dùng **KWP2000** (SID 0x18/0x14), không phải OBD-II hay UDS
- DTC codes format SAE J2012 → tương thích database OBD-II public ✅
- Smart routing: currentPage==14 → SAS, còn lại → ECM

### SAS Module Info (verified Launch)
```
t_ecu  = "04 / 9C / 00"
t_part = decode LID 0x87, fallback "B600A732"
t_hard = "030100"
t_soft = "0100"
t_diag = "80"
```

### Nextion Page IDs
| ID | Name | Chức năng |
|----|------|-----------|
| 0 | page0 | Home |
| 1 | sas | SAS menu |
| 2 | ecm | ECM menu |
| 3 | setting | Brightness/Dark (Nextion only) |
| 4 | diag | ECM diagnostic |
| 5 | i4 | ECM Module Info |
| 6 | act test | Actuator Test |
| 7 | i4 (sas) | SAS Module Info |
| 8 | diag (saas) | SAS diagnostic |
| 9 | monitor(sas) | Steering gauge |
| 10 | about | Uptime + QR |
| 11 | live data | 30 PIDs |
| 12 | dtc | ECM DTC |
| 13 | dtc search | DTC lookup (SD) |
| 14 | dtc (sas) | SAS DTC |

---

## 6. SESSION MỚI — PASTE NGAY

```
Repo: https://github.com/quangnguyenken123-bit/Xpander_VCI
Clone repo, đọc tiep_tuc.md (hoặc tiep_tuc_v2.md nếu có),
sau đó hỏi tôi: "Codex đã apply và push 8 fixes chưa?"
rồi tiếp tục từ đó.
```
