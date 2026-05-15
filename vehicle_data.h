// ============================================================
// File: vehicle_data.h
// Mô tả: Struct dữ liệu trung tâm + Mutex bảo vệ
// ============================================================
#ifndef VEHICLE_DATA_H
#define VEHICLE_DATA_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// ============================================================
// STRUCT DỮ LIỆU XE
// ============================================================
struct VehicleData {

  // --- NHÓM 1: ĐỘNG CƠ CƠ BẢN ---
  float rpm             = 0;
  float speed           = 0;
  float engineTemp      = 0;    // °C
  float intakeTemp      = 0;    // °C
  float throttle        = 0;    // %
  float engineLoad      = 0;    // %
  float calcLoad        = 0;    // %
  float runTime         = 0;    // giây
  float timingAdvance   = 0;    // °CA

  // --- NHÓM 2: NHIÊN LIỆU & KHÍ NẠP ---
  float lambda          = 0;
  float stf             = 0;    // Short Term Fuel Trim %
  float ltf             = 0;    // Long Term Fuel Trim %
  float map_kpa         = 0;    // Manifold Absolute Pressure kPa
  float baro_kpa        = 0;    // Barometric Pressure kPa
  float fuelSysStatus   = 0;
  String fuelType       = "---";

  // --- NHÓM 3: CƠ CẤU CHẤP HÀNH ---
  float pedalD          = 0;    // %
  float pedalE          = 0;    // %
  float injectorMs      = 0;    // ms  (fix cứng)
  float throttleActPct  = 0;    // %   (fix cứng)
  float brakeBoostV     = 0;    // V   (fix cứng)

  // --- NHÓM 4: CẢM BIẾN ĐIỆN ---
  float o2Front         = 0;    // V
  float o2Rear          = 0;    // V
  float controlVolt     = 0;    // V

  // --- NHÓM 5: TRẠNG THÁI MITSUBISHI (bit flags) ---
  bool  acSwitch        = false;
  bool  acRelay         = false;
  bool  clutchSw        = false;
  bool  ignitionSw      = false;
  bool  crankingSignal  = false;
  bool  fuelPumpRelay   = false;
  bool  starterRelay    = false;

  // --- NHÓM 6: MODULE INFO ---
  String vin            = "Chua doc...";
  String ambientTemp_s  = "---";   // dùng string để hiển thị Nextion
  float  ambientTemp    = 0;

  // --- TRẠNG THÁI HỆ THỐNG ---
  bool  dataValid       = false;
  unsigned long lastUpdate = 0;
};

// ĐÃ XÓA DÒNG EXTERN BỊ DƯ Ở ĐÂY

struct SteeringData {
  float angleDeg     = 0;
  float velocityDps  = 0;
  unsigned long lastUpdate = 0;
  bool  valid        = false;
};

extern SteeringData steering; // <-- CHỈ GIỮ LẠI DÒNG NÀY Ở DƯỚI CÙNG

struct SasInfo {
  char partNumber87[16]; 
  bool valid = false;
};

extern SasInfo sasInfo;

// ============================================================
// BIẾN TOÀN CỤC - extern (định nghĩa thực ở Xpander_VCI.ino)
// ============================================================
extern VehicleData xData;
extern SemaphoreHandle_t xDataMutex;

// ============================================================
// MACRO AN TOÀN ĐỌC/GHI STRUCT
// ============================================================
#define LOCK_DATA   if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(10)) == pdTRUE)
#define UNLOCK_DATA xSemaphoreGive(xDataMutex);

#endif // VEHICLE_DATA_H