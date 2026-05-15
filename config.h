// ============================================================
// File: config.h
// Mô tả: Toàn bộ cấu hình phần cứng và hằng số hệ thống
//        Chỉ cần sửa file này khi thay đổi phần cứng
// ============================================================
#ifndef CONFIG_H
#define CONFIG_H

// ============================================================
// 1. CAN BUS (MCP2515)
// ============================================================
#define CAN_CS_PIN        5       // SPI Chip Select
#define CAN_INT_PIN       21      // Interrupt pin
#define CAN_BAUDRATE      CAN_500KBPS
#define CAN_OSC_FREQ      MCP_8MHZ

// CAN ID - OBD2 Standard
#define CAN_ID_BROADCAST  0x7DF   // Gửi broadcast tới tất cả ECU
#define CAN_ID_ECM_REQ    0x7E0   // Request tới ECM
#define CAN_ID_ECM_RESP   0x7E8   // Response từ ECM
#define CAN_ID_TCM_REQ    0x7E1   // Request tới TCM (hộp số)
#define CAN_ID_TCM_RESP   0x7E9   // Response từ TCM
#define SAS_REQ_ID    0x622    // ← đổi từ 0x784
#define SAS_RESP_ID   0x484    // ← đổi từ 0x785
#define EPS_REQ_ID              0x796
#define EPS_RESP_ID             0x797
#define ISOTP_TIMEOUT_MS        1000
#define ISOTP_MAX_PENDING_RETRY 3
#define ISOTP_FLOW_CONTROL_BS   0x08
#define ISOTP_FLOW_CONTROL_ST   0x0A

// Timeout chờ response (ms)
#define CAN_TIMEOUT_MS    100

// ============================================================
// 2. SPI BUS (Dùng chung MCP2515 + MicroSD)
// ============================================================
#define SPI_MOSI_PIN      23
#define SPI_MISO_PIN      19
#define SPI_SCK_PIN       18      // Xác nhận lại chân SCK trên board của bạn

// MicroSD - TẠM THỜI CHƯA DÙNG (module đang hỏng)
// #define SD_CS_PIN      xx      // TODO: Điền vào sau khi thay module mới
#define SD_ENABLED        false   // Bật lên khi SD hoạt động

// ============================================================
// 3. NEXTION DISPLAY (UART2)
// ============================================================
#define NEXTION_SERIAL    Serial2
#define NEXTION_TX_PIN    17
#define NEXTION_RX_PIN    16
#define NEXTION_BAUDRATE  9600

// ============================================================
// 4. THÔNG SỐ HỆ THỐNG
// ============================================================
#define SERIAL_DEBUG_BAUDRATE   115200  // Serial monitor

// Chu kỳ task FreeRTOS (ms)
#define TASK_CAN_DELAY_MS       20      // Delay giữa 2 PID request
#define TASK_NEXTION_DELAY_MS   50      // Refresh màn hình
#define TASK_SD_DELAY_MS        500     // Ghi log (khi SD hoạt động)

// Stack size FreeRTOS (bytes)
#define STACK_CAN_TASK          12288
#define STACK_NEXTION_TASK      4096
#define STACK_SD_TASK           4096

// Priority FreeRTOS (số càng cao càng ưu tiên)
#define PRIORITY_CAN_TASK       3
#define PRIORITY_NEXTION_TASK   2
#define PRIORITY_SD_TASK        1

// ============================================================
// 5. GIÁ TRỊ GIẢ LẬP (Fix cứng - giữ lại từ code cũ)
// Xóa sau khi ECU trả về đúng giá trị thực
// ============================================================
#define FIX_INJECTOR_MS         2.05f
#define FIX_THROTTLE_PCT        3.14f
#define FIX_BRAKE_VOLT          1.45f
#define CACHED_VIN              "RLA0LNNC1L1000001"

#endif // CONFIG_H