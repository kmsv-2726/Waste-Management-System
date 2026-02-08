#include <SPI.h>
#include <LoRa.h>

#define LORA_SS   5
#define LORA_RST  27
#define LORA_DIO0 26

unsigned long startTime;
const int SLOT_DURATION = 10; // seconds
const int TOTAL_NODES = 2;

void setup() {
  Serial.begin(115200);
  delay(1000);

  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa failed");
    while (1);
  }

  startTime = millis();
  Serial.println("ESP32 Global Clock Started");
}

void loop() {
  unsigned long currentSeconds = (millis() - startTime) / 1000;
  int currentSlot = (currentSeconds / SLOT_DURATION) % TOTAL_NODES;
  int activeNode = currentSlot + 1; // Node IDs: 1,2

  // Broadcast slot info
  LoRa.beginPacket();
  LoRa.print("SYNC,");
  LoRa.print(currentSeconds);
  LoRa.print(",");
  LoRa.print(activeNode);
  LoRa.endPacket();

  Serial.print("Time: ");
  Serial.print(currentSeconds);
  Serial.print(" | Active Node: ");
  Serial.println(activeNode);

  // Listen for node response
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String msg = "";
    while (LoRa.available()) msg += (char)LoRa.read();
    Serial.print("Received: ");
    Serial.println(msg);
  }

  delay(1000);
}
