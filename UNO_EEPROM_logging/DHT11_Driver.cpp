#include "DHT11_Driver.h"

DHT11_Driver::DHT11_Driver(uint8_t pin) { _pin = pin; }

void DHT11_Driver::begin() {
  pinMode(_pin, OUTPUT);
  digitalWrite(_pin, HIGH);
}

bool DHT11_Driver::read(int &temperature, int &humidity) {

  uint8_t data[5] = {0};

  pinMode(_pin, OUTPUT);
  digitalWrite(_pin, LOW);
  delay(20);
  digitalWrite(_pin, HIGH);
  delayMicroseconds(30);
  pinMode(_pin, INPUT);

  if (pulseIn(_pin, LOW) == 0) return false;
  if (pulseIn(_pin, HIGH) == 0) return false;

  for (int i = 0; i < 40; i++) {
    pulseIn(_pin, LOW);
    unsigned long duration = pulseIn(_pin, HIGH);
    data[i/8] <<= 1;
    if (duration > 40)
      data[i/8] |= 1;
  }

  if ((data[0]+data[1]+data[2]+data[3]) != data[4])
    return false;

  humidity = data[0];
  temperature = data[2];

  return true;
}