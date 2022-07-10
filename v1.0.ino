#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>
#include <FS.h>
#include <ArduinoJson.h>

const int led = 0;//esp01s relay ok
//const int led = 2;
const char* mqtt_server = "broker-cn.emqx.io";
int h1,h3,m1,m3;
bool timeOn=false;
bool bStart=true;
char msg[10]={0};
char sendto[15]={0};
char sendfrom[15]={0};

WiFiClient espClient;
PubSubClient mqttClient(espClient);
WiFiUDP ntpUDP;//wifi time;
NTPClient ntpClient(ntpUDP,"ntp.aliyun.com",28800,60000);

void smartConfig()
{
//  Serial.println("\r\nWait for Smartconfig...");//打印log信息
  WiFi.beginSmartConfig();//开始SmartConfig，等待手机端发出用户名和密码
  bool rn=true;
  while(rn)
  {
//    Serial.println(".");
    digitalWrite(LED_BUILTIN,HIGH);//指示灯闪烁
    delay(1000);
    digitalWrite(LED_BUILTIN,LOW);//指示灯闪烁
    delay(1000);
    if(WiFi.smartConfigDone())//配网成功，接收到SSID和密码
    {
//      Serial.println("SmartConfig Success");
      rn=false;
      //break;      
    }
  }  
}

void read_wificnf(){
    SPIFFS.begin();
    if(!SPIFFS.exists("/net.txt")){
      File file2=SPIFFS.open("/net.txt","w");
      file2.write("ab");
      file2.close();
      }
    File file=SPIFFS.open("/net.txt","r");
//  String nameStr = doc["name"].as<String>();WiFi.psk().c_str()// 获取解析后的数据信息
//  int yearsInt = doc["years"].as<int>();
    StaticJsonDocument<128> doc;  //最小值不能低于128
    DeserializationError error = deserializeJson(doc, file);
    if (error){
//      Serial.println("json file error");
//        Serial.println(error.f_str());
       file.close();
       file=SPIFFS.open("/net.txt","w");
       StaticJsonDocument<128> doc2;
       smartConfig();
//       Serial.println("set net ok;");
       doc2["host"]=WiFi.SSID();
       doc2["pwd"]=WiFi.psk();
       serializeJson(doc2, file);
      }
     else{
//      Serial.printf("2PSW:%s\r\n",doc["pwd"]);
       const char* s1=doc["host"];
       const char* s2=doc["pwd"];
       WiFi.begin(s1,s2);
        }
  file.close();
  SPIFFS.end(); 
}

void init_wifi() {
  WiFi.mode(WIFI_STA);
  read_wificnf();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
//    Serial.print(".");
  }
//  Serial.println("IP address: ");
//  Serial.print(WiFi.localIP());
//  Serial.printf("MAC address: %s\n",WiFi.macAddress().c_str());
  char saddr[18];
  strcpy(saddr,WiFi.macAddress().c_str());
  int j=0;
  for (int i=0;i<18;i++){
    if (saddr[i]==':'){
      continue;
    }
    sendto[j]=saddr[i];
    sendfrom[j]=saddr[i];
    j=j+1;
  }
  sendto[12]='_';
  sendto[13]='B';
  sendfrom[12]='_';
  sendfrom[13]='A';
//  Serial.println(LED_BUILTIN);
//  Serial.println("-----------");
//  Serial.println(sendto);
//  Serial.println(sendfrom);
//  sendto[14]='\0';
 
}

void chkTime(){
   if (h1==ntpClient.getHours() && m1==ntpClient.getMinutes() && bStart){
//    Serial.println("start..");
      mqttClient.publish(sendto,"00");
      bStart=false;
      digitalWrite(led, LOW);
    }
   if (h3==ntpClient.getHours() && m3==ntpClient.getMinutes()){
      mqttClient.publish(sendto,"4");
      timeOn=false;
      digitalWrite(led, HIGH);//HIGH off
      mqttClient.publish(sendto,"11");
    }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
//  Serial.println("length:");
//  Serial.println(length);
  char s1=(char)payload[0];
  msg[0]=s1;
  int n=s1-'0';
//  String sp=(char*)payload;
//  Serial.println(sp);
  int mn=digitalRead(led);
  char smg[3];
//  char smg[3]={"\0"};
  int ns[8]={0};
  char ss;
  int j=0;
  int h2,m2;
//  Serial.println(n);
  switch (n) {
   case 0: digitalWrite(led, LOW); break;//switch on 
   case 1: digitalWrite(led, HIGH);timeOn=false; break;//switch off
   case 2: //switch time on
     if (length<8){
        for (int i = 1; i < 5; i++) {
          ss=(char)payload[i];
          //Serial.println(ss);
          j=i-1;
          msg[i]=ss;
          ns[j]=ss-'0';
        }
        msg[5]='\0';
        h2=ns[0]*10+ns[1];
        m2=ns[2]*10+ns[3];
        h3=ntpClient.getHours()+h2;
        m3=ntpClient.getMinutes()+m2;
//        Serial.println("stop time:");
//        Serial.println(h3);
//        Serial.println(m3);
        if (m3>59){
          h3=h3+1;
          m3=m3-60;
        }
        if (h3>23){
          h3=h3-24;
        }
        bStart=false;
      }
      else
      {
        for (int i = 1; i < 9; i++) {
          //Serial.print((char)payload[i]);
          ss=(char)payload[i];
          //Serial.println(ss);
          j=i-1;
          msg[i]=ss;
          ns[j]=ss-'0';
        }
        h1=ns[0]*10+ns[1];
        m1=ns[2]*10+ns[3];
        h2=ns[4]*10+ns[5];
        m2=ns[6]*10+ns[7];
        m3=m1+m2;
        h3=h1+h2;
        if (m3>59){
          h3=h3+1;
          m3=m3-60;
        }
        if (h3>23){
          h3=h3-24;
        }
        bStart=true;
       }
      timeOn=true;
     break;
   case 3: timeOn=false;break;//switch time off
   case 4://return all status
     ltoa(mn,smg,2);
     if (timeOn){strcat(smg,"0");}
     else{strcat(smg,"1");}
     mqttClient.publish(sendto,smg); 
     break;
   case 5: //return time
     if (timeOn){
       mqttClient.publish(sendto,msg);
     }else{
      mqttClient.publish(sendto,"4");
     }
    break; 
   case 6://switch time range on
    break;
  }
}

void mqttReconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    // Create a random client ID
    String clientId = "mqtt%*-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (mqttClient.connect(clientId.c_str())) {
      mqttClient.subscribe(sendfrom);
    } else {
//      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
  
void setup() {
  Serial.begin(115200);
//  Serial.println("start......");
  pinMode(LED_BUILTIN,OUTPUT);
//  digitalWrite(LED_BUILTIN, LOW);
  pinMode(led, OUTPUT);
  digitalWrite(led, HIGH);
  init_wifi();
  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(mqttCallback);
  ntpClient.begin();
}

void loop() {
  ntpClient.update();
  //Serial.println(ntpClient.getFormattedTime());
  if (!mqttClient.connected()) {
    mqttReconnect();
  }
  mqttClient.loop();
  if (timeOn){chkTime();}
}
