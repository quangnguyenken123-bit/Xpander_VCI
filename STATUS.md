# PROJECT STATUS — Xpander_VCI
# Cập nhật: 15/05/2026

## ✅ HOẠT ĐỘNG TỐT (đã test trên xe)
- Live Data 30 PID realtime (RPM, Speed, Coolant Temp, v.v.)
- Module Information: VIN + hardcoded Software/Hardware/Cal/Protocol
- Nextion UART communication (9600 baud)
- ISO-TP transport layer (isotp.ino)
- KWP2000 SID 0x21 decode logic
- MCP2515 CAN filter (ECM 0x7E8, TCM 0x7E9, SAS 0x484, EPS 0x797)
- VIN đọc từ xe qua ISO-TP, fallback CACHED_VIN offline

## ⚠️ CÓ CODE NHƯNG CHƯA TEST ĐỦ
- ECM Read DTC (SID 0x18) — code đúng, chưa test khi xe có lỗi thật
- ECM Clear DTC (SID 0x14) — code đúng, chưa test
- SAS Module Info (LID 0x87) — code đúng, chưa test
- EPS Steering Angle — code đúng, chưa test trên xe
- UX startup tự động (phụ thuộc Nextion PostInitialize Event chưa add đủ)

## ❌ CHƯA IMPLEMENT
- Actuator Test 4 injectors (có CAN frame, chưa code vào actuator_test.ino)
- SAS Read/Clear DTC (có spec, chưa hook vào Nextion)
- SD Logger (hardware hỏng)

## 🐛 BUG BIẾT TRƯỚC (chưa fix)

### Bug 1 — CRITICAL: Task order trong setup() sai
`taskTesterPresent` và `taskSteering` được tạo TRƯỚC khi mutex và `hTaskCAN` sẵn sàng.
**Fix cần**: di chuyển xTaskCreate của 2 task này xuống SAU dòng `xTaskCreate(taskReadCAN, ...)`.

### Bug 2 — steering_module.ino thiếu extern currentPage  
`taskSteering` dùng `currentPage` nhưng không có `extern volatile int currentPage;`.
**Fix cần**: thêm `extern volatile int currentPage;` vào đầu steering_module.ino.

### Bug 3 — taskNextionTX thiếu startup push
Module Info chỉ push khi nhận `p:5` từ Nextion. Nếu PostInitialize Event chưa add → không bao giờ push.
**Fix cần**: thêm `updateModuleInfoPage();` sau `vTaskDelay(3000)` trước vòng `for(;;)`.

### Bug 4 — updateLiveDataPage() gọi 3 lần/loop
Trong taskNextionTX, page 11 gọi updateLiveDataPage() dư 2 lần.

## 📋 NEXTION STATUS
- PostInitialize Event: **CHƯA add đủ tất cả pages** (cần thêm `print "p:X" + printh 0a`)
- Page 9 (monitor sas): đã có z0 (Gauge), t0 (Text), m0 (Back), m1 (Home)
- Page 12 (dtc): đã có t0 (Text kết quả), m0 (Read), m1 (Clear)

## 🔧 VIỆC CẦN LÀM TIẾP THEO (theo thứ tự ưu tiên)
1. Fix 4 bugs trên
2. Thêm PostInitialize Event cho tất cả Nextion pages → compile → flash SD
3. Test DTC khi xe có lỗi thật (tháo connector injector)
4. Implement actuator_test.ino (chờ CAN frame)
5. Test SAS Module Info + EPS Steering trên xe
