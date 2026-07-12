// SmartBuilding-Node_3.ino
/* NOTE:
  This file is based on the circuit designed on wokwi.
  Replace YOUR_WIFI_NAME / PASSWORD if needed. */

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

#define NODE_ID "NODE_3"

const char* ssid = "Project BMS";             // Your WiFi SSID
const char* password = "hdsoaw_509p6";        // Your WiFi password
const char* mqtt_server = "192.168.137.146";  // Your MQTT Broker IP
const int mqtt_port = 1883;                   // Your MQTT port

#define DHTPIN 4
#define DHTTYPE DHT22
#define FLAME_PIN 27
#define GAS_DO 26
#define GAS_AO 34
#define BUZZER_PIN 25

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27,20,4);

unsigned long previousSensorMillis = 0;
const unsigned long sensorInterval = 1000;
unsigned long previousBuzzerMillis = 0;
bool buzzerState = false;

void connectWiFi(){
  if(WiFi.status()==WL_CONNECTED) return;
  WiFi.begin(ssid,password);
  while(WiFi.status()!=WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected");
}

void callback(char* topic, byte* payload, unsigned int length){
  Serial.print("MQTT: ");
  for(unsigned int i=0;i<length;i++) Serial.print((char)payload[i]);
  Serial.println();
}

void reconnectMQTT()
{
    while (!client.connected())
    {
        Serial.println("Connecting MQTT...");

        if (client.connect(NODE_ID))
        {
            Serial.println("MQTT Connected");

            String t = "building/";
            t += NODE_ID;
            t += "/cmd";

            client.subscribe(t.c_str());

            Serial.println("Subscribed to: " + t);
        }
        else
        {
            Serial.print("MQTT Failed. State = ");
            Serial.println(client.state());

            delay(2000);
        }
    }
}

void setup(){
  Serial.begin(115200);
  dht.begin();
  pinMode(FLAME_PIN,INPUT);
  pinMode(GAS_DO,INPUT);

  lcd.init();
  lcd.backlight();
  lcd.clear();

  ledcAttach(BUZZER_PIN,2000,8);

  connectWiFi();
  Serial.println("WiFi Connected");
  Serial.println("Setting MQTT Server...");
  client.setServer(mqtt_server,mqtt_port);
  client.setCallback(callback);

  reconnectMQTT();
}

void loop(){
  connectWiFi();

  if(!client.connected())
    reconnectMQTT();

  client.loop();

  if(millis()-previousSensorMillis<sensorInterval)
    return;

  previousSensorMillis=millis();

  float temp=dht.readTemperature();
  float hum=dht.readHumidity();

  if(isnan(temp)||isnan(hum)){
    Serial.println("DHT Read Failed");
    return;
        
  }

  bool flameDetected=(digitalRead(FLAME_PIN)==LOW);
  bool gasDetected=(digitalRead(GAS_DO)==LOW);
  int gasValue=analogRead(GAS_AO);

  String fireStatus="SAFE";
  if(flameDetected) fireStatus="DANGER";
  else if(temp>40) fireStatus="WARNING";

  String gasStatus="SAFE";
  if(gasValue<1000) gasStatus="SAFE";
  else if(gasValue<2000) gasStatus="LOW";
  else if(gasValue<3000) gasStatus="MED";
  else gasStatus="DANGER";
  if(gasDetected) gasStatus="DANGER";

  String alertLevel="SAFE";
  if(fireStatus=="DANGER"||gasStatus=="DANGER") alertLevel="DANGER";
  else if(fireStatus=="WARNING"||gasStatus=="LOW"||gasStatus=="MED") alertLevel="WARNING";

  if(alertLevel=="SAFE"){
    ledcWriteTone(BUZZER_PIN,0);
    buzzerState=false;
  }else if(alertLevel=="WARNING"){
    if(millis()-previousBuzzerMillis>=300){
      previousBuzzerMillis=millis();
      buzzerState=!buzzerState;
      ledcWriteTone(BUZZER_PIN,buzzerState?2000:0);
    }
  }else{
    ledcWriteTone(BUZZER_PIN,3000);
  }

  lcd.setCursor(0,0);
  lcd.print("T:"); lcd.print(temp,1); lcd.print((char)223); lcd.print("C ");
  lcd.print("H:"); lcd.print(hum,1); lcd.print("%   ");

  lcd.setCursor(0,1);
  lcd.print("Fire:"); lcd.print(fireStatus); lcd.print("     ");

  lcd.setCursor(0,2);
  lcd.print("Gas:"); lcd.print(gasStatus); lcd.print("      ");

  lcd.setCursor(0,3);
  lcd.print(NODE_ID); lcd.print(" "); lcd.print(alertLevel); lcd.print("    ");

  StaticJsonDocument<256> doc;
  doc["node"]=NODE_ID;
  doc["temperature"]=temp;
  doc["humidity"]=hum;
  doc["gasValue"]=gasValue;
  doc["gasStatus"]=gasStatus;
  doc["fireStatus"]=fireStatus;
  doc["alert"]=alertLevel;

  char payload[256];
  serializeJson(doc,payload);

  String topic="building/";
  topic+=NODE_ID;
  topic+="/data";

  Serial.println("Publishing...");

  bool ok = client.publish(topic.c_str(), payload);

  Serial.print("Publish = ");
  Serial.println(ok);

  Serial.println(payload);
}
