
/*
 * 
 * This detects advertising messages of BLE devices and compares it with stored MAC addresses. 
 * If one matches, it sends an MQTT message to swithc something

   Copyright <2017> <Andreas Spiess>
   Based on Neil Kolban's example file: https://github.com/nkolban/ESP32_BLE_Arduino
 
 Wiring Relay = NO dan COM untuk ke blackbox dan setup Relay HIGH, kalau bukan NC dan COM
 
 
 */
#include <ArduinoJson.h>
#include "BLEDevice.h"
#include <WiFi.h>
#include <PubSubClient.h>


static BLEAddress *pServerAddress;

#define RELAY 5
#define PIR 27
//#define PushButton 12
#define LEDWF 2
#define LEDBT 15


BLEScan* pBLEScan;
BLEClient*  pClient;

String ble_name;
//String ble_data;
bool deviceFound = false;

String knownAddresses[] = {"5c:d0:6e:34:12:1a", "0c:98:38:b6:86:06", "30:c6:f7:05:86:a6", "58:bf:25:35:f6:8e", "0a:13:42:e5:ba:0b"};

const char* ssid = "Engineering24";
const char* password = "12345678";
//const char* mqtt_server = "test.mosquitto.org";  // change for your own MQTT broker address
//#define TOPIC "ESP32/BLEDoorAccess"  // Change for your own topic
//#define PAYLOAD "1"    // change for your own payload

const char* mqtt_server = "broker.emqx.io";//server address
const int port=1883;//server port
const char* user          = "Yusri";
const char* pass          = "Esp@32";
const char* client_id     = "mqttx_4e57a5ec";
//const char* topicResult   = "ESP32BLE/DoorAccess";
const char* topicSubs     = "ESP32BLE/DoorAccess"; //"esp32door/#"

 unsigned long entry;

WiFiClient espClient;
PubSubClient MQTTclient(espClient);

/*******************************************************************
 BLE IS WORKING
 
 *******************************************************************/

static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
  Serial.print("Notify callback for characteristic ");
  Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
  Serial.print(" of data length ");
  Serial.println(length);
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    /**
        Called for each advertising BLE server.
    */
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      Serial.print("BLE Advertised Device found: ");
      Serial.println(advertisedDevice.toString().c_str());
      //ble_data=advertisedDevice.toString().c_str();
      pServerAddress = new BLEAddress(advertisedDevice.getAddress());

      bool known = false;
      for (int i = 0; i < (sizeof(knownAddresses) / sizeof(knownAddresses[0])); i++) {
        if (strcmp(pServerAddress->toString().c_str(), knownAddresses[i].c_str()) == 0) known = true;
      }
      if (known) {
        Serial.print("Device found: ");
        Serial.println(advertisedDevice.getRSSI());
        if (advertisedDevice.getRSSI() > -69) deviceFound = true;
        else deviceFound = false;
        Serial.println(pServerAddress->toString().c_str());
        ble_name=pServerAddress->toString().c_str();
        advertisedDevice.getScan()->stop();
      }

      /******** This releases the memory when we're done. ********/
      delete pServerAddress; 
      digitalWrite(LEDWF, HIGH);
      digitalWrite(LEDBT, LOW);
      
   }
}; // MyAdvertisedDeviceCallbacks

/*******************************************************************
 WIFI IS WORKING
 
 *******************************************************************/

void sendMessage() {
  //btStop();
  StaticJsonDocument<500> data;
  char sentSrvr[512];
  //data["id"]="BLEDOOR0001";
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  entry = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - entry >= 15000) esp_restart();
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("WiFi connected, IP address: ");
  Serial.println(WiFi.localIP());
  MQTTclient.setServer(mqtt_server, 1883);
  MQTTclient.setCallback(MQTTcallback);
  Serial.println("Connect to MQTT server...");
  while (!MQTTclient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (MQTTclient.connect(client_id, user, pass)) {
      Serial.println("connected");
      data["deviceFound"]=ble_name;
      //data["deviceFound"]=ble_data;
      //data["pirVal"]="1";
      //data["doorOpen"]="1"; 
      serializeJsonPretty(data,sentSrvr);           
      MQTTclient.publish("ESP32BLE/DoorAccess", sentSrvr); 
      serializeJsonPretty(data,Serial);     
    } else {
      Serial.print("failed, rc=");
      Serial.print(MQTTclient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(1000);
    }
  }
  for (int i = 0; i > 10; i++) {
    MQTTclient.loop();
    delay(100);
  }
  MQTTclient.disconnect();
  delay(100);
  WiFi.mode(WIFI_OFF);
  btStart();
  digitalWrite(LEDWF, LOW);
  digitalWrite(LEDBT, HIGH);
}

void MQTTcallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Arduino BLE Client application...");
  pinMode(RELAY, OUTPUT);
  digitalWrite(RELAY, HIGH);
  pinMode(LEDWF, OUTPUT);
  pinMode(LEDBT, OUTPUT);
 
  
  
  BLEDevice::init("");

  pClient  = BLEDevice::createClient();
  Serial.println(" - Created client");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
}

void loop() {

  Serial.println();
  Serial.println("BLE Scan restarted.....");
  
  deviceFound = false;
  BLEScanResults scanResults = pBLEScan->start(30);
  int sensValue = digitalRead(PIR);
  //int Push_button_state = digitalRead(PushButton); 
  if (deviceFound && sensValue == HIGH) {
    Serial.println("MOTION DETECTED! DOOR OPEN");
    digitalWrite(RELAY, LOW);
    delay(10000);
    digitalWrite(RELAY, HIGH);
    Serial.println("DOOR CLOSE");    
    sendMessage();
    Serial.println("Waiting for 60 seconds");
    delay(60000);

  }else {
    Serial.println("DOOR CLOSE");
    digitalWrite(RELAY, HIGH);
  }
} // End of loop
