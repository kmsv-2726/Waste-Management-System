#include <SPI.h>
#include <LoRa.h>
#include <Servo.h>

#define NODE_ID 1

#define LORA_SS   10
#define LORA_RST  9
#define LORA_DIO0 2

#define SERVO_PIN 6

Servo servo;
bool scanning = false;
int adaptiveDelay = 5000;   // default safe delay

void setup() {
  Serial.begin(9600);
  servo.attach(SERVO_PIN);
  servo.write(30);

  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) while (1);

  Serial.println("Node Ready");
}

void loop() {
  int p = LoRa.parsePacket();
  if (!p) return;

  String msg = "";
  while (LoRa.available()) msg += (char)LoRa.read();

  if (msg == "CMD,SCAN," + String(NODE_ID) && !scanning) {
    runScan();
  }

  if (msg.startsWith("ACK,DELAY")) {
    adaptiveDelay = msg.substring(10).toInt();
    Serial.print("New delay: ");
    Serial.println(adaptiveDelay);
  }
}

/* ---------- Scan ---------- */
void runScan() {
  scanning = true;

  for (int a = 30; a <= 150; a += 10) {
    servo.write(a);
    delay(adaptiveDelay);
    sendAngle(a);
  }

  for (int a = 150; a >= 30; a -= 10) {
    servo.write(a);
    delay(15);
  }

  LoRa.beginPacket();
  LoRa.print("NODE,");
  LoRa.print(NODE_ID);
  LoRa.print(",SCAN_DONE");
  LoRa.endPacket();

  scanning = false;
}

/* ---------- Data ---------- */
void sendAngle(int angle) {
  LoRa.beginPacket();
  LoRa.print("NODE,");
  LoRa.print(NODE_ID);
  LoRa.print(",ANGLE,");
  LoRa.print(angle);
  LoRa.endPacket();
}
