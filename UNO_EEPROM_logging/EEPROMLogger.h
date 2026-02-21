#ifndef EEPROM_LOGGER_H
#define EEPROM_LOGGER_H
#include <Arduino.h>
#include <EEPROM.h>

#define MAX_RECORDS 10
#define RECORD_SIZE 120

class EEPROMLogger {
public:
  void begin();
  void saveRecord(String data, int seq);
  bool getRecord(int index, String &data);
  void markDelivered(int index);
};

#endif