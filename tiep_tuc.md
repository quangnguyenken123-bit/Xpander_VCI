# TIẾP TỤC — Xpander VCI Session Handoff
# Tạo: 21/05/2026 | Đọc kỹ trước khi làm bất cứ thứ gì

---

## 1. TỔNG QUAN DỰ ÁN

**Mục tiêu**: Bộ VCI (Vehicle Communication Interface) tự chế cho Mitsubishi Xpander 2020.
Chức năng: Live Data, Module Information, Đọc/Xóa DTC, Steering Angle, Actuator Test (Injector Cut-off).

### Hardware
- **MCU**: ESP32 Dev Module
- **CAN**: MCP2515 (8MHz) — CS=GPIO5, INT=GPIO21, MOSI=23, MISO=19, SCK=18
- **Display**: Nextion NX8048P050_011 (5", 800x480) — UART2: TX=GPIO17, RX=GPIO16 @ 9600 baud
- **SD Card**: CS=GPIO13, dùng chung SPI (MOSI=23, MISO=19, SCK=18) với MCP2515
- **CAN Bus**: ISO 15765-4, 500 kbps

### Protocol Stack
```
Application:  dtc_handler.ino, steering_module.ino, actuator_test.ino
KWP2000:      kwp2000.ino  (SID 0x1A, 0x21, 0x3E)
ISO-TP:       isotp.ino    (FF/CF/FC/NRC 0x78)
CAN:          can_handler.ino (MCP2515)
```

### CAN IDs (VERIFIED trên Xpander 2020)
```
ECM: REQ=0x7E0  RESP=0x7E8
TCM: REQ=0x7E1  RESP=0x7E9
SAS: REQ=0x622  RESP=0x484  (KWP2000)
EPS: REQ=0x796  RESP=0x797  (KWP2000)
```

### GitHub
- Repo: https://github.com/quangnguyenken123-bit/Xpander_VCI
- Branch: **master** (default, không phải main)
- Git local: `D:\DATNNNN\Xpander_VCI`

---

## 2. CẤU TRÚC FILE

| File | Vai trò |
|------|---------|
| `config.h` | Pins, CAN IDs, constants. SD_CS_PIN chưa enable |
| `vehicle_data.h` | VehicleData + SteeringData + SasInfo structs + LOCK_DATA macro |
| `Xpander_VCI.ino` | setup(), loop(), taskReadCAN, taskTesterPresent, readVIN() |
| `can_handler.ino` | MCP2515 init, filter, canSendRaw, canReceive, canFlushBuffer |
| `pid_parser.ino` | parseMode01() cho 22 PIDs |
| `dtc_handler.ino` | readECMDTC/clearECMDTC/readSASDTC/clearSASDTC via SID 0x18/0x14 |
| `isotp.ino` | ISO-TP transport layer |
| `kwp2000.ino` | kwp_read_ecu_id, kwp_read_data_by_lid, kwp_send_tester_present |
| `steering_module.ino` | eps_read_steering_angle(), sas_read_module_info(), taskSteering |
| `nextion_ui.ino` | nxSendCmd, setupNextion, taskNextionRX, flag parsing |
| `nextion_pages.ino` | updateLiveDataPage, updateModuleInfoPage, updateSASInfoPage, taskNextionTX |
| `actuator_test.ino` | ecm_injector_cutoff_test(), taskActuator |
| `sd_logger.ino` | **PLACEHOLDER — chưa implement, chờ database DTC** |

---

## 3. NHỮNG GÌ ĐÃ HOÀN THÀNH

### ✅ Hoạt động tốt (đã test trên xe)
- Live Data 30 PID realtime (RPM, Speed, Coolant, v.v.)
- ECM Module Info: VIN đọc từ xe, fallback CACHED_VIN="RLA0LNNC1L1000001"
- EPS Steering Angle 10Hz — updateSteeringPage() gọi trực tiếp trong taskSteering
- Actuator Test 4 injectors (SID 0x30) — engine rung khi cắt nhiên liệu
- Navigation Nextion mượt

### ✅ Đã code, chưa test với lỗi thật
- ECM Read/Clear DTC (SID 0x18/0x14 + ISO-TP)
- SAS Read/Clear DTC (SID 0x18/0x14, ID 0x622/0x484)
- Smart DTC routing: page 12 → ECM, page 14 → SAS (phân biệt bằng currentPage)

### ✅ FreeRTOS Critical Rules (PHẢI THEO)
1. `vTaskSuspend(hTaskCAN)` TRƯỚC mọi ISO-TP request
2. `vTaskResume(hTaskCAN)` TRƯỚC MỌI câu return
3. Mọi xData access: `LOCK_DATA { ... } UNLOCK_DATA`
4. Task order trong setup(): Mutex → CAN → VIN → Nextion → taskReadCAN(&hTaskCAN) → các task khác

### ✅ Nextion
- PostInitialize Event đủ tất cả pages: `print "p:X"` + `printh 0a`
- Smart back button About page dùng `page0.vaPageBack.val`
- Page Setting (brightness/dark mode) tự xử lý trong Nextion, không cần ESP32

---

## 4. TRẠNG THÁI HIỆN TẠI — ĐANG DỪNG Ở ĐÂY

### Đang thảo luận, CHƯA gửi Codex, CHƯA có trong repo

Có **1 prompt Codex tổng hợp** đang chờ gửi. Dưới đây là toàn bộ nội dung:

---

### PROMPT CODEX (copy paste gửi ngay khi vào session mới)

```
Đọc AGENTS.md và STATUS.md.

Fix toàn bộ các vấn đề sau buổi test xe:

═══════════════════════════════════════
FIX 1: Tester Present can thiệp — Bad Response DTC
═══════════════════════════════════════

Nguyên nhân: taskTesterPresent gửi 0x3E mỗi 2s,
ECM response 0x7E8 lẫn vào DTC/Actuator/Steering.

**Xpander_VCI.ino:**
- Khai báo thêm: TaskHandle_t hTaskTP = NULL;
- Trong setup(), sửa dòng xTaskCreate taskTesterPresent:
  xTaskCreate(taskTesterPresent, "TP", 2048, NULL, 1, &hTaskTP);
- Tăng stack:
  taskSteering:   4096 → 6144
  taskActuator:   4096 → 6144
  taskNextionTX:  8192 → 12288

**dtc_handler.ino:**
Thêm extern đầu file:
  extern TaskHandle_t hTaskTP;

Trong readECMDTC(), readSASDTC(), clearECMDTC(), clearSASDTC():
  SAU vTaskSuspend(hTaskCAN), THÊM:
    if (hTaskTP != NULL) vTaskSuspend(hTaskTP);
    vTaskDelay(pdMS_TO_TICKS(30));
  TRƯỚC mỗi vTaskResume(hTaskCAN), THÊM:
    if (hTaskTP != NULL) vTaskResume(hTaskTP);

**actuator_test.ino:**
Thêm extern: extern TaskHandle_t hTaskTP;
Trong ecm_injector_cutoff_test():
  SAU vTaskSuspend(hTaskCAN), THÊM:
    if (hTaskTP != NULL) vTaskSuspend(hTaskTP);
    vTaskDelay(pdMS_TO_TICKS(30));
  TRƯỚC mỗi vTaskResume(hTaskCAN), THÊM:
    if (hTaskTP != NULL) vTaskResume(hTaskTP);

**steering_module.ino:**
Thêm extern: extern TaskHandle_t hTaskTP;
Trong eps_read_steering_angle() và sas_read_module_info():
  SAU vTaskSuspend(hTaskCAN), THÊM:
    if (hTaskTP != NULL) vTaskSuspend(hTaskTP);
    vTaskDelay(pdMS_TO_TICKS(30));
  TRƯỚC mỗi vTaskResume(hTaskCAN), THÊM:
    if (hTaskTP != NULL) vTaskResume(hTaskTP);

═══════════════════════════════════════
FIX 2: Crash sau 10 phút
═══════════════════════════════════════

**nextion_ui.ino — setupNextion():**
Sau NEXTION_SERIAL.begin(...), thêm:
  NEXTION_SERIAL.setTxBufferSize(2048);
  delay(100);

**nextion_pages.ino — updateLiveDataPage():**
Thay toàn bộ hàm bằng version dùng snprintf:

void updateLiveDataPage() {
  char cmd[80];
  for (int i = 0; i < 10; i++) {
    int pidIdx = scrollOffset + i;
    if (pidIdx >= 30) pidIdx = 29;
    snprintf(cmd, sizeof(cmd), "tname_%d.txt=\"%s\"",
             i, PID_NAMES[pidIdx]);
    nxSendCmd(String(cmd));
    String val = getPIDValueString(pidIdx);
    snprintf(cmd, sizeof(cmd), "t%d.txt=\"%s\"",
             i, val.c_str());
    nxSendCmd(String(cmd));
    snprintf(cmd, sizeof(cmd), "t%d.txt=\"%s\"",
             i + 10, PID_UNITS[pidIdx]);
    nxSendCmd(String(cmd));
  }
}

**nextion_pages.ino — taskNextionTX:**
  vTaskDelay(pdMS_TO_TICKS(800)); → vTaskDelay(pdMS_TO_TICKS(1000));

**Xpander_VCI.ino — taskPrintSerial:**
Trong vòng for(;;), thêm:
  static uint32_t lastHeapLog = 0;
  if (millis() - lastHeapLog > 30000) {
    Serial.printf("[HEAP] Free=%u Min=%u\n",
      ESP.getFreeHeap(), ESP.getMinFreeHeap());
    lastHeapLog = millis();
  }

═══════════════════════════════════════
FIX 3: DTC separator xuống dòng
═══════════════════════════════════════

**dtc_handler.ino — parseDTCPayload():**
  if (result != "") result += ", ";
→ if (result != "") result += "\r\n";

═══════════════════════════════════════
FIX 4: SAS Module Info — component names mới
═══════════════════════════════════════

Page i4(sas) đã redesign với components mới:
t_ecu, t_part, t_hard, t_soft, t_diag

**nextion_pages.ino — updateSASInfoPage():**
Thay toàn bộ hàm:

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

═══════════════════════════════════════
FIX 5: config.h — enable SD card
═══════════════════════════════════════

Trong config.h, sửa 2 dòng SD:
  // #define SD_CS_PIN   xx   → #define SD_CS_PIN  13
  #define SD_ENABLED  false  → #define SD_ENABLED true
  // Ghi chú: SD dùng chung SPI với MCP2515 (MOSI=23, MISO=19, SCK=18)
```

---

### Sau khi Codex apply
```bash
git add .
git commit -m "Fix: TP interference + 10min stability + DTC format + SAS info + SD pin"
git push
```

---

## 5. CÁC BƯỚC TIẾP THEO (theo thứ tự)

### Ngay lập tức
1. **Gửi prompt Codex** (section 4) → apply → compile → push
2. **Chờ database DTC từ bạn user** (chưa biết format file)
3. Khi có database → implement sd_logger.ino:
   - Đọc CSV từ SD vào `dtcDatabase[]` khi boot
   - Hàm `lookupDTC(String code)` → trả về description
   - Sửa `decodeDTC()` trong dtc_handler.ino để ghép code + description
   - Kết quả: "P0201: Inj 1 Circuit\r\nP0113: IAT Sensor High"

### Test cuối trước khi nộp báo cáo
4. Test ECM DTC: tháo connector injector → Read DTC → Clear DTC
5. Test SAS DTC: rút connector SAS hoặc nhờ thợ
6. Monitor Serial 30+ phút kiểm tra [HEAP] log

### Deferred (không bắt buộc cho báo cáo)
7. Page dtc search (page 13): user gõ mã → tra CSV → hiện description
8. About page QR code: chờ có poster URL

---

## 6. QUY TẮC & LƯU Ý ĐẶC BIỆT

### Workflow cố định
```
Thảo luận kỹ thuật → Claude
Sửa file trực tiếp → Codex (gửi prompt rõ ràng)
Sau Codex: git add . → git commit → git push
Session mới Claude: "Clone repo + đọc AGENTS.md + STATUS.md"
```

### Coding rules đã thống nhất
- **KHÔNG dùng cross-page command** cho page có space/ngoặc trong tên
  → Dùng bare command (vì hàm chỉ gọi khi user đang ở page đó)
- **hTaskTP pattern**: suspend TP + flush buffer + delay 30ms trước ISO-TP
- **Fallback values**: nếu đọc xe fail → hiện giá trị hardcode (không để trống)
- **DTC separator**: `"\r\n"` (không phải `", "`) để hiển thị mỗi mã 1 dòng
- **Part Number SAS fallback**: `"B600A732"` (chữ B, không phải 8)

### Nextion rules
- Page name có space → KHÔNG dùng `pagename.component.txt=` cross-page
- DTC t0 component: txt_maxl=200, isbr=True (đã verify cả page 12 và 14)
- vaPageBack.val mapping cho About smart-back:
  3=setting, 4=diag, 5=i4, 6=act test, 7=i4(sas), 8=diag(saas)

### SAS Module Info fields (verified từ Launch)
```
t_ecu  = "04 / 9C / 00"     (ECU type / Supplier / ECU Confirm)
t_part = từ LID 0x87 decode, fallback "B600A732"
t_hard = "030100"            (Hardware Version)
t_soft = "0100"              (Software Version)
t_diag = "80"                (ECU Diagnostic Version)
```

### CAN Filter trong can_handler.ino (ĐỪNG SỬA)
```cpp
CAN.init_Mask(0, 0, 0x07FF0000);
CAN.init_Mask(1, 0, 0x07FF0000);
CAN.init_Filt(0, 0, 0x07E80000); // ECM 0x7E8
CAN.init_Filt(1, 0, 0x07E90000); // TCM 0x7E9
CAN.init_Filt(2, 0, 0x04840000); // SAS 0x484
CAN.init_Filt(3, 0, 0x07970000); // EPS 0x797
CAN.init_Filt(4, 0, 0x07E80000);
CAN.init_Filt(5, 0, 0x07E80000);
```

### Nextion Page ID Map (đủ 15 pages)
| ID | Name | Chức năng |
|----|------|-----------|
| 0 | page0 | Home |
| 1 | sas | SAS menu |
| 2 | ecm | ECM menu |
| 3 | setting | Brightness/Dark (Nextion tự xử lý) |
| 4 | diag | ECM diagnostic |
| 5 | i4 | ECM Module Info |
| 6 | act test | Actuator Test |
| 7 | i4 (sas) | SAS Module Info |
| 8 | diag (saas) | SAS diagnostic |
| 9 | monitor(sas) | Steering gauge |
| 10 | about | About + uptime |
| 11 | live data | 30 PIDs |
| 12 | dtc | ECM DTC |
| 13 | dtc search | DTC lookup (deferred) |
| 14 | dtc (sas) | SAS DTC |

---

*Session handoff tạo tự động — đọc kỹ section 4 trước khi làm bất cứ thứ gì*
