#include <SPI.h>
#include <LoRa.h>

#define NODE_ID 1   // 👈 CHANGE to 2 for second Arduino

#define LORA_SS   10
#define LORA_RST  9
#define LORA_DIO0 2

void setup() {
  Serial.begin(9600);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa failed");
    while (1);
  }

  randomSeed(analogRead(A0));
  Serial.print("Node ");
  Serial.print(NODE_ID);
  Serial.println(" ready");
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (!packetSize) return;

  String msg = "";
  while (LoRa.available()) msg += (char)LoRa.read();

  // Expected: SYNC,time,node
  if (!msg.startsWith("SYNC")) return;

  int firstComma = msg.indexOf(',');
  int secondComma = msg.indexOf(',', firstComma + 1);

  unsigned long globalTime =
      msg.substring(firstComma + 1, secondComma).toInt();
  int allowedNode =
      msg.substring(secondComma + 1).toInt();

  if (allowedNode == NODE_ID) {
    int value = random(0, 101);

    Serial.print("My slot! Time: ");
    Serial.print(globalTime);
    Serial.print(" Sending: ");
    Serial.println(value);

    LoRa.beginPacket();
    LoRa.print("NODE,");
    LoRa.print(NODE_ID);
    LoRa.print(",VALUE,");
    LoRa.print(value);
    LoRa.endPacket();
  }
}
