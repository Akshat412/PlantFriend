const int wakeupPin = 7;  // GPIO 1 for external wake-up
RTC_DATA_ATTR int bootCount = 0;  // Number of reboots

void setup() {
  Serial.begin(115200);  // Start serial port at 115200 baud rate
  pinMode(wakeupPin, INPUT_PULLDOWN);  // Declaring the pin

  bootCount++;  // Increment the number of boots by 1
  Serial.println("Boot number: " + String(bootCount));  // Print the boot number

  esp_sleep_enable_ext0_wakeup((gpio_num_t)wakeupPin, HIGH);  // Configure external wake-up
  Serial.println("I'm going to sleep now.");  // Print a statement before entering deep sleep

  delay(5000);  // Adding a 1 second delay to avoid multiple presses

  esp_deep_sleep_start();  // Enter deep sleep mode
}

void loop() {

}