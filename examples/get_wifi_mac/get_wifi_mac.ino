#include <WiFi.h>

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_MODE_STA); // Or WIFI_MODE_APSTA for AP and station mode
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());
}

void loop() {
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());
  delay(1000);
}