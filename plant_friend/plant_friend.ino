// Libraries for AWS IoT communication
#include "secrets.h"
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "WiFi.h"

// Libraries for our I2C sensors
#include "Adafruit_seesaw.h"
#include <BH1750.h>

// Topics for AWS IoT
#define AWS_IOT_PUBLISH_TOPIC   "esp32/pub"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp32/sub"

// Globals for temperature, soil moisture and light
float t;  // Temperature in °C
float h;  // Soil moisture as a percentage
float l;  // Light in lux

WiFiClientSecure net = WiFiClientSecure();
PubSubClient client(net);

// The soil moisture sensor will be on the I2C bus as 0x36
Adafruit_seesaw ss;

// The light sensor will be on the bus as 0x23

// The light sensor also needs a pre-initialized I2C bus, which
// the soil moisture sensor's initialization might do
BH1750 lightMeter;

void enableSoilSensor() {
  Serial.println("Enabling soil sensor");
  
  if (!ss.begin(0x36)) {
    Serial.println("ERROR! Sensor not found");
    while(1) delay(1);
  } else {
    Serial.print("Soil sensor started! Firmware Version: ");
    Serial.println(ss.getVersion(), HEX);
  }
}

void enableLightSensor() {
  Serial.println("Enabling soil sensor");
  lightMeter.begin();
  Serial.println("Enabled soil sensor");
}

void connectAWS() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println("Connecting to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // Configure WiFiClientSecure to use the AWS IoT device credentials
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);

  // Connect to the MQTT broker on the AWS endpoint we defined earlier
  client.setServer(AWS_IOT_ENDPOINT, 8883);

  // Create a message handler
  client.setCallback(messageHandler);

  Serial.println("Connecting to AWS IOT");

  while (!client.connect(THINGNAME)) {
    Serial.print(".");
    delay(100);
  }

  if (!client.connected()) {
    Serial.println("AWS IoT Timeout!");
    return;
  }

  // Subscribe to a topic
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
  Serial.println("AWS IoT Connected!");
}

void publishMessage() {
  StaticJsonDocument<200> doc;
  doc["soil_moisture"] = h;
  doc["temperature"] = t;
  doc["light"] = l;
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer); // print to client

  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
}

void messageHandler(char* topic, byte* payload, unsigned int length) {
  Serial.print("incoming: ");
  Serial.println(topic);

  StaticJsonDocument<200> doc;
  deserializeJson(doc, payload);
  const char* message = doc["message"];
  Serial.println(message);
}

void setup() {
  // Initialization
  Serial.begin(115200);
  enableSoilSensor();
  enableLightSensor();
  connectAWS();
}

void loop() {
  // Sample sensors
  t = ss.getTemp();
  h = ss.touchRead(0);
  l = lightMeter.readLightLevel();

  // Scale from 0-100%
  h = map(h, 0, 1023, 0, 100);

  // Serial monitor writes
  Serial.print(F("Soil Moisture: "));
  Serial.print(h);
  Serial.print(F("%, Temperature: "));
  Serial.print(t);
  Serial.print("°C, Light: ");
  Serial.print(l);
  Serial.println(" lux");

  // Write to cloud
  publishMessage();
  client.loop();

  // Delay in superloop
  delay(1000);
}
