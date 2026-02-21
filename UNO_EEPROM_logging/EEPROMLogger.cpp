#include "EEPROMLogger.h"

void EEPROMLogger::begin() {}

void EEPROMLogger::saveRecord(String data, int seq) {

  int index = seq % MAX_RECORDS;
  int base = index * RECORD_SIZE;

  EEPROM.write(base, 0);

  for (int i = 0; i < data.length() && i < RECORD_SIZE-2; i++)
    EEPROM.write(base + 1 + i, data[i]);

  EEPROM.write(base + RECORD_SIZE - 1, '\0');
}

bool EEPROMLogger::getRecord(int index, String &data) {

  int base = index * RECORD_SIZE;
  if (EEPROM.read(base) != 0) return false;

  data = "";
  for (int i = 1; i < RECORD_SIZE; i++) {
    char c = EEPROM.read(base + i);
    if (c == '\0') break;
    data += c;
  }
  return true;
}

void EEPROMLogger::markDelivered(int index) {
  EEPROM.write(index * RECORD_SIZE, 1);
}