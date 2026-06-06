#include "config.h"
#include <SD.h>
#include <SPI.h>

struct DTCRecord {
  char code[8];   // "P0201"
  char desc[48];  // "Injector 1 Circuit Open"
};

#define MAX_DTC_RECORDS 120
#define DTC_LINES_PER_PAGE 6
#define DTC_MAX_RESULTS 50
static DTCRecord dtcDatabase[MAX_DTC_RECORDS];
static int dtcDatabaseSize = 0;
static bool sdReady = false;
static String dtcResults[DTC_MAX_RESULTS];
static int dtcResultCount = 0;
static int dtcPageIndex = 0;

bool loadDTCDatabase();
extern void nxSendCmd(const String& cmd);

bool initSD() {
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("[SD] Init FAIL");
    sdReady = false;
    return false;
  }
  Serial.println("[SD] Init OK");
  sdReady = true;
  return loadDTCDatabase();
}

bool loadDTCDatabase() {
  File f = SD.open("/database.csv");
  if (!f) {
    Serial.println("[SD] database.csv not found");
    return false;
  }

  dtcDatabaseSize = 0;
  if (f.available()) f.readStringUntil('\n'); // skip header "DTC Code,Description"
  while (f.available() && dtcDatabaseSize < MAX_DTC_RECORDS) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() < 5) continue;

    int comma = line.indexOf(',');
    if (comma < 0) continue;

    String code = line.substring(0, comma);
    String desc = line.substring(comma + 1);
    code.trim();
    desc.trim();

    strncpy(dtcDatabase[dtcDatabaseSize].code,
            code.c_str(), sizeof(dtcDatabase[dtcDatabaseSize].code) - 1);
    strncpy(dtcDatabase[dtcDatabaseSize].desc,
            desc.c_str(), sizeof(dtcDatabase[dtcDatabaseSize].desc) - 1);
    dtcDatabase[dtcDatabaseSize].code[sizeof(dtcDatabase[dtcDatabaseSize].code) - 1] = '\0';
    dtcDatabase[dtcDatabaseSize].desc[sizeof(dtcDatabase[dtcDatabaseSize].desc) - 1] = '\0';
    dtcDatabaseSize++;
  }
  f.close();
  Serial.printf("[SD] Loaded %d DTC records\n", dtcDatabaseSize);
  return (dtcDatabaseSize > 0);
}

String lookupDTC(const String& code) {
  for (int i = 0; i < dtcDatabaseSize; i++) {
    if (code.equalsIgnoreCase(dtcDatabase[i].code)) {
      return String(dtcDatabase[i].desc);
    }
  }
  return "";
}

static String buildDtcPageText() {
  if (dtcResultCount == 0) return "No DTC found";

  String pageText = "";
  int start = dtcPageIndex * DTC_LINES_PER_PAGE;
  int end = start + DTC_LINES_PER_PAGE;
  if (end > dtcResultCount) end = dtcResultCount;

  for (int i = start; i < end; i++) {
    if (pageText.length() > 0) pageText += "\r\n";
    pageText += dtcResults[i];
  }
  return pageText;
}

void renderDtcPage() {
  String pageText = buildDtcPageText();
  nxSendCmd(String("t0.txt=\"") + pageText + "\"");
}

void dtcNextPage() {
  if (dtcResultCount == 0) {
    renderDtcPage();
    return;
  }

  int lastPage = (dtcResultCount - 1) / DTC_LINES_PER_PAGE;
  if (dtcPageIndex < lastPage) {
    dtcPageIndex++;
    renderDtcPage();
  }
}

void dtcPrevPage() {
  if (dtcResultCount == 0) {
    renderDtcPage();
    return;
  }

  if (dtcPageIndex > 0) {
    dtcPageIndex--;
    renderDtcPage();
  }
}

String searchDTCByCategory(char category) {
  dtcResultCount = 0;
  dtcPageIndex = 0;

  for (int i = 0; i < dtcDatabaseSize && dtcResultCount < DTC_MAX_RESULTS; i++) {
    if (dtcDatabase[i].code[0] == category) {
      dtcResults[dtcResultCount] = String(dtcDatabase[i].code)
                                   + ": " + String(dtcDatabase[i].desc);
      dtcResultCount++;
    }
  }

  renderDtcPage();
  return buildDtcPageText();
}
