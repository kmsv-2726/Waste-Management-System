#include <SPI.h>
#include <LoRa.h>
#include <Servo.h>
#include <EEPROM.h>

#include "UltrasonicDriver.h"
#include "DHT11_Driver.h"
#include "EEPROMLogger.h"

/* ---------- NODE CONFIG ---------- */
#define NODE_ID 2
#define PROTOCOL "SCANv2"

/* ---------- LoRa ---------- */
#define LORA_SS   10
#define LORA_RST  9
#define LORA_DIO0 2   // INT0

/* ---------- Pins ---------- */
#define GAS_PIN       3     // INT1
#define GAS_ANALOG    A0
#define DHT_PIN       7
#define BAT_PIN       A1

#define TRIG_PIN      4
#define ECHO_PIN      8     // PCINT
#define SERVO_PIN     6

/* ---------- Thresholds ---------- */
#define TEMP_THRESHOLD 40
#define ULTRA_THRESHOLD 10
#define LOW_BATTERY 3.5

#define SERVO_INTERVAL 300
#define SCAN_FLAG_ADDR 500

/* ---------- Globals ---------- */
volatile bool gasTriggered = false;
volatile bool loraFlag = false;

Servo servo;
UltrasonicDriver ultrasonic(TRIG_PIN, ECHO_PIN);
DHT11_Driver dht(DHT_PIN);
EEPROMLogger logger;

enum ScanState { SCAN_IDLE, SCAN_ACTIVE };
ScanState scanState = SCAN_IDLE;

int currentAngle = 30;
unsigned long lastMoveTime = 0;
String scanPayload = "";
int seq_id = 0;

bool waitingAck = false;
unsigned long sendTime;
int lastSentIndex = -1;

/* ---------- ISRs ---------- */
void gasISR() { gasTriggered = true; }
void loraISR() { loraFlag = true; }

/* ---------- Setup ---------- */
void setup() {

  Serial.begin(9600);

  servo.attach(SERVO_PIN);
  servo.write(30);

  ultrasonic.begin();
  dht.begin();
  logger.begin();

  pinMode(GAS_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(GAS_PIN), gasISR, RISING);

  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) while (1);

  LoRa.setSpreadingFactor(10);
  LoRa.enableCrc();
  attachInterrupt(digitalPinToInterrupt(LORA_DIO0), loraISR, RISING);

  /* --- Power failure scan recovery --- */
  if (EEPROM.read(SCAN_FLAG_ADDR) == 1) {
    Serial.println("Previous scan interrupted by power loss.");
    EEPROM.write(SCAN_FLAG_ADDR, 0);
  }

  Serial.println("NODE READY");
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
  handleTimeoutRetry();
}

/* ---------- LoRa RX ---------- */
void handleLoRaPacket() {

  int packetSize = LoRa.parsePacket();
  if (!packetSize) return;

  String msg="";
  while(LoRa.available()) msg+=(char)LoRa.read();

  if (msg == "CMD,SCAN," + String(NODE_ID)) {
    startScan();
  }

  if (msg.startsWith("ACK")) {
    logger.markDelivered(lastSentIndex);
    waitingAck = false;
  }
}

/* ---------- Scan Control ---------- */
void startScan() {

  if (scanState != SCAN_IDLE) return;

  EEPROM.write(SCAN_FLAG_ADDR, 1);  // mark scan active

  seq_id++;
  currentAngle = 30;
  scanPayload = "";

  scanState = SCAN_ACTIVE;
  lastMoveTime = millis();
}

/* ---------- Scan State Machine ---------- */
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

/* ---------- Finish Scan ---------- */
void finishScan() {

  EEPROM.write(SCAN_FLAG_ADDR, 0);  // mark scan complete

  int temperature, humidity;
  if (!dht.read(temperature, humidity)) {
    temperature = -1;
    humidity = -1;
  }

  int gasValue = analogRead(GAS_ANALOG);
  float battery = readBattery();

  String packet =
    "NODE," + String(NODE_ID) + "," +
    PROTOCOL + "," +
    String(seq_id) + "," +
    String(temperature) + "," +
    String(humidity) + "," +
    String(gasValue) + "," +
    String(battery,2) + "," +
    scanPayload;

  logger.saveRecord(packet, seq_id);

  int index = seq_id % MAX_RECORDS;

  LoRa.beginPacket();
  LoRa.print(packet);
  LoRa.endPacket();

  waitingAck = true;
  sendTime = millis();
  lastSentIndex = index;

  scanState = SCAN_IDLE;
}

/* ---------- Retry Logic ---------- */
void handleTimeoutRetry() {

  if (waitingAck && millis() - sendTime > 5000) {

    String data;
    if (logger.getRecord(lastSentIndex, data)) {

      LoRa.beginPacket();
      LoRa.print(data);
      LoRa.endPacket();

      sendTime = millis();
    }
  }
}

/* ---------- Alerts ---------- */
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