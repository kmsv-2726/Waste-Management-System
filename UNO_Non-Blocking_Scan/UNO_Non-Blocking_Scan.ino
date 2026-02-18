#include <SPI.h>
#include <LoRa.h>
#include <Servo.h>
#include "UltrasonicDriver.h"
#include "DHT11_Driver.h"

/* ---------- Node ---------- */
#define NODE_ID 2
#define PROTOCOL "SCANv2"

/* ---------- LoRa ---------- */
#define LORA_SS   10
#define LORA_RST  9
#define LORA_DIO0 2   // INT0

/* ---------- Sensors ---------- */
#define GAS_PIN       3   // INT1 (Digital interrupt)
#define GAS_ANALOG    A0  // Raw gas value
#define DHT_PIN       7
#define BAT_PIN       A1

#define TRIG_PIN      4
#define ECHO_PIN      8
#define SERVO_PIN     6

#define TEMP_THRESHOLD 40
#define ULTRA_THRESHOLD 10
#define LOW_BATTERY 3.5

#define SERVO_INTERVAL 300

/* ---------- Interrupt Flags ---------- */
volatile bool gasTriggered = false;
volatile bool loraFlag = false;

/* ---------- Objects ---------- */
Servo servo;
UltrasonicDriver ultrasonic(TRIG_PIN, ECHO_PIN);
DHT11_Driver dht(DHT_PIN);

/* ---------- Scan State ---------- */
enum ScanState { SCAN_IDLE, SCAN_ACTIVE };
ScanState scanState = SCAN_IDLE;

int currentAngle = 30;
unsigned long lastMoveTime = 0;
String scanPayload = "";
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
  LoRa.enableCrc();

  attachInterrupt(digitalPinToInterrupt(LORA_DIO0), loraISR, RISING);

  Serial.println("[NODE READY]");
}

/* ---------- Loop ---------- */
void loop() {

  if (gasTriggered) {
    gasTriggered = false;
    sendAlert("GAS");
  }

  if (loraFlag) {
    loraFlag = false;
    handleLoRaPacket();
  }

  handleScan();
}

/* ---------- LoRa ---------- */
void handleLoRaPacket() {
  int packetSize = LoRa.parsePacket();
  if (!packetSize) return;

  String msg="";
  while(LoRa.available()) msg+=(char)LoRa.read();

  if (msg == "CMD,SCAN," + String(NODE_ID)) {
    startScan();
  }
}

/* ---------- Start Scan ---------- */
void startScan() {

  if (scanState != SCAN_IDLE) return;

  seq_id++;
  currentAngle = 30;
  scanPayload = "";

  scanState = SCAN_ACTIVE;
  lastMoveTime = millis();

  Serial.println("[SCAN STARTED]");
}

/* ---------- Handle Scan ---------- */
void handleScan() {

  if (scanState != SCAN_ACTIVE) return;

  if (millis() - lastMoveTime >= SERVO_INTERVAL) {
    lastMoveTime = millis();
    servo.write(currentAngle);
    ultrasonic.trigger();
  }

  if (ultrasonic.isReady()) {

    int dist = ultrasonic.getDistance();

    scanPayload += String(currentAngle) + ":" + String(dist);

    if (currentAngle < 150)
      scanPayload += "|";

    if (dist > 0 && dist < ULTRA_THRESHOLD)
      sendAlert("OVERFLOW");

    currentAngle += 30;

    if (currentAngle > 150)
      finishScan();
  }
}

/* ---------- Finish Scan (ML PACKET) ---------- */
void finishScan() {

  int temperature, humidity;
  dht.read(temperature, humidity);

  int gasValue = analogRead(GAS_ANALOG);
  float battery = readBattery();

  LoRa.beginPacket();
  LoRa.print("NODE,");
  LoRa.print(NODE_ID);
  LoRa.print(",");
  LoRa.print(PROTOCOL);
  LoRa.print(",");
  LoRa.print(seq_id);
  LoRa.print(",");
  LoRa.print(temperature);
  LoRa.print(",");
  LoRa.print(humidity);
  LoRa.print(",");
  LoRa.print(gasValue);
  LoRa.print(",");
  LoRa.print(battery, 2);
  LoRa.print(",");
  LoRa.print(scanPayload);
  LoRa.endPacket();

  scanState = SCAN_IDLE;

  Serial.println("[SCAN COMPLETE - ML DATA SENT]");
}

/* ---------- Alert ---------- */
void sendAlert(String type) {
  LoRa.beginPacket();
  LoRa.print("NODE,");
  LoRa.print(NODE_ID);
  LoRa.print(",ALERTv1,");
  LoRa.print(type);
  LoRa.endPacket();
}

/* ---------- Battery ---------- */
float readBattery() {
  int raw = analogRead(BAT_PIN);
  return (raw / 1023.0) * 5.0 * 2.0;
}
