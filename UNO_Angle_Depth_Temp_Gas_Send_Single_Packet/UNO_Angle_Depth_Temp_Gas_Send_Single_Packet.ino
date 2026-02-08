#include <SPI.h>
#include <LoRa.h>
#include <Servo.h>
#include <DHT.h>

/* ---------- Node Identity ---------- */
#define NODE_ID 1

/* ---------- LoRa Pins ---------- */
#define LORA_SS   10
#define LORA_RST  9
#define LORA_DIO0 2

/* ---------- Sensors ---------- */
#define SERVO_PIN 6
#define TRIG_PIN 4
#define ECHO_PIN 3
#define GAS_PIN A0
#define DHT_PIN 7
#define DHT_TYPE DHT11

Servo servo;
DHT dht(DHT_PIN, DHT_TYPE);

bool scanning = false;
String payload = "";

void setup() {
  Serial.begin(9600);

  servo.attach(SERVO_PIN);
  servo.write(30);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  dht.begin();

  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) while (1);

  Serial.println("Node Ready");
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (!packetSize) return;

  String msg = "";
  while (LoRa.available()) msg += (char)LoRa.read();

  if (msg == "CMD,SCAN," + String(NODE_ID) && !scanning) {
    runScan();
  }
}

/* ---------- Main Scan Logic ---------- */
void runScan() {
  scanning = true;
  payload = "";
  /* ---- Scan-level sensors (ONCE) ---- */
  int temp = (int)dht.readTemperature();
  int gas  = analogRead(GAS_PIN);

  payload += String(temp);
  payload += ",";
  payload += String(gas);
  payload += ",";

  /* ---- Angle sweep ---- */
  for (int angle = 30; angle <= 150; angle += 10) {
    servo.write(angle);
    delay(300);  // servo + ultrasonic settle

    int distance = readUltrasonic();

    payload += String(angle);
    payload += ":";
    payload += String(distance);

    if (angle < 150) payload += "|";
  }

  /* ---- Single LoRa packet ---- */
  LoRa.beginPacket();
  LoRa.print("NODE,");
  LoRa.print(NODE_ID);
  LoRa.print(",SCAN,");
  LoRa.print(payload);
  LoRa.endPacket();

  scanning = false;
}

/* ---------- Ultrasonic ---------- */
int readUltrasonic() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  return duration > 0 ? duration / 58 : -1;
}
