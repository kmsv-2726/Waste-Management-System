#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <time.h>

/* ---------- LoRa Pins ---------- */
#define LORA_SS   5
#define LORA_RST  27
#define LORA_DIO0 26

/* ---------- Config ---------- */
#define TOTAL_NODES 2
#define SCAN_TIMEOUT_MS 20000
#define MAX_RETRIES 2

/* ---------- WiFi ---------- */
const char* ssid = "CSE23728";
const char* password = "Mani@123";

/* ---------- State ---------- */
enum State { IDLE, SCANNING };
State systemState = IDLE;

/* ---------- Queues ---------- */
int manualQueue[10], emergencyQueue[10];
int manualHead = 0, manualTail = 0;
int emergencyHead = 0, emergencyTail = 0;

/* ---------- Scan tracking ---------- */
int currentNode = -1;
unsigned long scanStartTime = 0;
int retryCount = 0;

/* ---------- Queue helpers ---------- */
bool queueEmpty(int h, int t) { return h == t; }
void queuePush(int q[], int &t, int v) { q[t] = v; t = (t + 1) % 10; }
int queuePop(int q[], int &h) { int v = q[h]; h = (h + 1) % 10; return v; }

/* ---------- Setup ---------- */
void setup() {
  Serial.begin(115200);
  delay(1000);

  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) while (1);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  configTime(0, 0, "pool.ntp.org");

  Serial.println("[SYS] ESP32 Master Ready");
}

/* ---------- Loop ---------- */
void loop() {
  handleSerial();
  handleLoRaRx();

  if (systemState == SCANNING) checkTimeout();
  if (systemState == IDLE) scheduler();
}

/* ---------- Scheduler ---------- */
void scheduler() {
  if (!queueEmpty(emergencyHead, emergencyTail)) {
    startScan(queuePop(emergencyQueue, emergencyHead));
  }
  else if (!queueEmpty(manualHead, manualTail)) {
    startScan(queuePop(manualQueue, manualHead));
  }
}

/* ---------- Start Scan ---------- */
void startScan(int node) {
  currentNode = node;
  retryCount = 0;
  systemState = SCANNING;
  sendScanCommand();
}

/* ---------- Send Scan ---------- */
void sendScanCommand() {
  scanStartTime = millis();

  Serial.print("[EXEC] Scanning Node ");
  Serial.print(currentNode);
  Serial.print(" (attempt ");
  Serial.print(retryCount + 1);
  Serial.println(")");

  LoRa.beginPacket();
  LoRa.print("CMD,SCAN,");
  LoRa.print(currentNode);
  LoRa.endPacket();
}

/* ---------- Timeout ---------- */
void checkTimeout() {
  if (millis() - scanStartTime > SCAN_TIMEOUT_MS) {
    if (retryCount < MAX_RETRIES) {
      retryCount++;
      Serial.println("[RETRY] Scan retry");
      sendScanCommand();
    } else {
      Serial.println("[FAIL] Node unresponsive");
      systemState = IDLE;
    }
  }
}

/* ---------- Serial ---------- */
void handleSerial() {
  if (!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim(); cmd.toUpperCase();

  if (cmd.startsWith("SCAN")) {
    int node = cmd.substring(5).toInt();
    if (node >= 1 && node <= TOTAL_NODES) {
      queuePush(manualQueue, manualTail, node);
      Serial.print("[MANUAL] Queued Node ");
      Serial.println(node);
      printQueues();
    }
  }
}

/* ---------- LoRa RX ---------- */
void handleLoRaRx() {
  int p = LoRa.parsePacket();
  if (!p) return;

  String msg = "";
  while (LoRa.available()) msg += (char)LoRa.read();

  int rssi = LoRa.packetRssi();
  Serial.print("[RX] ");
  Serial.print(msg);
  Serial.print(" | RSSI=");
  Serial.println(rssi);

  if (msg.startsWith("NODE," + String(currentNode) + ",ANGLE")) {
    sendAdaptiveDelay(rssi);
  }

  if (msg == "NODE," + String(currentNode) + ",SCAN_DONE") {
    sendAck();
    Serial.println("[EXEC] Scan complete");
    systemState = IDLE;
    printQueues();
  }

  if (msg.startsWith("EMERGENCY")) {
    int node = msg.substring(10).toInt();
    queuePush(emergencyQueue, emergencyTail, node);
    Serial.print("[EMERG] Node ");
    Serial.println(node);
  }
}

/* ---------- Adaptive Delay ---------- */
void sendAdaptiveDelay(int rssi) {
  int delayMs;

  if (rssi > -70) delayMs = 1000;
  else if (rssi > -85) delayMs = 3000;
  else delayMs = 6000;

  LoRa.beginPacket();
  LoRa.print("ACK,DELAY,");
  LoRa.print(delayMs);
  LoRa.endPacket();

  Serial.print("[CTRL] Delay set to ");
  Serial.print(delayMs);
  Serial.println(" ms");
}

/* ---------- ACK ---------- */
void sendAck() {
  LoRa.beginPacket();
  LoRa.print("ACK,");
  LoRa.print(currentNode);
  LoRa.endPacket();
}

/* ---------- Queue Print ---------- */
void printQueues() {
  Serial.print("[QUEUE] Emergency: [ ");
  int i = emergencyHead;
  while (i != emergencyTail) {
    Serial.print(emergencyQueue[i]);
    Serial.print(" ");
    i = (i + 1) % 10;
  }
  Serial.println("]");

  Serial.print("[QUEUE] Manual   : [ ");
  i = manualHead;
  while (i != manualTail) {
    Serial.print(manualQueue[i]);
    Serial.print(" ");
    i = (i + 1) % 10;
  }
  Serial.println("]");
}
