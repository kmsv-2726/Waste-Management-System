#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <time.h>

/* ---------- LoRa ---------- */
#define LORA_SS   5
#define LORA_RST  27
#define LORA_DIO0 26

/* ---------- WiFi ---------- */
const char* ssid = "YOUR_WIFI";
const char* password = "YOUR_PASS";

/* ---------- System ---------- */
#define TOTAL_NODES 2
enum State { IDLE, SCANNING };
State state = IDLE;

int manualQ[10], emergencyQ[10];
int mh=0, mt=0, eh=0, et=0;
int currentNode = -1;

/* ---------- Utils ---------- */
bool empty(int h,int t){return h==t;}
void push(int q[],int &t,int v){q[t]=v; t=(t+1)%10;}
int pop(int q[],int &h){int v=q[h]; h=(h+1)%10; return v;}

/* ---------- Setup ---------- */
void setup() {
  Serial.begin(115200);
  Serial.println("[SYS] Booting ESP32");

  WiFi.begin(ssid, password);
  Serial.print("[WIFI] Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\n[WIFI] Connected");
  Serial.println(WiFi.localIP());

  configTime(0,0,"pool.ntp.org");
  Serial.println("[TIME] NTP Ready");

  LoRa.setPins(LORA_SS,LORA_RST,LORA_DIO0);
  if (!LoRa.begin(433E6)) while(1);

  Serial.println("[SYS] System Ready");
}

/* ---------- Loop ---------- */
void loop() {
  handleSerial();
  handleLoRa();

  if (state == IDLE) scheduler();
}

/* ---------- Scheduler ---------- */
void scheduler() {
  if (!empty(eh,et)) startScan(pop(emergencyQ,eh));
  else if (!empty(mh,mt)) startScan(pop(manualQ,mh));
}

/* ---------- Start Scan ---------- */
void startScan(int node) {
  currentNode = node;
  state = SCANNING;

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
    Serial.println("[EMERGENCY] Queued");
  }
  else if (cmd.startsWith("SCAN")) {
    push(manualQ, mt, cmd.substring(5).toInt());
    Serial.println("[MANUAL] Queued");
  }
}

/* ---------- LoRa RX ---------- */
void handleLoRa() {
  int p = LoRa.parsePacket();
  if (!p) return;

  String msg="";
  while(LoRa.available()) msg+=(char)LoRa.read();

  Serial.print("[RX] ");
  Serial.println(msg);

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
