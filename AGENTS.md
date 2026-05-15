# Xpander_VCI — AGENTS.md
# OBD2 Diagnostic VCI for Mitsubishi Xpander 2020

## Hardware
- **MCU**: ESP32 Dev Module
- **CAN**: MCP2515 (8MHz crystal) — CS=GPIO5, INT=GPIO21, MOSI=23, MISO=19, SCK=18
- **Display**: Nextion NX8048P050_011 (5", 800x480) — UART2: TX=GPIO17, RX=GPIO16 @ 9600 baud
- **CAN Bus**: ISO 15765-4, 500 kbps, OBD2 Pin 6/14

## File Structure
| File | Mô tả |
|------|-------|
| `config.h` | Pins, CAN IDs, constants, FreeRTOS params |
| `vehicle_data.h` | VehicleData + SteeringData + SasInfo structs + LOCK_DATA macro |
| `Xpander_VCI.ino` | setup(), loop(), taskReadCAN, taskTesterPresent, readVIN() |
| `can_handler.ino` | MCP2515 init, filter setup, canSendMode01/Raw, canReceive |
| `pid_parser.ino` | parseMode01() for all 22 OBD2 PIDs |
| `dtc_handler.ino` | readECMDTC/clearECMDTC/readSASDTC/clearSASDTC via SID 0x18/0x14 + ISO-TP |
| `isotp.ino` | ISO-TP transport layer (FF/CF/FC/NRC 0x78) |
| `kwp2000.ino` | KWP2000 SID 0x1A, 0x21, 0x3E |
| `steering_module.ino` | eps_read_steering_angle(), sas_read_module_info(), taskSteering |
| `nextion_ui.ino` | nxSendCmd, setupNextion, taskNextionRX, flag parsing |
| `nextion_pages.ino` | updateLiveDataPage, updateModuleInfoPage, updateSteeringPage, taskNextionTX |
| `actuator_test.ino` | Placeholder — chưa implement |
| `sd_logger.ino` | Placeholder — SD module hỏng |

## CAN IDs (VERIFIED on Xpander 2020)
```
ECM:  REQ=0x7E0  RESP=0x7E8   ← Standard OBD2
TCM:  REQ=0x7E1  RESP=0x7E9
SAS:  REQ=0x622  RESP=0x484   ← KWP2000 + ISO-TP
EPS:  REQ=0x796  RESP=0x797   ← KWP2000 + ISO-TP
```

## Critical Rules (MUST follow)

### FreeRTOS / Concurrency
1. **Luôn gọi `vTaskSuspend(hTaskCAN)` trước mọi ISO-TP request** (đọc DTC, steering, SAS info)
2. **Luôn gọi `vTaskResume(hTaskCAN)` TRƯỚC MỌI câu `return`** — không được bỏ sót
3. **Mọi truy cập `xData` phải dùng `LOCK_DATA { ... } UNLOCK_DATA;`**

### Setup Order (QUAN TRỌNG)
```cpp
// ĐÚNG thứ tự trong setup():
xDataMutex = xSemaphoreCreateMutex();  // 1. Mutex TRƯỚC TIÊN
setupCAN();                             // 2. CAN init
readVIN();                              // 3. VIN (blocking, 2s timeout)
setupNextion();                         // 4. Nextion UART
xTaskCreate(taskReadCAN, ...);          // 5. Tạo CAN task → gán hTaskCAN
xTaskCreate(taskTesterPresent, ...);    // 6. Tester Present
xTaskCreate(taskSteering, ...);         // 7. Steering (dùng hTaskCAN → phải sau bước 5)
xTaskCreate(taskNextionRX, ...);        // 8. Nextion tasks
xTaskCreate(taskNextionTX, ...);
```

### Nextion Commands
- **Cross-page**: `nxSendCmd("i4.t_vin.txt=\"value\"")` — hoạt động khi user ở page khác
- **Bare command**: `nxSendCmd("t0.txt=\"value\"")` — chỉ hoạt động khi user đang ở page đó
- **Gauge**: `nxSendCmd("z0.val=90")` — 90 = 12 giờ = 0° lái; val giảm = quay phải

### DTC Decode
```cpp
uint16_t raw = ((uint16_t)hi << 8) | lo;
char letter = "PCBU"[raw >> 14];
sprintf(buf, "%c%d%03X", letter, (raw >> 12) & 3, raw & 0x0FFF);
```

## Nextion Pages
| Page ID | objname | Mô tả |
|---------|---------|-------|
| 0 | page0 | Home menu |
| 1 | sas | SAS menu |
| 2 | ecm | ECM menu |
| 4 | diag | Diagnostic menu |
| 5 | i4 | Module Information |
| 9 | monitor(sas) | Steering angle gauge |
| 11 | live data | Live data (30 PIDs, slider h0) |
| 12 | dtc | DTC read/clear |

## Nextion Navigation Protocol
- Pages gửi `p:XX\n` về ESP32 khi load (PostInitialize Event)
- Slider gửi `scroll:` + 4 bytes little-endian
- DTC buttons gửi `read_dtc\n` / `clear_dtc\n`

## Known Issues / Limitations
- SAS LID 0x9C trả NRC 0x12 → chỉ dùng LID 0x87
- Mode 21 (0x1D, 0x1E) ECU không trả → ON/OFF hardcode theo RPM
- SD module hỏng → sd_logger.ino placeholder only
- Actuator test 4 injectors → chưa có CAN frame
