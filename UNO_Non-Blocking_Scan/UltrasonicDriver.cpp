#include "UltrasonicDriver.h"

volatile unsigned long UltrasonicDriver::startTime = 0;
volatile unsigned long UltrasonicDriver::endTime = 0;
volatile bool UltrasonicDriver::ready = false;

static UltrasonicDriver* instance = nullptr;

UltrasonicDriver::UltrasonicDriver(uint8_t trigPin, uint8_t echoPin) {
    _trigPin = trigPin;
    _echoPin = echoPin;
    instance = this;
}

void UltrasonicDriver::begin() {
    pinMode(_trigPin, OUTPUT);
    pinMode(_echoPin, INPUT);
    digitalWrite(_trigPin, LOW);

    PCICR |= (1 << PCIE0);
    PCMSK0 |= (1 << PCINT0);
}

void UltrasonicDriver::trigger() {
    ready = false;
    digitalWrite(_trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(_trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(_trigPin, LOW);
}

ISR(PCINT0_vect) {
    if (digitalRead(8) == HIGH) {
        UltrasonicDriver::startTime = micros();
    } else {
        UltrasonicDriver::endTime = micros();
        UltrasonicDriver::ready = true;
    }
}

bool UltrasonicDriver::isReady() {
    return ready;
}

int UltrasonicDriver::getDistance() {
    ready = false;
    unsigned long duration = endTime - startTime;
    return duration * 0.034 / 2;
}
