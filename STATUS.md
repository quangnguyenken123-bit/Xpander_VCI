# PROJECT STATUS — Xpander_VCI
# Cập nhật: 16/05/2026 — 03:00

## ✅ HOÀN THÀNH (đã compile OK)

### Core
- Live Data 30 PID realtime (22 Mode01 PIDs)
- ECM Module Information (VIN + hardcoded fields, fallback CACHED_VIN)
- SAS Module Information (Part Number từ LID 0x87, fallback "8600A732")
- Nextion UART communication (9600 baud)
- ISO-TP transport layer (isotp.ino)
- KWP2000 SID 0x21 + 0x1A (kwp2000.ino)
- VIN đọc từ xe, fallback offline

### Diagnostics
- ECM Read DTC (SID 0x18 + ISO-TP) — chưa test lỗi thật
- ECM Clear DTC (SID 0x14) — chưa test
- SAS Read/Clear DTC — chưa test
- Smart DTC routing: page 12 → ECM, page 14 → SAS

### Actuator Test
- ECM Injector Cut-off Test (SID 0x30)
- Poll 150ms, safety RPM > 600, abort RPM < 400
- 4 injectors (bt0-bt3)

### Steering
- EPS Steering Angle 10Hz (taskSteering)
- Gauge z0 + Text t0 trên page monitor(sas)

### UX
- Startup push Module Info ngay khi boot
- PostInitialize Events đã add đủ tất cả pages
- Smart back button About page (vaPageBack.val)
- About page: uptime MM:SS
- taskNextionTX 800ms refresh
- GitHub workflow: AGENTS.md + STATUS.md + git local

### GitHub
- Repo: https://github.com/quangnguyenken123-bit/Xpander_VCI
- Default branch: master ✅

## ⚠️ CHƯA TEST TRÊN XE

- ECM DTC read/clear (cần tạo lỗi: tháo injector connector)
- SAS Module Info (cần cap Launch sáng mai)
- SAS DTC read/clear
- EPS Steering Angle
- Actuator Test 4 injectors (cần nổ máy)
- Live data khi xe đang chạy (hôm qua ok nhưng đơ)

## 📋 SÁNG MAI — THỨ TỰ TEST

1. Cắm OBD → kiểm tra Module Info hiện VIN ngay
2. Live Data: kiểm tra 30 PIDs + tốc độ cập nhật
3. Cap Launch SAS Module Info → check fields thật
4. Test DTC: tháo connector injector → Read DTC → Clear DTC
5. Test Steering Angle: tay xoay vô lăng xem kim la bàn
6. Test Actuator Test khi nổ máy

## 🔧 VIỆC CÒN LẠI (không cần cho test sáng mai)

- SAS Module Info: update fields sau khi có data Launch
- About page: QR code URL (chờ có poster)
- Page dtc search (page 13): deferred
- SD Logger: hardware hỏng, deferred
- taskPrintSerial cleanup (minor)

## ⚠️ KNOWN ISSUES / LIMITATIONS

- SAS LID 0x9C trả NRC 0x12 → chỉ dùng LID 0x87
- Mode 21 (0x1D, 0x1E) ECU không trả → ON/OFF hardcode theo RPM
- SD module hỏng → sd_logger.ino placeholder only
- Page name có space/ngoặc → dùng bare command (không dùng cross-page)
