#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <time.h>

/* ---------- LoRa ---------- */
#define LORA_SS   5
#define LORA_RST  27
#define LORA_DIO0 26

/* 🔵 NEW: Match Node PHY */
#define LORA_FREQ 433E6
#define SPREADING_FACTOR 10
#define BANDWIDTH 125E3
#define CODING_RATE 5

/* ---------- WiFi ---------- */
const char* ssid = "CSE23728";
const char* password = "Mani@123";

/* ---------- System ---------- */
#define TOTAL_NODES 2
#define SCAN_TIMEOUT 50000   

enum State { IDLE, SCANNING };
State state = IDLE;

int manualQ[10], emergencyQ[10];
int mh=0, mt=0, eh=0, et=0;
int currentNode = -1;

unsigned long scanStartTime = 0;   
int lastScheduledHour = -1;       

/* ---------- Utils ---------- */
bool empty(int h,int t){return h==t;}
void push(int q[],int &t,int v){q[t]=v; t=(t+1)%10;}
int pop(int q[],int &h){int v=q[h]; h=(h+1)%10; return v;}

/* ---------- Setup ---------- */
void setup() {
  Serial.begin(115200);
  Serial.println("[SYS] Booting ESP32");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);

  configTime(0,0,"pool.ntp.org");

  LoRa.setPins(LORA_SS,LORA_RST,LORA_DIO0);
  if (!LoRa.begin(LORA_FREQ)) while(1);

  /* Spreading Factor */
  LoRa.setSpreadingFactor(SPREADING_FACTOR);
  LoRa.setSignalBandwidth(BANDWIDTH);
  LoRa.setCodingRate4(CODING_RATE);
  LoRa.enableCrc();

  Serial.println("[SYS] System Ready");
}

/* ---------- Loop ---------- */
void loop() {
  handleSerial();
  handleLoRa();

  if (state == IDLE) scheduler();
  if (state == SCANNING && millis() - scanStartTime > SCAN_TIMEOUT) {
    Serial.println("[TIMEOUT] Node did not respond");
    state = IDLE;
  }

  hourScheduler();  
}

/* ---------- Scheduler ---------- */
void scheduler() {
  if (!empty(eh,et)) startScan(pop(emergencyQ,eh));
  else if (!empty(mh,mt)) startScan(pop(manualQ,mh));
}

/* ---------- Hour Based Scheduler ---------- */
void hourScheduler() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  if (timeinfo.tm_hour != lastScheduledHour) {
    lastScheduledHour = timeinfo.tm_hour;
    push(manualQ, mt, 1);   // auto scan node 1 every hour
  }
}

/* ---------- Start Scan ---------- */
void startScan(int node) {
  currentNode = node;
  state = SCANNING;
  scanStartTime = millis();  

  Serial.print("[EXEC] Scanning Node ");
  Serial.println(node);

  LoRa.beginPacket();
  LoRa.print("CMD,SCAN,");
  LoRa.print(node);
  LoRa.endPacket();
}

/* ---------- Serial ---------- */
void handleSerial() {
  if (!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim(); cmd.toUpperCase();

  if (cmd.startsWith("EMERGENCY")) {
    push(emergencyQ, et, cmd.substring(10).toInt());
  }
  else if (cmd.startsWith("SCAN")) {
    push(manualQ, mt, cmd.substring(5).toInt());
  }
}

/* ---------- LoRa RX ---------- */
void handleLoRa() {
  int p = LoRa.parsePacket();
  if (!p) return;

  String msg="";
  while(LoRa.available()) msg+=(char)LoRa.read();

  Serial.println("[RX] " + msg);

  if (msg.startsWith("NODE")) {
    int node = msg.substring(5, msg.indexOf(',',5)).toInt();

    if (msg.indexOf("SCANv1") > 0) {
      sendACK(node, msg);
      Serial.println("[DATA] Scan received");
      state = IDLE;
    }

    if (msg.indexOf("ALERTv1") > 0) {
      Serial.println("[ALERT] LOW BATTERY");
    }
  }
}

/* ---------- ACK ---------- */
void sendACK(int node, String msg) {
  int seqPos = msg.indexOf("SCANv1,") + 7;
  int seq = msg.substring(seqPos, msg.indexOf(',', seqPos)).toInt();

  LoRa.beginPacket();
  LoRa.print("ACKv1,");
  LoRa.print(node);
  LoRa.print(",");
  LoRa.print(seq);
  LoRa.endPacket();
}
