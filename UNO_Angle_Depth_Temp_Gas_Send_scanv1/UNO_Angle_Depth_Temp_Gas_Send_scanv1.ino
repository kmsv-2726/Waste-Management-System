#include <SPI.h>
#include <LoRa.h>
#include <Servo.h>
#include <DHT.h>

/* ---------- CONFIG ---------- */
#define NODE_ID 2
#define PROTOCOL "SCANv1"

/* ---------- LoRa ---------- */
#define LORA_SS   10
#define LORA_RST  9
#define LORA_DIO0 2

/* ---------- Pins ---------- */
#define SERVO_PIN 6
#define TRIG_PIN  4
#define ECHO_PIN  3
#define GAS_PIN   A0
#define BAT_PIN   A1
#define DHT_PIN   7
#define DHT_TYPE  DHT11

#define LOW_BATTERY 3.5
#define ACK_TIMEOUT 3000
#define MAX_RETRY   3

Servo servo;
DHT dht(DHT_PIN, DHT_TYPE);

int seq_id = 0;
bool waitingACK = false;
unsigned long ackTimer;

/* ---------- Utils ---------- */
void log(String msg) {
  Serial.print("[NODE ");
  Serial.print(NODE_ID);
  Serial.print("] ");
  Serial.println(msg);
}

float readBattery() {
  int raw = analogRead(BAT_PIN);
  float v = (raw / 1023.0) * 5.0 * 2.0; // divider
  return v;
}

int readUltrasonic() {
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long d = pulseIn(ECHO_PIN, HIGH, 30000);
  return d > 0 ? d / 58 : -1;
}

/* ---------- Setup ---------- */
void setup() {
  Serial.begin(9600);

  servo.attach(SERVO_PIN);
  servo.write(30);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  dht.begin();

  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) while (1);

  log("Node Ready");
}

/* ---------- Loop ---------- */
void loop() {
  int p = LoRa.parsePacket();
  if (!p) return;

  String msg = "";
  while (LoRa.available()) msg += (char)LoRa.read();

  if (msg == "CMD,SCAN," + String(NODE_ID)) {
    runScan();
  }

  if (msg == "ACKv1," + String(NODE_ID) + "," + String(seq_id)) {
    waitingACK = false;
    log("ACK received");
  }
}

/* ---------- Scan ---------- */
void runScan() {
  seq_id++;

  float battery = readBattery();
  if (battery < LOW_BATTERY) {
    LoRa.beginPacket();
    LoRa.print("NODE,");
    LoRa.print(NODE_ID);
    LoRa.print(",ALERTv1,LOW_BATTERY,");
    LoRa.print(battery);
    LoRa.endPacket();
    log("LOW BATTERY ALERT SENT");
  }

  int temp = dht.readTemperature();
  if (temp == 0 || isnan(temp)) {
    delay(1000);
    temp = dht.readTemperature();
  }

  int gas = analogRead(GAS_PIN);

  String payload = String(seq_id) + ",";
  payload += String(temp) + ",";
  payload += String(gas) + ",";
  payload += String(battery) + ",";

  for (int angle = 30; angle <= 150; angle += 10) {
    servo.write(angle);
    delay(300);

    int dist = readUltrasonic();
    log("Angle " + String(angle));

    payload += String(angle) + ":" + String(dist);
    if (angle < 150) payload += "|";
  }

  sendWithRetry(payload);
}

/* ---------- Send + Retry ---------- */
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

    waitingACK = true;
    ackTimer = millis();

    while (waitingACK && millis() - ackTimer < ACK_TIMEOUT) {
      int p = LoRa.parsePacket();
      if (!p) continue;

      String r = "";
      while (LoRa.available()) r += (char)LoRa.read();
      if (r == "ACKv1," + String(NODE_ID) + "," + String(seq_id)) {
        waitingACK = false;
        log("ACK OK");
        return;
      }
    }
  }

  log("ACK FAILED");
}
