#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <time.h>

/* ---------- LoRa ---------- */
#define LORA_SS   5
#define LORA_RST  27
#define LORA_DIO0 26

#define LORA_FREQ 433E6
#define SPREADING_FACTOR 10 //Gives Moderate Range, Moderate Airtime
#define BANDWIDTH 125E3
#define CODING_RATE 5

#define SCAN_TIMEOUT 60000

/* ---------- WiFi ---------- */
const char* ssid = "CSE23728";
const char* password = "Mani@123";

/* ---------- State Machine ---------- */
enum State { IDLE, SCANNING, WAITING_INTERRUPT };
State state = IDLE;

/* ---------- Queues ---------- */
int manualQ[10], emergencyQ[10];
int mh=0, mt=0, eh=0, et=0;

int currentNode = -1;
unsigned long interruptStartTime = 0;
int lastScheduledHour = -1;

volatile bool loraFlag = false;

/* ---------- Queue Utils ---------- */
bool empty(int h,int t){return h==t;}
void push(int q[],int &t,int v){q[t]=v; t=(t+1)%10;}
int pop(int q[],int &h){int v=q[h]; h=(h+1)%10; return v;}

/* ---------- ISR ---------- */
void IRAM_ATTR loraISR() {
  loraFlag = true;
}

/* ---------- Setup ---------- */
void setup() {
  Serial.begin(115200);
  Serial.println("[SYS] Booting Gateway");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);

  configTime(0,0,"pool.ntp.org");

  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_FREQ)) while(1);

  /* ---------- Explicit PHY Config ---------- */
  LoRa.setSpreadingFactor(SPREADING_FACTOR);
  LoRa.setSignalBandwidth(BANDWIDTH);
  LoRa.setCodingRate4(CODING_RATE);
  LoRa.enableCrc();

  attachInterrupt(digitalPinToInterrupt(LORA_DIO0), loraISR, RISING);

  Serial.println("[SYS] Gateway Ready");
}

/* ---------- Loop ---------- */
void loop() {

  handleSerial();

  if (loraFlag) {
    loraFlag = false;
    handleLoRaPacket();
  }

  if (state == IDLE)
    scheduler();

  hourScheduler();

  /* Timeout for interrupt result */
  if (state == WAITING_INTERRUPT) {
    if (millis() - interruptStartTime > SCAN_TIMEOUT) {
      Serial.println("[TIMEOUT] Interrupt scan not received");
      state = IDLE;
    }
  }
}

/* ---------- Hourly Scheduler ---------- */
void hourScheduler() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  if (timeinfo.tm_hour != lastScheduledHour) {
    lastScheduledHour = timeinfo.tm_hour;

    Serial.println("[SCHEDULER] Hourly scan triggered");
    push(manualQ, mt, 1);   // Auto-scan node 1
  }
}

/* ---------- Priority Scheduler ---------- */
void scheduler() {

  if (!empty(eh,et)) {
    startScan(pop(emergencyQ,eh));
    return;
  }

  if (!empty(mh,mt)) {
    startScan(pop(manualQ,mh));
    return;
  }
}

/* ---------- Start Scan ---------- */
void startScan(int node) {

  currentNode = node;
  state = SCANNING;

  Serial.println("[EXEC] Scanning Node " + String(node));

  LoRa.beginPacket();
  LoRa.print("CMD,SCAN,");
  LoRa.print(node);
  LoRa.endPacket();
}

/* ---------- Manual Override ---------- */
void handleSerial() {

  if (!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim(); cmd.toUpperCase();

  if (cmd.startsWith("SCAN")) {
    int node = cmd.substring(5).toInt();
    push(manualQ, mt, node);
    Serial.println("[MANUAL] Scan queued");
  }

  if (cmd.startsWith("EMERGENCY")) {
    int node = cmd.substring(10).toInt();
    push(emergencyQ, et, node);
    Serial.println("[EMERGENCY] Priority scan queued");
  }
}

/* ---------- LoRa Packet Handler ---------- */
void handleLoRaPacket() {

  int packetSize = LoRa.parsePacket();
  if (!packetSize) return;

  String msg="";
  while(LoRa.available()) msg+=(char)LoRa.read();

  Serial.println("[RX] " + msg);

  if (!msg.startsWith("NODE")) return;

  int node = msg.substring(5, msg.indexOf(',',5)).toInt();

  /* ---------- ALERT ---------- */
  if (msg.indexOf("ALERTv1") > 0) {

    Serial.println("===== 🚨 INTERRUPT ALERT =====");
    Serial.println("Node: " + String(node));

    push(emergencyQ, et, node);

    LoRa.beginPacket();
    LoRa.print("ACKv1,");
    LoRa.print(node);
    LoRa.print(",ALERT");
    LoRa.endPacket();

    return;
  }

  /* ---------- SCAN RESULT ---------- */
  if (msg.indexOf("SCANv1") > 0) {

    Serial.println("===== 📊 SCAN DATA RECEIVED =====");

    int seqPos = msg.indexOf("SCANv1,") + 7;
    int seq = msg.substring(seqPos, msg.indexOf(',', seqPos)).toInt();

    LoRa.beginPacket();
    LoRa.print("ACKv1,");
    LoRa.print(node);
    LoRa.print(",");
    LoRa.print(seq);
    LoRa.endPacket();

    state = IDLE;
  }
}
