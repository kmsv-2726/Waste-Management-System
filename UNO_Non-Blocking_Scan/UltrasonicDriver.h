#ifndef ULTRASONIC_DRIVER_H
#define ULTRASONIC_DRIVER_H

#include <Arduino.h>

class UltrasonicDriver {
public:
    UltrasonicDriver(uint8_t trigPin, uint8_t echoPin);
    void begin();
    void trigger();
    bool isReady();
    int getDistance();

private:
    uint8_t _trigPin;
    uint8_t _echoPin;

    static volatile unsigned long startTime;
    static volatile unsigned long endTime;
    static volatile bool ready;

    static void echoISR();
};

#endif
