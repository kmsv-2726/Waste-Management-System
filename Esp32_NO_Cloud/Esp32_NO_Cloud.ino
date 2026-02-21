#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

/* CONFIG */
#define TOTAL_NODES 2
#define QUEUE_SIZE 5
#define COMMAND_TIMEOUT 5000

/* LORA */
#define LORA_SS 5
#define LORA_RST 27
#define LORA_DIO0 26
#define LORA_SCK 18
#define LORA_MISO 19
#define LORA_MOSI 23

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP,"pool.ntp.org",19800,60000);

int emergencyQ[QUEUE_SIZE], eHead=0,eTail=0;
int manualQ[QUEUE_SIZE], mHead=0,mTail=0;
int schedQ[QUEUE_SIZE], sHead=0,sTail=0;

bool busy=false;
int currentNode=-1;
unsigned long commandStartTime=0;
int lastHour=-1;

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

void setup(){

  Serial.begin(115200);

  WiFi.begin("YOUR_WIFI","YOUR_PASS");
  while(WiFi.status()!=WL_CONNECTED) delay(500);
  timeClient.begin();

  SPI.begin(LORA_SCK,LORA_MISO,LORA_MOSI,LORA_SS);
  LoRa.setPins(LORA_SS,LORA_RST,LORA_DIO0);
  LoRa.begin(433E6);
  LoRa.setSpreadingFactor(10);
  LoRa.enableCrc();

  Serial.println("Gateway Ready");
}

void loop(){

  timeClient.update();
  handleSerial();
  handleScheduler();
  handleLoRa();
  executeQueue();
  checkTimeout();
}

void handleSerial(){

  if(!Serial.available()) return;

  String cmd=Serial.readStringUntil('\n');
  cmd.trim();

  if(cmd.startsWith("em")){
    push(emergencyQ,eTail,cmd.substring(2).toInt());
  }
  else if(cmd.startsWith("scan")){
    push(manualQ,mTail,cmd.substring(4).toInt());
  }
}

void handleScheduler(){

  int hourNow=timeClient.getHours();

  if(hourNow!=lastHour){
    lastHour=hourNow;
    for(int i=1;i<=TOTAL_NODES;i++)
      push(schedQ,sTail,i);
  }
}

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

void handleLoRa(){

  int size=LoRa.parsePacket();
  if(!size) return;

  String packet="";
  while(LoRa.available()) packet+=(char)LoRa.read();

  processPacket(packet);
}

void processPacket(String packet){

  if(!packet.startsWith("NODE")) return;

  int f1=packet.indexOf(',');
  int f2=packet.indexOf(',',f1+1);
  int f3=packet.indexOf(',',f2+1);

  int node=packet.substring(f1+1,f2).toInt();
  String protocol=packet.substring(f2+1,f3);

  if(protocol=="SCANv2")
    parseScan(packet,f3,node);
  else if(protocol=="ALERTv1")
    parseAlert(packet,f3,node);
}

void parseScan(String packet,int start,int node){

  String data=packet.substring(start+1);

  int i1=data.indexOf(',');
  int i2=data.indexOf(',',i1+1);
  int i3=data.indexOf(',',i2+1);
  int i4=data.indexOf(',',i3+1);
  int i5=data.indexOf(',',i4+1);

  int seq=data.substring(0,i1).toInt();

  sendACK(node,seq);

  busy=false;
  currentNode=-1;
}

void parseAlert(String packet,int start,int node){

  String type=packet.substring(start+1);

  Serial.println("ALERT from Node "+String(node));
  Serial.println("Type: "+type);
  Serial.println("Time: "+String(timeClient.getEpochTime()));
}

void sendACK(int node,int seq){

  LoRa.beginPacket();
  LoRa.print("ACK,");
  LoRa.print(node);
  LoRa.print(",");
  LoRa.print(seq);
  LoRa.endPacket();
}

void checkTimeout(){

  if(!busy) return;

  if(millis()-commandStartTime>COMMAND_TIMEOUT){
    busy=false;
    currentNode=-1;
  }
}