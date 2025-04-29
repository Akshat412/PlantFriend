// Library for display integration
#include <Arduino.h>
#include <U8g2lib.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

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

#define uS_TO_S_FACTOR 1000000ULL  // Conversion factor for micro seconds to seconds
#define TIME_TO_SLEEP  300         // Time ESP32 will go to sleep (in seconds)

const int wakeupPin = 7;  // GPIO 7 for external wake-up
esp_sleep_wakeup_cause_t wakeup_reason;

// Globals for temperature, soil moisture and light
float t[5];  // Temperature in °C
float h[5];  // Soil moisture as a percentage
float l[5];  // Light in lux

WiFiClientSecure net = WiFiClientSecure();
PubSubClient client(net);

// The soil moisture sensor will be on the I2C bus as 0x36
Adafruit_seesaw ss;

// The light sensor will be on the bus as 0x23

// The light sensor also needs a pre-initialized I2C bus, which
// the soil moisture sensor's initialization might do
BH1750 lightMeter;

/* --- DISPLAY CODE --- */
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

#define cross_width 24
#define cross_height 24
static const unsigned char cross_bits[] U8X8_PROGMEM  = {
  0x00, 0x18, 0x00, 0x00, 0x24, 0x00, 0x00, 0x24, 0x00, 0x00, 0x42, 0x00, 
  0x00, 0x42, 0x00, 0x00, 0x42, 0x00, 0x00, 0x81, 0x00, 0x00, 0x81, 0x00, 
  0xC0, 0x00, 0x03, 0x38, 0x3C, 0x1C, 0x06, 0x42, 0x60, 0x01, 0x42, 0x80, 
  0x01, 0x42, 0x80, 0x06, 0x42, 0x60, 0x38, 0x3C, 0x1C, 0xC0, 0x00, 0x03, 
  0x00, 0x81, 0x00, 0x00, 0x81, 0x00, 0x00, 0x42, 0x00, 0x00, 0x42, 0x00, 
  0x00, 0x42, 0x00, 0x00, 0x24, 0x00, 0x00, 0x24, 0x00, 0x00, 0x18, 0x00, };

#define cross_fill_width 24
#define cross_fill_height 24
static const unsigned char cross_fill_bits[] U8X8_PROGMEM  = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x18, 0x64, 0x00, 0x26, 
  0x84, 0x00, 0x21, 0x08, 0x81, 0x10, 0x08, 0x42, 0x10, 0x10, 0x3C, 0x08, 
  0x20, 0x00, 0x04, 0x40, 0x00, 0x02, 0x80, 0x00, 0x01, 0x80, 0x18, 0x01, 
  0x80, 0x18, 0x01, 0x80, 0x00, 0x01, 0x40, 0x00, 0x02, 0x20, 0x00, 0x04, 
  0x10, 0x3C, 0x08, 0x08, 0x42, 0x10, 0x08, 0x81, 0x10, 0x84, 0x00, 0x21, 
  0x64, 0x00, 0x26, 0x18, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, };

#define cross_block_width 14
#define cross_block_height 14
static const unsigned char cross_block_bits[] U8X8_PROGMEM  = {
  0xFF, 0x3F, 0x01, 0x20, 0x01, 0x20, 0x01, 0x20, 0x01, 0x20, 0x01, 0x20, 
  0xC1, 0x20, 0xC1, 0x20, 0x01, 0x20, 0x01, 0x20, 0x01, 0x20, 0x01, 0x20, 
  0x01, 0x20, 0xFF, 0x3F, };

void u8g2_prepare(void) {
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setFontRefHeightExtendedText();
  u8g2.setDrawColor(1);
  u8g2.setFontPosTop();
  u8g2.setFontDirection(0);
}

void u8g2_happy(uint8_t a) {
  u8g2.clearBuffer();

  // Happy eyes
  u8g2.drawDisc(40, 24, 8, U8G2_DRAW_ALL);  // Left eye
  u8g2.drawDisc(88, 24, 8, U8G2_DRAW_ALL);  // Right eye

  // Smiling mouth (arc from 20° to 160°)
  for (int angle = 20; angle <= 160; angle += 10) {
    float rad1 = angle * 3.14159 / 180.0;
    float rad2 = (angle + 10) * 3.14159 / 180.0;
    int x1 = 64 + cos(rad1) * 6;
    int y1 = 28 + sin(rad1) * 6;  // was 38, now 28
    int x2 = 64 + cos(rad2) * 6;
    int y2 = 28 + sin(rad2) * 6;  // was 38, now 28
    u8g2.drawLine(x1, y1, x2, y2);
  }

  u8g2.sendBuffer();
}

void u8g2_sad(uint8_t a) {
  u8g2.clearBuffer();

  // Sad eyes
  u8g2.drawDisc(40, 24, 8, U8G2_DRAW_ALL);  // Left eye
  u8g2.drawDisc(88, 24, 8, U8G2_DRAW_ALL);  // Right eye

  // Frowning mouth (arc from 20° to 160°)
  for (int angle = 20; angle <= 160; angle += 10) {
    float rad1 = angle * 3.14159 / 180.0;
    float rad2 = (angle + 10) * 3.14159 / 180.0;
    int x1 = 64 + cos(rad1) * 6;
    int y1 = 32 - sin(rad1) * 6;  // moved mouth center from 28 to 32
    int x2 = 64 + cos(rad2) * 6;
    int y2 = 32 - sin(rad2) * 6;  // moved mouth center from 28 to 32
    u8g2.drawLine(x1, y1, x2, y2);
  }

  u8g2.sendBuffer();
}

// Method to print the reason by which ESP32 has been awaken from sleep
void print_wakeup_reason(){
  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}

void goToSleep() {
  // Configure the wake up source: set our ESP32 to wake up TIME_TO_SLEEP seconds
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) +
  " Seconds");
  delay(100);

  Serial.flush(); 
  esp_deep_sleep_start();
}

void enableSoilSensor() {
  Serial.println("Enabling soil sensor");
  
  if (!ss.begin(0x36)) {
    while(1) {
      Serial.println("ERROR! Sensor not found");
      delay(1000);
    }
  } else {
    Serial.print("Soil sensor started! Firmware Version: ");
    Serial.println(ss.getVersion(), HEX);
  }
}

void enableLightSensor() {
  Serial.println("Enabling light sensor");
  lightMeter.begin();
  Serial.println("Enabled light sensor");
}

void connectAWS() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID);

  Serial.println("Connecting to Wi-Fi");

  int tries_max = 64;
  int tries = 0;

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

  JsonArray tempArray = doc.createNestedArray("temperature");
  JsonArray soilArray = doc.createNestedArray("soil_moisture");
  JsonArray lightArray = doc.createNestedArray("light");

  // Add float arrays to JsonArrays 
  for(int i = 0; i < 5; i++){
    tempArray.add(t[i]);
    soilArray.add(h[i]);
    lightArray.add(l[i]);
  }

  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer); // print to client

  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
}

void messageHandler(char* topic, byte* payload, unsigned int length) {
  Serial.println("In message handler");
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, payload, length);

  if (error) {
    Serial.print("Deserialization failed: ");
    Serial.println(error.c_str());
    return;
  }

  // String message = doc["status_code"].as<String>(); // extract as String
  int message = doc["status_code"].as<int>(); // matches JSON structure

  Serial.print("Message received from AWS: ");
  Serial.println(message);

  // Shortcut 
  u8g2.clearBuffer();
  if (message == 1) { // happy
    Serial.println("The message is HAPPY, calling draw function now");
    u8g2_happy(0);
  } else if (message == 2) { // sad
    Serial.println("The message is SAD, calling draw function now");
    u8g2_sad(0);
  // } else if (message == "sade") {
  //   Serial.println("The message is SADE, calling draw function now");
  //   u8g2_sad(0);
  } else {
    Serial.println("nothing!");
  }

  // Write all values to screen
  u8g2.sendBuffer();
}

void setup() {
  pinMode(wakeupPin, INPUT_PULLDOWN);  // Declaring the pin
  esp_sleep_enable_ext0_wakeup((gpio_num_t)wakeupPin, HIGH);  // Configure external wake-up

  // Initialize serial
  Serial.begin(115200);
  delay(100);

  // Initialize screen
  u8g2.begin();

  // Print wakeup reason (DEBUGGING)
  print_wakeup_reason();

  // Enable all sensors and AWS IoT core
  enableSoilSensor();
  enableLightSensor();
  connectAWS();
}

void loop() {
  for (int i = 0; i < 5; i++) {
    // Sample sensors
    t[i] = ss.getTemp();
    h[i] = ss.touchRead(0);
    l[i] = lightMeter.readLightLevel();

    // Scale from 0-100%
    h[i] = map(h[i], 0, 1023, 0, 100);

    // Serial monitor writes
    Serial.print(F("Soil Moisture: "));
    Serial.print(h[i]);
    Serial.print(F("%, Temperature: "));
    Serial.print(t[i]);
    Serial.print("°C, Light: ");
    Serial.print(l[i]);
    Serial.println(" lux");
  }
  Serial.println();

  // Write to cloud
  publishMessage();

  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
    // Wakeup by button: turn on face
    Serial.println("Wakeup caused by external signal using RTC_IO");
    int start_time = millis();
    while (millis() - start_time < 2000) client.loop();

    // Delay on face for 10s
    delay(10000);

    // Clear screen
    u8g2.clearBuffer();
    u8g2.sendBuffer();
  }

  else {
    // Clear screen
    u8g2.clearBuffer();
    u8g2.sendBuffer();
  }

  // Sleep for TIME_TO_SLEEP seconds
  Serial.println("PlantFriend going to sleep");
  delay(100);
  goToSleep();
}
