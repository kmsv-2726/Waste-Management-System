#include <SPI.h>
#include <LoRa.h>
#include <Servo.h>
#include "UltrasonicDriver.h"
#include "DHT11_Driver.h"

/* ---------- Node ---------- */
#define NODE_ID 2
#define PROTOCOL "SCANv1"

/* ---------- LoRa ---------- */
#define LORA_SS   10
#define LORA_RST  9
#define LORA_DIO0 2   // INT0

/* ---------- Sensors ---------- */
#define GAS_PIN   3   // INT1
#define DHT_PIN   7
#define BAT_PIN   A1

#define TRIG_PIN  4
#define ECHO_PIN  5
#define SERVO_PIN 6

#define TEMP_THRESHOLD 40
#define ULTRA_THRESHOLD 10
#define LOW_BATTERY 3.5

#define ACK_TIMEOUT 3000

volatile bool gasTriggered = false;
volatile bool loraFlag = false;

Servo servo;
UltrasonicDriver ultrasonic(TRIG_PIN, ECHO_PIN);
DHT11_Driver dht(DHT_PIN);

int seq_id = 0;

/* ---------- ISRs ---------- */
void gasISR() {
  gasTriggered = true;
}

void loraISR() {
  loraFlag = true;
}

/* ---------- Setup ---------- */
void setup() {
  Serial.begin(9600);

  servo.attach(SERVO_PIN);
  servo.write(30);

  ultrasonic.begin();
  dht.begin();

  pinMode(GAS_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(GAS_PIN), gasISR, RISING);

  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) while (1);

  LoRa.setSpreadingFactor(10);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.enableCrc();

  attachInterrupt(digitalPinToInterrupt(LORA_DIO0), loraISR, RISING);

  Serial.println("[NODE READY]");
}

/* ---------- Loop ---------- */
void loop() {

  /* 🔴 Gas Interrupt */
  if (gasTriggered) {
    gasTriggered = false;
    sendAlert("GAS");
  }

  /* 🔴 LoRa RX Interrupt */
  if (loraFlag) {
    loraFlag = false;
    handleLoRaPacket();
  }

  /* 🔵 Temperature Event */
  int temp=-1, hum=-1;
  if (dht.read(temp, hum)) {
    if (temp > TEMP_THRESHOLD) {
      sendAlert("HIGHTEMP");
    }
  }

  /* 🔵 Battery Check */
  float battery = readBattery();
  if (battery < LOW_BATTERY) {
    sendAlert("LOWBAT");
  }
}

/* ---------- Handle LoRa Packet ---------- */
void handleLoRaPacket() {

  int packetSize = LoRa.parsePacket();
  if (!packetSize) return;

  String msg="";
  while(LoRa.available()) msg+=(char)LoRa.read();

  if (msg == "CMD,SCAN," + String(NODE_ID)) {
    runScan();
  }

  /* ACK Handling */
  if (msg.startsWith("ACKv1")) {
    Serial.println("[ACK RECEIVED]");
  }
}

/* ---------- Alert Sender ---------- */
void sendAlert(String type) {
  LoRa.beginPacket();
  LoRa.print("NODE,");
  LoRa.print(NODE_ID);
  LoRa.print(",ALERTv1,");
  LoRa.print(type);
  LoRa.endPacket();
}

/* ---------- Scan With Servo + Ultrasonic ---------- */
void runScan() {

  seq_id++;

  int temp=-1, hum=-1;
  dht.read(temp, hum);

  String payload = String(seq_id) + ",";
  payload += String(temp) + ",";

  for (int angle = 30; angle <= 150; angle += 30) {
    servo.write(angle);
    delay(300);

    int dist = ultrasonic.readDistance();
    payload += String(angle) + ":" + String(dist);

    if (angle < 150) payload += "|";

    if (dist > 0 && dist < ULTRA_THRESHOLD) {
      sendAlert("OVERFLOW");
    }
  }

  LoRa.beginPacket();
  LoRa.print("NODE,");
  LoRa.print(NODE_ID);
  LoRa.print(",");
  LoRa.print(PROTOCOL);
  LoRa.print(",");
  LoRa.print(payload);
  LoRa.endPacket();
}

/* ---------- Battery ---------- */
float readBattery() {
  int raw = analogRead(BAT_PIN);
  return (raw / 1023.0) * 5.0 * 2.0;
}
