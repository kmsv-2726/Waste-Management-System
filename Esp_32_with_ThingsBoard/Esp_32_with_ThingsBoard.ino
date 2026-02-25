#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <PubSubClient.h>

/* ================= WIFI ================= */
#define WIFI_SSID "CSE23728"
#define WIFI_PASS "Mani@123"

/* ================= THINGSBOARD ================= */
#define TB_SERVER "thingsboard.cloud"
#define TB_PORT 1883
#define TB_TOKEN "YOUR_DEVICE_ACCESS_TOKEN"

/* ================= NETWORK CONFIG ================= */
#define TOTAL_NODES 3
#define QUEUE_SIZE 5
#define COMMAND_TIMEOUT 5000

/* ================= LORA ================= */
#define LORA_SS 5
#define LORA_RST 27
#define LORA_DIO0 26
#define LORA_SCK 18
#define LORA_MISO 19
#define LORA_MOSI 23

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800, 60000);

WiFiClient espClient;
PubSubClient client(espClient);

/* ================= QUEUES ================= */
int emergencyQ[QUEUE_SIZE], eHead=0,eTail=0;
int manualQ[QUEUE_SIZE], mHead=0,mTail=0;
int schedQ[QUEUE_SIZE], sHead=0,sTail=0;

/* ================= STATE ================= */
bool busy=false;
int currentNode=-1;
unsigned long commandStartTime=0;
int lastHour=-1;
/* ================= ADR ================= */
int nodeSF[TOTAL_NODES + 1] = {0};  // store current SF per node
bool adrEnabled = true;
void adjustDataRate(int node, int rssi){

  if(!adrEnabled) return;

  int targetSF;

  if(rssi > -70)
    targetSF = 7;
  else if(rssi > -85)
    targetSF = 9;
  else
    targetSF = 12;

  if(nodeSF[node] == targetSF)
    return;

  Serial.print("ADR changing Node ");
  Serial.print(node);
  Serial.print(" to SF ");
  Serial.println(targetSF);

  LoRa.beginPacket();
  LoRa.print("CMD,");
  LoRa.print(node);
  LoRa.print(",SF,");
  LoRa.print(targetSF);
  LoRa.endPacket();

  nodeSF[node] = targetSF;
}

/* ================= QUEUE UTILS ================= */
bool isEmpty(int h,int t){ return h==t; }

void push(int q[],int &tail,int val){
  q[tail]=val;
  tail=(tail+1)%QUEUE_SIZE;
}

int pop(int q[],int &head){
  int v=q[head];
  head=(head+1)%QUEUE_SIZE;
  return v;
}

/* ================= MQTT RECONNECT ================= */
void reconnectMQTT(){
  while(!client.connected()){
    if(client.connect("GatewayClient",TB_TOKEN,NULL)){
      client.subscribe("v1/devices/me/rpc/request/+");
    } else {
      delay(2000);
    }
  }
}

/* ================= RPC CALLBACK ================= */
void rpcCallback(char* topic, byte* payload, unsigned int length){

  String msg="";
  for(unsigned int i=0;i<length;i++)
    msg+=(char)payload[i];

  // Extract request ID from topic
  String topicStr = String(topic);
  int lastSlash = topicStr.lastIndexOf('/');
  String requestId = topicStr.substring(lastSlash + 1);

  bool accepted = false;
  int node = -1;

  if(msg.indexOf("scan")!=-1){
    int idx = msg.indexOf("node");
    node = msg.substring(idx+6).toInt();
    push(manualQ, mTail, node);
    accepted = true;
  }

  if(msg.indexOf("emergency")!=-1){
    int idx = msg.indexOf("node");
    node = msg.substring(idx+6).toInt();
    push(emergencyQ, eTail, node);
    accepted = true;
  }

  // Build RPC response
  String response = "{";
  response += "\"status\":\"";
  response += (accepted ? "accepted" : "rejected");
  response += "\",";
  response += "\"node\":";
  response += String(node);
  response += ",\"queued\":";
  response += (accepted ? "true" : "false");
  response += "}";

  // Publish response to correct TB topic
  String responseTopic = "v1/devices/me/rpc/response/" + requestId;
  client.publish(responseTopic.c_str(), response.c_str());
}

/* ================= SETUP ================= */
void setup(){

  Serial.begin(115200);

  WiFi.begin(WIFI_SSID,WIFI_PASS);
  while(WiFi.status()!=WL_CONNECTED) delay(500);

  client.setServer(TB_SERVER,TB_PORT);
  client.setCallback(rpcCallback);

  timeClient.begin();

  SPI.begin(LORA_SCK,LORA_MISO,LORA_MOSI,LORA_SS);
  LoRa.setPins(LORA_SS,LORA_RST,LORA_DIO0);
  LoRa.begin(433E6);
  LoRa.setSpreadingFactor(10);
  LoRa.enableCrc();

  Serial.println("FULL CLOUD QUEUE GATEWAY READY");
}

/* ================= LOOP ================= */
void loop(){

  if(!client.connected())
    reconnectMQTT();

  client.loop();
  timeClient.update();

  handleScheduler();
  handleLoRa();
  executeQueue();
  checkTimeout();
}

/* ================= SCHEDULER ================= */
void handleScheduler(){

  int hourNow=timeClient.getHours();

  if(hourNow!=lastHour){
    lastHour=hourNow;
    for(int i=1;i<=TOTAL_NODES;i++)
      push(schedQ,sTail,i);
  }
}

/* ================= EXECUTION ENGINE ================= */
void executeQueue(){

  if(busy) return;

  if(!isEmpty(eHead,eTail))
    currentNode=pop(emergencyQ,eHead);
  else if(!isEmpty(mHead,mTail))
    currentNode=pop(manualQ,mHead);
  else if(!isEmpty(sHead,sTail))
    currentNode=pop(schedQ,sHead);
  else return;

  LoRa.beginPacket();
  LoRa.print("CMD,");
  LoRa.print(currentNode);
  LoRa.print(",SCAN");
  LoRa.endPacket();

  busy=true;
  commandStartTime=millis();
}

/* ================= LORA RECEIVE ================= */
void handleLoRa(){

int size = LoRa.parsePacket();
if(!size) return;

int rssi = LoRa.packetRssi();

String packet="";
while(LoRa.available())
  packet+=(char)LoRa.read();

processPacket(packet, rssi);
}

/* ================= PACKET PROCESSOR ================= */
void processPacket(String packet, int rssi){

  if(!packet.startsWith("NODE")) return;

  int f1=packet.indexOf(',');
  int f2=packet.indexOf(',',f1+1);
  int f3=packet.indexOf(',',f2+1);

  int node=packet.substring(f1+1,f2).toInt();
  String protocol=packet.substring(f2+1,f3);

  if(protocol=="SCANv2")
    parseScan(packet,f3,node,,rssi);
  else if(protocol=="ALERTv1")
    parseAlert(packet,f3,node,rssi);
}

/* ================= SCAN PARSER ================= */
void parseScan(String packet,int start,int node,int rssi){

  String data=packet.substring(start+1);

  int i1=data.indexOf(',');
  int i2=data.indexOf(',',i1+1);
  int i3=data.indexOf(',',i2+1);
  int i4=data.indexOf(',',i3+1);
  int i5=data.indexOf(',',i4+1);

  int seq=data.substring(0,i1).toInt();
  float temp=data.substring(i1+1,i2).toFloat();
  float hum=data.substring(i2+1,i3).toFloat();
  int gas=data.substring(i3+1,i4).toInt();
  float battery=data.substring(i4+1,i5).toFloat();

  unsigned long timestamp=timeClient.getEpochTime();

  String json="{";
  json+="\"node\":"+String(node)+",";
  json+="\"seq\":"+String(seq)+",";
  json+="\"temperature\":"+String(temp)+",";
  json+="\"humidity\":"+String(hum)+",";
  json+="\"gas\":"+String(gas)+",";
  json+="\"battery\":"+String(battery,2)+",";
  json+="\"timestamp\":"+String(timestamp);
  json+="}";

  client.publish("v1/devices/me/telemetry",json.c_str());
  adjustDataRate(node, rssi);
  sendACK(node,seq);

  busy=false;
  currentNode=-1;
}

/* ================= ALERT PARSER ================= */
void parseAlert(String packet,int start,int node){

  String type=packet.substring(start+1);

  unsigned long timestamp=timeClient.getEpochTime();

  String json="{";
  json+="\"node\":"+String(node)+",";
  json+="\"alert\":\""+type+"\",";
  json+="\"timestamp\":"+String(timestamp);
  json+="}";

  client.publish("v1/devices/me/telemetry",json.c_str());
}

/* ================= ACK ================= */
void sendACK(int node,int seq){

  LoRa.beginPacket();
  LoRa.print("ACK,");
  LoRa.print(node);
  LoRa.print(",");
  LoRa.print(seq);
  LoRa.endPacket();
}

/* ================= TIMEOUT ================= */
void checkTimeout(){

  if(!busy) return;

  if(millis()-commandStartTime>COMMAND_TIMEOUT){
    busy=false;
    currentNode=-1;
  }
}