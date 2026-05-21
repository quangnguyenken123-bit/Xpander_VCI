# PROJECT STATUS — Xpander_VCI
# Cập nhật: 22/05/2026

## ✅ HOÀN THÀNH (compile OK)

### Core
- Live Data 30 PID realtime
- ECM Module Info (VIN đọc từ xe, fallback CACHED_VIN)
- SAS Module Info (Part Number LID 0x87, fallback "B600A732")
- Nextion UART 115200 baud
- ISO-TP + KWP2000 transport layer
- VIN đọc từ xe, fallback offline

### Diagnostics
- ECM Read/Clear DTC (KWP2000 SID 0x18/0x14)
- SAS Read/Clear DTC (KWP2000 SID 0x18/0x14)
- Smart routing: page 12→ECM, page 14→SAS
- DTC hiển thị xuống dòng "\r\n"
- DTC description từ SD card database

### SD Card
- SD_CS_PIN=13, dùng chung SPI với MCP2515
- database.csv: 104 mã lỗi P/C/B/U
- lookupDTC(): tra description khi đọc DTC
- searchDTCByCategory(): lọc 10 mã theo P/C/B/U

### Actuator Test
- ECM Injector Cut-off (SID 0x30)
- Poll 150ms, safety RPM>600, abort RPM<400
- 4 injectors (b0-b3)

### Steering
- EPS Steering Angle 10Hz
- Gauge z0 + Text t0 trên page monitor(sas)

### Stability Fixes
- hTaskTP suspend: fix Bad Response DTC/Actuator
- UART TX buffer 2048
- updateLiveDataPage dùng snprintf
- MCP2515 Bus-Off auto-recovery mỗi 5s
- Stack tăng: Steering/Actuator=6144, NextionTX=12288
- Heap log mỗi 30s

### UX
- Smart back button About (vaPageBack.val)
- About uptime MM:SS (millis)
- PostInitialize Events đủ tất cả pages
- Page 13 (dtc search): browse P/C/B/U, 10 mã/category

## ⚠️ CHƯA TEST TRÊN XE (lần này)

- DTC read/clear ECM (cần tạo lỗi: rút connector injector)
- DTC read/clear SAS (cần tạo lỗi: rút connector SAS)
- SD card + database.csv hoạt động
- DTC description hiển thị đúng
- Page 13 dtc search
- Mode 21 LID 0x1D/0x1E (ECU có trả không?)
- Stability >10 phút sau fix baud 115200

## 📋 CHECKLIST TEST XE NGÀY MAI

### Trước khi ra xe
- [ ] Nextion: thêm bauds=115200 vào Program.s → compile → flash SD
- [ ] SD card: copy database.csv vào thư mục gốc
- [ ] Arduino IDE: compile + upload ESP32

### Test offline (không cần xe)
- [ ] Module Info ECM: VIN hiện không
- [ ] About page: uptime đếm không
- [ ] Page 13: bấm P/C/B/U xem 10 mã hiện không
- [ ] Serial Monitor: "[SD] Loaded X DTC records"

### Test trên xe
- [ ] Live Data: 30 PIDs realtime, chạy >10 phút
- [ ] Serial Monitor: theo dõi [HEAP] log
- [ ] Module Info SAS: Part Number đúng không
- [ ] Steering Angle: xoay vô lăng xem kim la bàn
- [ ] Actuator Test: rút 1 injector → bấm test → động cơ rung
- [ ] ECM DTC: rút connector injector → Read DTC → thấy P02xx
- [ ] ECM Clear DTC: bấm Clear → Read lại → No Error
- [ ] SAS DTC: rút connector SAS → Read DTC → thấy Cxxxx
- [ ] DTC description: mã lỗi có kèm mô tả không

### Nếu vẫn crash >10 phút
- [ ] Chụp Serial Monitor → gửi Claude phân tích

## 🔧 DEFERRED
- Page 13: DTC status (Stored/Pending/Permanent)
- About page: QR code URL (chờ poster)
- Page dtc search: keyboard input (Nextion basic không hỗ trợ)
- SD logger: ghi log dữ liệu

## ⚠️ KNOWN ISSUES
- SAS LID 0x9C trả NRC 0x12 → chỉ dùng LID 0x87
- Mode 21 (0x1D, 0x1E): chưa biết ECU có trả không
- Page name có space → dùng bare command
- DTC injector codes cần verify mai: P0261/P0264/P0267/P0270
