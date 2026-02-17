#include <SPI.h>
#include <LoRa.h>
#include <Servo.h>
#include "UltrasonicDriver.h"   
#include "DHT11_Driver.h"       

#define NODE_ID 2
#define PROTOCOL "SCANv1"       

#define LORA_SS   10
#define LORA_RST  9
#define LORA_DIO0 2

#define SERVO_PIN 6
#define TRIG_PIN  4
#define ECHO_PIN  3
#define GAS_PIN   A0
#define BAT_PIN   A1
#define DHT_PIN   7

#define LOW_BATTERY 3.5
#define ACK_TIMEOUT 3000
#define MAX_RETRY   3

Servo servo;
UltrasonicDriver ultrasonic(TRIG_PIN, ECHO_PIN); // 🔵 REPLACED OLD FUNCTION
DHT11_Driver dht(DHT_PIN);                       // 🔵 REPLACED DHT LIB

int seq_id = 0;

void log(String msg) {
  Serial.print("[NODE ");
  Serial.print(NODE_ID);
  Serial.print("] ");
  Serial.println(msg);
}

float readBattery() {
  int raw = analogRead(BAT_PIN);
  return (raw / 1023.0) * 5.0 * 2.0;
}

void setup() {
  Serial.begin(9600);

  servo.attach(SERVO_PIN);
  servo.write(30);

  ultrasonic.begin();     // 🔵 NEW
  dht.begin();            // 🔵 NEW

  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa Failed!");
    while (1);
  }

  // SF LoRA Communication
  LoRa.setSpreadingFactor(10);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.enableCrc();

  log("Node Ready");
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (!packetSize) return;

  String msg = "";
  while (LoRa.available()) msg += (char)LoRa.read();

  if (msg == "CMD,SCAN," + String(NODE_ID)) {
    runScan();
  }
}

void runScan() {
  seq_id++;

  float battery = readBattery();

  int temp = -1, hum = -1;
  if (!dht.read(temp, hum)) {
    log("DHT Failed");
  }

  int gas = analogRead(GAS_PIN);

  String payload = String(seq_id) + ",";
  payload += String(temp) + ",";
  payload += String(gas) + ",";
  payload += String(battery) + ",";

  for (int angle = 30; angle <= 150; angle += 30) {
    servo.write(angle);
    delay(300);

    int dist = ultrasonic.readDistance(); 
    payload += String(angle) + ":" + String(dist);
    if (angle < 150) payload += "|";
  }

  sendWithRetry(payload);
}

void sendWithRetry(String data) {
  for (int i = 0; i < MAX_RETRY; i++) {

    LoRa.beginPacket();
    LoRa.print("NODE,");
    LoRa.print(NODE_ID);
    LoRa.print(",");
    LoRa.print(PROTOCOL);
    LoRa.print(",");
    LoRa.print(data);
    LoRa.endPacket();

    unsigned long start = millis();

    while (millis() - start < ACK_TIMEOUT) {
      int p = LoRa.parsePacket();
      if (!p) continue;

      String r = "";
      while (LoRa.available()) r += (char)LoRa.read();

      if (r == "ACKv1," + String(NODE_ID) + "," + String(seq_id)) {
        log("ACK OK");
        return;
      }
    }
  }

  log("ACK FAILED");
}
