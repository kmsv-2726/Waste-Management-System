#ifndef DHT11_DRIVER_H
#define DHT11_DRIVER_H

#include <Arduino.h>

class DHT11_Driver {
public:
    DHT11_Driver(uint8_t pin);
    void begin();
    bool read(int &temperature, int &humidity);

private:
    uint8_t _pin;
};

#endif
