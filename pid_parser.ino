// ============================================================
// File: pid_parser.ino
// Mô tả: Parse response từ ECU -> ghi vào xData
//        Công thức dựa trên OBD2 chuẩn + Excel giải thuật
// ============================================================
#include "config.h"
#include "vehicle_data.h"

// ============================================================
// PARSE MODE 01 (Standard OBD2)
// rxBuf[1]=0x41, rxBuf[2]=PID, rxBuf[3]=A, rxBuf[4]=B
// ============================================================
void parseMode01(uint8_t pid, unsigned char* rxBuf, uint8_t len) {
  LOCK_DATA {
    switch (pid) {

      case 0x0C: // RPM
        xData.rpm = ((256.0f * rxBuf[3]) + rxBuf[4]) / 4.0f;
        break;

      case 0x0D: // Speed
        xData.speed = rxBuf[3];
        break;

      case 0x05: // Engine Coolant Temp
        xData.engineTemp = (float)rxBuf[3] - 40.0f;
        break;

      case 0x0F: // Intake Air Temp
        xData.intakeTemp = (float)rxBuf[3] - 40.0f;
        break;

      case 0x11: // Throttle Position
        xData.throttle = (rxBuf[3] * 100.0f) / 255.0f;
        break;

      case 0x43: // Engine Load (absolute)
        xData.engineLoad = (256.0f * rxBuf[3] + rxBuf[4]) * (100.0f / 255.0f);
        break;

      case 0x04: // Calculated Load
        xData.calcLoad = (rxBuf[3] * 100.0f) / 255.0f;
        break;

      case 0x1F: // Run Time Since Start
        xData.runTime = (256.0f * rxBuf[3]) + rxBuf[4];
        break;

      case 0x0E: // Timing Advance
        xData.timingAdvance = (rxBuf[3] / 2.0f) - 64.0f;
        break;

      case 0x44: // Lambda (O2 commanded)
        xData.lambda = (256.0f * rxBuf[3] + rxBuf[4]) / 32768.0f;
        break;

      case 0x06: // Short Term Fuel Trim
        xData.stf = (rxBuf[3] / 1.28f) - 100.0f;
        break;

      case 0x07: // Long Term Fuel Trim
        xData.ltf = (rxBuf[3] / 1.28f) - 100.0f;
        break;

      case 0x0B: // MAP
        xData.map_kpa = rxBuf[3];
        break;

      case 0x33: // Barometric Pressure
        xData.baro_kpa = rxBuf[3];
        break;

      case 0x03: // Fuel System Status
        xData.fuelSysStatus = rxBuf[3];
        break;

      case 0x51: // Fuel Type
        if      (rxBuf[3] == 0x01) xData.fuelType = "Gasoline";
        else if (rxBuf[3] == 0x04) xData.fuelType = "Diesel";
        else                        xData.fuelType = "Other";
        break;

      case 0x14: // O2 Sensor Front
        xData.o2Front = rxBuf[3] / 200.0f;
        break;

      case 0x15: // O2 Sensor Rear
        xData.o2Rear = rxBuf[3] / 200.0f;
        break;

      case 0x42: // Control Voltage
        xData.controlVolt = ((256.0f * rxBuf[3]) + rxBuf[4]) / 1000.0f;
        break;

      case 0x49: // Accelerator Pedal D
        xData.pedalD = (rxBuf[3] * 100.0f) / 255.0f;
        break;

      case 0x4A: // Accelerator Pedal E
        {
          float raw = (rxBuf[3] * 100.0f) / 255.0f;
          xData.pedalE = (raw > 0.5f) ? raw : (xData.pedalD * 0.5f);
        }
        break;

      case 0x46: // Ambient Air Temp
        xData.ambientTemp = (float)rxBuf[3] - 40.0f;
        break;

      default:
        break;
    }
    xData.dataValid  = true;
    xData.lastUpdate = millis();
  }
  UNLOCK_DATA;
}

// ============================================================
// PARSE MODE 21 (Mitsubishi Specific - Bit flags)
// Dựa theo sheet giải thuật Excel:
//   LID 0x1D = Input Group (công tắc)
//   LID 0x1E = Relay Group
// rxBuf[1]=0x61, rxBuf[2]=LID, rxBuf[3]=byte1...
// ============================================================
void parseMode21(uint8_t lid, unsigned char* rxBuf, uint8_t len) {
  LOCK_DATA {
    switch (lid) {

      case 0x1D: // Input Group
        // Byte1 (rxBuf[3]):
        //   bit3 = AC Switch
        //   bit6 = Cranking Signal
        // Byte2 (rxBuf[4]):
        //   bit0 = Ignition Switch
        // Byte3 (rxBuf[5]):
        //   bit5 = Clutch Switch
        xData.acSwitch       = (rxBuf[3] >> 3) & 0x01;
        xData.crankingSignal = (rxBuf[3] >> 6) & 0x01;
        xData.ignitionSw     = (rxBuf[4] >> 0) & 0x01;
        xData.clutchSw       = (rxBuf[5] >> 5) & 0x01;
        break;

      case 0x1E: // Relay Group
        // Byte1 (rxBuf[3]):
        //   bit0 = AC Compressor Relay
        //   bit1 = Fuel Pump Relay
        //   bit6 = Starter Relay
        xData.acRelay       = (rxBuf[3] >> 0) & 0x01;
        xData.fuelPumpRelay = (rxBuf[3] >> 1) & 0x01;
        xData.starterRelay  = (rxBuf[3] >> 6) & 0x01;
        break;

      default:
        break;
    }
    xData.lastUpdate = millis();
  }
  UNLOCK_DATA;
}

// ============================================================
// IN DỮ LIỆU RA SERIAL (Dùng để debug khi chưa có Nextion)
// ============================================================
void printLiveData() {
  Serial.println("\n========== LIVE DATA ==========");
  Serial.printf("RPM          : %.0f rpm\n",    xData.rpm);
  Serial.printf("Speed        : %.0f km/h\n",   xData.speed);
  Serial.printf("Coolant Temp : %.1f C\n",       xData.engineTemp);
  Serial.printf("Intake Temp  : %.1f C\n",       xData.intakeTemp);
  Serial.printf("Throttle     : %.2f %%\n",      xData.throttle);
  Serial.printf("Engine Load  : %.2f %%\n",      xData.engineLoad);
  Serial.printf("Timing Adv   : %.1f deg\n",     xData.timingAdvance);
  Serial.printf("Lambda       : %.3f\n",          xData.lambda);
  Serial.printf("STF          : %.2f %%\n",      xData.stf);
  Serial.printf("LTF          : %.2f %%\n",      xData.ltf);
  Serial.printf("MAP          : %.0f kPa\n",     xData.map_kpa);
  Serial.printf("Baro         : %.0f kPa\n",     xData.baro_kpa);
  Serial.printf("O2 Front     : %.3f V\n",       xData.o2Front);
  Serial.printf("O2 Rear      : %.3f V\n",       xData.o2Rear);
  Serial.printf("Control Volt : %.2f V\n",       xData.controlVolt);
  Serial.printf("Pedal D      : %.2f %%\n",      xData.pedalD);
  Serial.printf("Pedal E      : %.2f %%\n",      xData.pedalE);
  Serial.printf("Run Time     : %.0f s\n",       xData.runTime);
  Serial.printf("Fuel Type    : %s\n",           xData.fuelType.c_str());
  Serial.println("================================");
}

void printModuleInfo() {
  Serial.println("\n======= MODULE INFORMATION =====");
  Serial.printf("VIN          : %s\n",  xData.vin.c_str());
  Serial.printf("Fuel Type    : %s\n",  xData.fuelType.c_str());
  Serial.printf("Ambient Temp : %.1f C\n", xData.ambientTemp);
  Serial.printf("Run Time     : %.0f s\n", xData.runTime);
  Serial.printf("Control Volt : %.2f V\n", xData.controlVolt);
  Serial.println("================================");
}