#ifndef ULTRASONIC_DRIVER_H
#define ULTRASONIC_DRIVER_H

class UltrasonicDriver {
  int trigPin, echoPin;

public:
  UltrasonicDriver(int trig, int echo) {
    trigPin = trig;
    echoPin = echo;
  }

  void begin() {
    pinMode(trigPin, OUTPUT);
    pinMode(echoPin, INPUT);
    digitalWrite(trigPin, LOW);
  }

  int readDistance() {
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);

    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    long duration = pulseIn(echoPin, HIGH, 30000);

    if (duration == 0) return -1;
    return duration / 58;
  }
};

#endif
