#ifndef DHT11_DRIVER_H
#define DHT11_DRIVER_H

class DHT11_Driver {
  int pin;

public:
  DHT11_Driver(int p) {
    pin = p;
  }

  void begin() {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
  }

  bool read(int &temperature, int &humidity) {
    uint8_t data[5] = {0};

    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    delay(20);
    digitalWrite(pin, HIGH);
    delayMicroseconds(40);
    pinMode(pin, INPUT);

    if (pulseIn(pin, LOW, 100) == 0) return false;
    if (pulseIn(pin, HIGH, 100) == 0) return false;

    for (int i = 0; i < 40; i++) {
      pulseIn(pin, LOW);
      long duration = pulseIn(pin, HIGH);
      if (duration > 40)
        data[i/8] |= (1 << (7 - (i%8)));
    }

    if ((data[0] + data[1] + data[2] + data[3]) != data[4])
      return false;

    temperature = data[2];
    humidity = data[0];
    return true;
  }
};

#endif
