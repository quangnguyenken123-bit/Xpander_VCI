// ============================================================
// File: nextion_pages.ino
// Mô tả: Logic cập nhật dữ liệu lên Nextion theo từng page
// ============================================================
#include "config.h"
#include "vehicle_data.h"
#define FALLBACK_SOFT   "1860D00500"
#define FALLBACK_HARD   "1860D005"
#define FALLBACK_CAL    "1860D0050001"
#define FALLBACK_PROTO  "ISO 15765-4 CAN"

extern volatile int  currentPage;
extern volatile int  scrollOffset;
extern volatile bool flagReadDTC;
extern volatile bool flagClearDTC;
extern volatile bool flagResetConn;
extern volatile bool flagScrollChanged;

extern void nxSendCmd(const String& cmd);
extern void nxSetText(const String& pageObj, const String& compName, const String& value);
extern void nxSetVal(const String& pageObj, const String& compName, int value);
extern String readDTC(uint8_t mode);
extern void clearDTC();
extern void setupCAN();
extern String readECMDTC();
extern bool   clearECMDTC();
extern String readSASDTC();
extern bool   clearSASDTC();
// ============================================================
// DANH SÁCH 30 PID HIỂN THỊ TRÊN LIVE DATA
// ============================================================
const char* PID_NAMES[30] = {
  "RPM",           "Speed",         "Coolant Temp",  "Intake Temp",
  "Throttle Pos",  "Engine Load",   "Calc Load",     "Timing Adv",
  "Lambda",        "Short Term FT", "Long Term FT",  "MAP",
  "Baro Press",    "O2 Sensor 1",   "O2 Sensor 2",   "Control Volt",
  "Pedal D",       "Pedal E",       "Injector",      "Throttle Act",
  "Brake Boost",   "Run Time",      "Ambient Temp",  "Fuel Type",
  "AC Switch",     "AC Relay",      "Clutch SW",     "Ignition SW",
  "Cranking",      "Fuel Pump"
};

const char* PID_UNITS[30] = {
  "rpm",  "km/h",  "C",     "C",
  "%",    "%",     "%",     "deg",
  "",     "%",     "%",     "kPa",
  "kPa",  "V",     "V",     "V",
  "%",    "%",     "ms",    "%",
  "V",    "s",     "C",     "",
  "",     "",      "",      "",
  "",     ""
};

// ============================================================
// LẤY GIÁ TRỊ PID THỨ idx (0-29) DƯỚI DẠNG STRING
// ============================================================
String getPIDValueString(int idx) {
  char buf[12];
  switch (idx) {
    case 0:  snprintf(buf, sizeof(buf), "%.0f", xData.rpm);           break;
    case 1:  snprintf(buf, sizeof(buf), "%.0f", xData.speed);         break;
    case 2:  snprintf(buf, sizeof(buf), "%.1f", xData.engineTemp);    break;
    case 3:  snprintf(buf, sizeof(buf), "%.1f", xData.intakeTemp);    break;
    case 4:  snprintf(buf, sizeof(buf), "%.2f", xData.throttle);      break;
    case 5:  snprintf(buf, sizeof(buf), "%.2f", xData.engineLoad);    break;
    case 6:  snprintf(buf, sizeof(buf), "%.2f", xData.calcLoad);      break;
    case 7:  snprintf(buf, sizeof(buf), "%.1f", xData.timingAdvance); break;
    case 8:  snprintf(buf, sizeof(buf), "%.3f", xData.lambda);        break;
    case 9:  snprintf(buf, sizeof(buf), "%.2f", xData.stf);           break;
    case 10: snprintf(buf, sizeof(buf), "%.2f", xData.ltf);           break;
    case 11: snprintf(buf, sizeof(buf), "%.0f", xData.map_kpa);       break;
    case 12: snprintf(buf, sizeof(buf), "%.0f", xData.baro_kpa);      break;
    case 13: snprintf(buf, sizeof(buf), "%.3f", xData.o2Front);       break;
    case 14: snprintf(buf, sizeof(buf), "%.3f", xData.o2Rear);        break;
    case 15: snprintf(buf, sizeof(buf), "%.2f", xData.controlVolt);   break;
    case 16: snprintf(buf, sizeof(buf), "%.2f", xData.pedalD);        break;
    case 17: snprintf(buf, sizeof(buf), "%.2f", xData.pedalE);        break;
    case 18: snprintf(buf, sizeof(buf), "%.2f", xData.injectorMs);    break;
    case 19: snprintf(buf, sizeof(buf), "%.2f", xData.throttleActPct);break;
    case 20: snprintf(buf, sizeof(buf), "%.2f", xData.brakeBoostV);   break;
    case 21: snprintf(buf, sizeof(buf), "%.0f", xData.runTime);       break;
    case 22: snprintf(buf, sizeof(buf), "%.1f", xData.ambientTemp);   break;
    case 23: return xData.fuelType;
    case 24: return xData.acSwitch       ? "ON" : "OFF";
    case 25: return xData.acRelay        ? "ON" : "OFF";
    case 26: return xData.clutchSw       ? "ON" : "OFF";
    case 27: return xData.ignitionSw     ? "ON" : "OFF";
    case 28: return xData.crankingSignal ? "ON" : "OFF";
    case 29: return xData.fuelPumpRelay  ? "ON" : "OFF";
    default: return "---";
  }
  return String(buf);
}

// ============================================================
// CẬP NHẬT PAGE LIVE DATA (10 hàng theo scrollOffset)
// ============================================================
void updateLiveDataPage() {
  for (int i = 0; i < 10; i++) {
    int pidIdx = scrollOffset + i;
    if (pidIdx >= 30) pidIdx = 29;

    nxSendCmd("tname_" + String(i) + ".txt=\"" + String(PID_NAMES[pidIdx]) + "\"");
    nxSendCmd("t"      + String(i) + ".txt=\"" + getPIDValueString(pidIdx) + "\"");
    nxSendCmd("t"      + String(i + 10) + ".txt=\"" + String(PID_UNITS[pidIdx]) + "\"");
  }
}

// ============================================================
// CẬP NHẬT PAGE MODULE INFO (i4)
// VIN đọc thực, các field khác hardcode
// ============================================================
void updateModuleInfoPage() {
  // VIN: dùng thật nếu đọc được (17 ký tự), ngược lại dùng cache
  String vin = (xData.vin.length() == 17) ? xData.vin : String(CACHED_VIN);

  nxSendCmd("i4.t_vin.txt=\""  + vin              + "\"");
  nxSendCmd(String("i4.t_soft.txt=\"") + FALLBACK_SOFT  + "\"");
  nxSendCmd(String("i4.t_hard.txt=\"") + FALLBACK_HARD  + "\"");
  nxSendCmd(String("i4.t_cal.txt=\"")  + FALLBACK_CAL   + "\"");
  nxSendCmd(String("i4.t_pro.txt=\"")  + FALLBACK_PROTO + "\"");
}

void updateSASInfoPage() {
  extern SasInfo sasInfo;

  String pn = (sasInfo.valid && strlen(sasInfo.partNumber87) > 0)
              ? String(sasInfo.partNumber87)
              : "8600A732";

  nxSendCmd("t_vin.txt=\"N/A\"");
  nxSendCmd(String("t_soft.txt=\"") + pn + "\"");
  nxSendCmd("t_hard.txt=\"N/A\"");
  nxSendCmd("t_cal.txt=\"N/A\"");
  nxSendCmd("t_pro.txt=\"KWP2000 + ISO-TP\"");
}

void updateAboutPage() {
  uint32_t totalSecs = millis() / 1000;
  uint32_t mins = totalSecs / 60;
  uint32_t secs = totalSecs % 60;
  char buf[8];
  snprintf(buf, sizeof(buf), "%02u:%02u",
           (unsigned)mins, (unsigned)secs);
  nxSendCmd(String("about.t0.txt=\"") + buf + "\"");
}

// ============================================================
// XỬ LÝ "read_dtc" từ Nextion
// ============================================================
void handleReadDTC() {
  if (currentPage == 14) {
    Serial.println("[NX] Doc SAS DTC...");
    nxSendCmd("t0.txt=\"Reading SAS DTC...\"");
    String result = readSASDTC();
    nxSendCmd(String("t0.txt=\"") + result + "\"");
    Serial.printf("[NX] DTC Result: %s\n", result.c_str());
    return;
  }

  Serial.println("[NX] Doc ECM DTC...");
  nxSendCmd("t0.txt=\"Reading ECM DTC...\"");

  String result = readECMDTC();   // 1 lần, SID 0x18 trả tất cả
  nxSendCmd(String("t0.txt=\"") + result + "\"");
  Serial.printf("[NX] DTC Result: %s\n", result.c_str());
}

void handleClearDTC() {
  if (currentPage == 14) {
    Serial.println("[NX] Xoa SAS DTC...");
    nxSendCmd("t0.txt=\"Clearing SAS...\"");
    bool ok = clearSASDTC();
    nxSendCmd(ok ? "t0.txt=\"DTC Cleared!\"" : "t0.txt=\"Clear Failed\"");
    return;
  }

  Serial.println("[NX] Xoa ECM DTC...");
  nxSendCmd("t0.txt=\"Clearing ECM...\"");
  bool ok = clearECMDTC();
  nxSendCmd(ok ? "t0.txt=\"DTC Cleared!\"" : "t0.txt=\"Clear Failed\"");
}

// ============================================================
// XỬ LÝ "reset_conn" từ Nextion
// ============================================================
void handleResetConn() {
  Serial.println("[NX] Reset CAN bus...");
  setupCAN();
  Serial.println("[NX] Reset xong");
}

// ============================================================
// TASK: Push data lên Nextion + handle flags
// ============================================================
void taskNextionTX(void* pvParameters) {
  vTaskDelay(pdMS_TO_TICKS(3000));
  updateModuleInfoPage();

  int lastPushedPage = -1;
  for (;;) {
    if (flagReadDTC)       { flagReadDTC = false;       handleReadDTC(); }
    if (flagClearDTC)      { flagClearDTC = false;      handleClearDTC(); }
    if (flagResetConn)     { flagResetConn = false;     handleResetConn(); }
    if (flagScrollChanged) { flagScrollChanged = false; updateLiveDataPage(); }
    if (currentPage != lastPushedPage) {
      switch (currentPage) {
        case 5:  updateModuleInfoPage(); break;
        case 7:  updateSASInfoPage();    break;
        case 10: updateAboutPage();      break;
        case 11: updateLiveDataPage();   break;
        default: break;
      }
      lastPushedPage = currentPage;
    }

    if (currentPage == 11) updateLiveDataPage();
    if (currentPage == 10) updateAboutPage();
    vTaskDelay(pdMS_TO_TICKS(800));
  }
}
//
extern SteeringData steering;
//
void updateSteeringPage() {
  extern SteeringData steering;

  if (!steering.valid) {
    nxSendCmd("t0.txt=\"---\"");
    return;
  }

  // Hiện số góc trên Text t0
  char buf[12];
  snprintf(buf, sizeof(buf), "%.1f", steering.angleDeg);
  nxSendCmd(String("t0.txt=\"") + buf + "\"");

  // Xoay kim la bàn Gauge z0
  // Nextion gauge: val=90 = 12 giờ = 0° lái
  // Quay phải (+angle) → kim quay thuận chiều → val giảm
  int gaugeVal = 90 - (int)steering.angleDeg;
  gaugeVal = ((gaugeVal % 360) + 360) % 360;  // wrap 0-360
  nxSendCmd("z0.val=" + String(gaugeVal));
}
