#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <time.h>

/* ---------- LoRa ---------- */
#define LORA_SS   5
#define LORA_RST  27
#define LORA_DIO0 26

/* ---------- WiFi / NTP ---------- */
const char* ssid = "CSE23728";
const char* password = "Mani@123";

/* ---------- System ---------- */
#define TOTAL_NODES 2
#define SCAN_TIMEOUT_MS 30000

enum State { IDLE, SCANNING };
State systemState = IDLE;

/* ---------- Queues ---------- */
int manualQueue[10], emergencyQueue[10];
int manualHead = 0, manualTail = 0;
int emergencyHead = 0, emergencyTail = 0;

/* ---------- Scan tracking ---------- */
int currentNode = -1;
unsigned long lastRxTime = 0;
bool hourlyScanDoneThisHour = false;

/* ---------- Queue helpers ---------- */
bool queueEmpty(int h, int t) { return h == t; }
void queuePush(int q[], int &t, int v) { q[t] = v; t = (t + 1) % 10; }
int queuePop(int q[], int &h) { int v = q[h]; h = (h + 1) % 10; return v; }

void setup() {
  Serial.begin(115200);

  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) while (1);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  configTime(0, 0, "pool.ntp.org");

  Serial.println("[SYS] ESP32 Master Ready (NTP)");
}

void loop() {
  handleSerial();
  handleHourlyScheduler();
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
  systemState = SCANNING;
  lastRxTime = millis();

  Serial.print("[EXEC] Scanning Node ");
  Serial.println(node);

  LoRa.beginPacket();
  LoRa.print("CMD,SCAN,");
  LoRa.print(node);
  LoRa.endPacket();
}

/* ---------- Timeout ---------- */
void checkTimeout() {
  if (millis() - lastRxTime > SCAN_TIMEOUT_MS) {
    Serial.print("[FAIL] Node ");
    Serial.print(currentNode);
    Serial.println(" timeout");
    systemState = IDLE;
    currentNode = -1;
  }
}

/* ---------- Manual Override ---------- */
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

/* ---------- Hourly Scheduler ---------- */
void handleHourlyScheduler() {
  struct tm t;
  if (!getLocalTime(&t)) return;

  if (t.tm_min == 0 && !hourlyScanDoneThisHour) {
    for (int i = 1; i <= TOTAL_NODES; i++)
      queuePush(manualQueue, manualTail, i);

    hourlyScanDoneThisHour = true;
    Serial.println("[SCHED] Hourly scan queued");
    printQueues();
  }

  if (t.tm_min != 0) hourlyScanDoneThisHour = false;
}

/* ---------- LoRa RX + Parsing ---------- */
void handleLoRaRx() {
  int p = LoRa.parsePacket();
  if (!p) return;

  String msg = "";
  while (LoRa.available()) msg += (char)LoRa.read();
  lastRxTime = millis();

  Serial.print("[RX] ");
  Serial.println(msg);

  if (msg.startsWith("NODE," + String(currentNode) + ",SCAN")) {
    parseScan(msg);
    systemState = IDLE;
    currentNode = -1;
    printQueues();
  }
}

/* ---------- Scan Parsing ---------- */
void parseScan(String msg) {
  // NODE,id,SCAN,temp,gas,angle:data|...
  int idx1 = msg.indexOf(',', 10);
  int idx2 = msg.indexOf(',', idx1 + 1);
  int idx3 = msg.indexOf(',', idx2 + 1);
  int idx4 = msg.indexOf(',', idx3 + 1);

  int temp = msg.substring(idx3 + 1, idx4).toInt();
  int gas  = msg.substring(idx4 + 1, msg.indexOf(',', idx4 + 1)).toInt();

  Serial.print("[DATA] Temp=");
  Serial.print(temp);
  Serial.print(" Gas=");
  Serial.println(gas);

  String angles = msg.substring(msg.lastIndexOf(',') + 1);
  Serial.print("[DATA] Angles: ");
  Serial.println(angles);
}

/* ---------- Queue Display ---------- */
void printQueues() {
  Serial.print("[QUEUE] Emergency: [ ");
  int i = emergencyHead;
  while (i != emergencyTail) {
    Serial.print(emergencyQueue[i]); Serial.print(" ");
    i = (i + 1) % 10;
  }
  Serial.println("]");

  Serial.print("[QUEUE] Manual   : [ ");
  i = manualHead;
  while (i != manualTail) {
    Serial.print(manualQueue[i]); Serial.print(" ");
    i = (i + 1) % 10;
  }
  Serial.println("]");
}
