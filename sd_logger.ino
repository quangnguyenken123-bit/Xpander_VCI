#include "config.h"
#include <SD.h>
#include <SPI.h>

struct DTCRecord {
  char code[8];   // "P0201"
  char desc[48];  // "Injector 1 Circuit Open"
};

#define MAX_DTC_RECORDS 120
static DTCRecord dtcDatabase[MAX_DTC_RECORDS];
static int dtcDatabaseSize = 0;
static bool sdReady = false;

bool loadDTCDatabase();

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

String searchDTCByCategory(char category) {
  String result = "";
  int count = 0;
  for (int i = 0; i < dtcDatabaseSize && count < 10; i++) {
    if (dtcDatabase[i].code[0] == category) {
      if (result != "") result += "\r\n";
      result += String(dtcDatabase[i].code)
                + ": " + String(dtcDatabase[i].desc);
      count++;
    }
  }
  if (result == "") result = "No codes found";
  return result;
}
