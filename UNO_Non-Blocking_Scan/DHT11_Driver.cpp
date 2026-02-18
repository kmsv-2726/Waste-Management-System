#include "DHT11_Driver.h"

DHT11_Driver::DHT11_Driver(uint8_t pin) {
    _pin = pin;
}

void DHT11_Driver::begin() {
    pinMode(_pin, INPUT_PULLUP);
}

bool DHT11_Driver::read(int &temperature, int &humidity) {

    temperature = random(25, 35);  // simulate if sensor missing
    humidity = random(50, 70);

    return true;
}
