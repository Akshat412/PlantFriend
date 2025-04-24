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
#define TIME_TO_SLEEP  10          // Time ESP32 will go to sleep (in seconds)

// Globals for temperature, soil moisture, and light
float t[5];  // Temperature in °C
float h[5];  // Soil moisture as a percentage
float l[5];  // Light in lux

WiFiClientSecure net = WiFiClientSecure();
PubSubClient client(net);

// The soil moisture sensor will be on the I2C bus as 0x36
Adafruit_seesaw ss;

// The light sensor will be on the bus as 0x23
BH1750 lightMeter;

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

void u8g2_prepare(void) {
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setFontRefHeightExtendedText();
  u8g2.setDrawColor(1);
  u8g2.setFontPosTop();
  u8g2.setFontDirection(0);
}

// void u8g2_happy(uint8_t a) {
//   u8g2.drawStr( 10, 10, "happy");

//   // Face outline
//   u8g2.drawCircle(64, 40, 20, U8G2_DRAW_ALL);

//   // Eyes
//   u8g2.drawDisc(56, 35, 2, U8G2_DRAW_ALL);  // Left eye
//   u8g2.drawDisc(72, 35, 2, U8G2_DRAW_ALL);  // Right eye

//   // Smile using an arc (top half of a circle)
//   for (int angle = 30; angle <= 150; angle += 10) {
//     float rad1 = angle * 3.14159 / 180.0;
//     float rad2 = (angle + 10) * 3.14159 / 180.0;
//     int x1 = 64 + cos(rad1) * 10;
//     int y1 = 47 + sin(rad1) * 10;  // moved down slightly for placement
//     int x2 = 64 + cos(rad2) * 10;
//     int y2 = 47 + sin(rad2) * 10;
//     u8g2.drawLine(x1, y1, x2, y2);
//   }

//   u8g2.sendBuffer();
// }

void u8g2_happy(uint8_t a) {
  u8g2.clearBuffer();

  // Face (plant head)
  u8g2.drawCircle(64, 35, 20, U8G2_DRAW_ALL);

  // Happy eyes
  u8g2.drawDisc(60, 28, 1, U8G2_DRAW_ALL);  // Left eye
  u8g2.drawDisc(68, 28, 1, U8G2_DRAW_ALL);  // Right eye

  // Smiling mouth (arc from 20° to 160°)
  for (int angle = 20; angle <= 160; angle += 10) {
    float rad1 = angle * 3.14159 / 180.0;
    float rad2 = (angle + 10) * 3.14159 / 180.0;
    int x1 = 64 + cos(rad1) * 6;
    int y1 = 38 + sin(rad1) * 6;
    int x2 = 64 + cos(rad2) * 6;
    int y2 = 38 + sin(rad2) * 6;
    u8g2.drawLine(x1, y1, x2, y2);
  }

  // Stem
  u8g2.drawLine(64, 47, 64, 60);

  // Leaves
  u8g2.drawEllipse(58, 52, 4, 2, U8G2_DRAW_ALL); // Left leaf
  u8g2.drawEllipse(70, 52, 4, 2, U8G2_DRAW_ALL); // Right leaf

  // Pot
  u8g2.drawBox(56, 60, 16, 6);               // Base
  u8g2.drawFrame(54, 60, 20, 4);             // Top lip

  u8g2.sendBuffer();
}

void u8g2_sad(uint8_t a) {
  // Draw pot (simple rounded pot)
  u8g2.drawBox(48, 40, 40, 20);           // Pot base
  u8g2.drawFrame(44, 36, 48, 6);          // Pot rim
  
  u8g2.setDrawColor(0);  // Set color to black
  // Smile
  for (int angle = 30; angle <= 150; angle += 10) {
    float rad1 = angle * 3.14159 / 180.0;
    float rad2 = (angle + 10) * 3.14159 / 180.0;
    int x1 = 64 + cos(rad1) * 10;
    int y1 = 57 + sin(rad1) * 10;  // moved down slightly for placement
    int x2 = 64 + cos(rad2) * 10;
    int y2 = 57 + sin(rad2) * 10;
    u8g2.drawLine(x1, y1, x2, y2);
  }

  // Eyes
  u8g2.drawDisc(58, 51, 1, U8G2_DRAW_ALL);  // Left eye
  u8g2.drawDisc(70, 51, 1, U8G2_DRAW_ALL);  // Right eye

  u8g2.setDrawColor(1);  // Set color to black

  // Center Leaf (vertical)
  u8g2.drawEllipse(64, 20, 18, 10);  // Vertical oval (fat center leaf)
  u8g2.drawLine(64, 10, 64, 30);     // Leaf center vein (line)

  // Left Leaf (angled, with midline)
  u8g2.drawEllipse(52, 26, 18, 10);  // Left oval leaf
  u8g2.drawLine(52, 16, 52, 36);     // Leaf center vein (line)

  // Right Leaf (angled, with midline)
  u8g2.drawEllipse(76, 26, 18, 10);  // Right oval leaf
  u8g2.drawLine(76, 16, 76, 36);     // Leaf center vein (line)

  u8g2.sendBuffer();
}


void u8g2_sade(uint8_t a) {

  u8g2.clearBuffer();

  // Face (plant head)
  u8g2.drawCircle(64, 35, 20, U8G2_DRAW_ALL);

  // Happy eyes
  u8g2.drawDisc(60, 28, 1, U8G2_DRAW_ALL);  // Left eye
  u8g2.drawDisc(68, 28, 1, U8G2_DRAW_ALL);  // Right eye
  // Smiling mouth (arc from 200° to 340°)
  for (int angle = 200; angle <= 340; angle += 10) {
    float rad1 = angle * 3.14159 / 180.0;
    float rad2 = (angle + 10) * 3.14159 / 180.0;
    int x1 = 64 + cos(rad1) * 6;
    int y1 = 38 + sin(rad1) * 6;
    int x2 = 64 + cos(rad2) * 6;
    int y2 = 38 + sin(rad2) * 6;
    u8g2.drawLine(x1, y1, x2, y2);
  }

  // Stem
  u8g2.drawLine(64, 47, 64, 60);

  // Leaves
  u8g2.drawEllipse(58, 52, 4, 2, U8G2_DRAW_ALL); // Left leaf
  u8g2.drawEllipse(70, 52, 4, 2, U8G2_DRAW_ALL); // Right leaf

  // Pot
  u8g2.drawBox(56, 60, 16, 6);               // Base
  u8g2.drawFrame(54, 60, 20, 4);             // Top lip

  u8g2.sendBuffer();

  // u8g2.clearBuffer();
  // u8g2.setFont(u8g2_font_6x10_tr);
  // u8g2.drawStr(0, 0, "low on water!");

  // // Draw shriveled pot
  // u8g2.drawFrame(54, 50, 20, 10);     // Pot base
  // u8g2.drawLine(54, 50, 50, 45);      // Left slant
  // u8g2.drawLine(74, 50, 78, 45);      // Right slant
  // u8g2.drawLine(50, 45, 78, 45);      // Pot top

  // // Drooping stem
  // u8g2.drawLine(64, 45, 62, 30);      // Slight lean to the left
  // u8g2.drawLine(62, 30, 60, 25);      // Drooping tip

  // // Limp leaf
  // u8g2.drawLine(62, 35, 58, 33);      // Small left droop leaf
  // u8g2.drawLine(58, 33, 56, 31);

  // // Bigger X-eyes (Left)
  // u8g2.drawLine(54, 18, 60, 24);
  // u8g2.drawLine(60, 18, 54, 24);

  // // Bigger X-eyes (Right)
  // u8g2.drawLine(66, 18, 72, 24);
  // u8g2.drawLine(72, 18, 66, 24);

  // // Dry, cracked mouth
  // u8g2.drawLine(60, 26, 68, 26);
  // u8g2.drawPixel(62, 25);
  // u8g2.drawPixel(66, 27);

  // // Little “cracked soil” lines
  // u8g2.drawLine(52, 60, 54, 58);
  // u8g2.drawLine(74, 60, 76, 58);
  // u8g2.drawPixel(64, 60);

  // u8g2.sendBuffer();
}

void u8g2_wet(uint8_t a) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 0, "I'm wet! [water high]");

  // Smiley face outline
  u8g2.drawCircle(64, 40, 20, U8G2_DRAW_ALL);

  // Happy eyes
  u8g2.drawDisc(56, 33, 2, U8G2_DRAW_ALL);  // Left eye
  u8g2.drawDisc(72, 33, 2, U8G2_DRAW_ALL);  // Right eye

  // Big smile (arc from 200° to 340°)
  for (int angle = 200; angle <= 340; angle += 10) {
    float rad1 = angle * 3.14159 / 180.0;
    float rad2 = (angle + 10) * 3.14159 / 180.0;
    int x1 = 64 + cos(rad1) * 10;
    int y1 = 45 + sin(rad1) * 10;
    int x2 = 64 + cos(rad2) * 10;
    int y2 = 45 + sin(rad2) * 10;
    u8g2.drawLine(x1, y1, x2, y2);
  }

  // Raindrops falling
  u8g2.drawLine(30, 10, 30, 14);
  u8g2.drawLine(50, 5, 50, 10);
  u8g2.drawLine(70, 8, 70, 13);
  u8g2.drawLine(90, 6, 90, 12);

  // Water wave at bottom — using arcs like a sine wave
  for (int x = 10; x < 120; x += 8) {
    u8g2.drawArc(x, 64, 4, 180, 360);  // Half-circle (bottom wave)
  }

  // Little splashes left and right
  u8g2.drawLine(45, 58, 43, 56);
  u8g2.drawLine(83, 58, 85, 56);
  u8g2.drawLine(47, 59, 45, 57);
  u8g2.drawLine(81, 59, 83, 57);

  // Optional: puddle highlight under the face
  u8g2.drawEllipse(64, 68, 20, 4, U8G2_DRAW_ALL);

  u8g2.sendBuffer();
}




void u8g2_string(uint8_t a) {
  u8g2.setFontDirection(0);
  u8g2.drawStr(30+a,31, " 0");
  u8g2.setFontDirection(1);
  u8g2.drawStr(30,31+a, " 90");
  u8g2.setFontDirection(2);
  u8g2.drawStr(30-a,31, " 180");
  u8g2.setFontDirection(3);
  u8g2.drawStr(30,31-a, " 270");
}

void u8g2_light_high(uint8_t a) {
  u8g2.drawStr( 0, 0, "Sunny  plant");

  // draw sunny face   
  // Center coordinates of the face
  int cx = 64;
  int cy = 40;
  int r = 20;

  // Draw sun rays around the face
  for (int angle = 0; angle < 360; angle += 30) {
    float rad = angle * 3.14159 / 180.0;
    int x1 = cx + cos(rad) * (r + 5);
    int y1 = cy + sin(rad) * (r + 5);
    int x2 = cx + cos(rad) * (r + 12);
    int y2 = cy + sin(rad) * (r + 12);
    u8g2.drawLine(x1, y1, x2, y2);
  }

  // Face outline (sun face)
  u8g2.drawCircle(cx, cy, r, U8G2_DRAW_ALL);

  // Squinting eyes
  u8g2.drawLine(cx - 8, cy - 5, cx - 4, cy - 5);
  u8g2.drawLine(cx + 4, cy - 5, cx + 8, cy - 5);

  // Overheated expression (tongue out)
  u8g2.drawCircle(cx, cy + 7, 5, U8G2_DRAW_ALL);  // Mouth outline
  u8g2.drawDisc(cx, cy + 9, 2, U8G2_DRAW_ALL);    // Tongue sticking out

  // Message
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(25, 64, "Too Much Sun!");
  
  u8g2.sendBuffer();
}

void drawToughPlantFace(uint8_t a) {
  u8g2.clearBuffer();

  // Draw oval cactus body
  u8g2.drawFrame(54, 20, 20, 35); // main body
  u8g2.drawFrame(49, 25, 5, 15);  // left arm
  u8g2.drawFrame(74, 25, 5, 15);  // right arm

  // Spikes
  for (int i = 0; i < 5; i++) {
    u8g2.drawPixel(56 + i * 3, 22); // top row
    u8g2.drawPixel(56 + i * 3, 50); // bottom row
    if (i < 4) {
      u8g2.drawPixel(54, 26 + i * 5); // left side
      u8g2.drawPixel(73, 26 + i * 5); // right side
    }
  }

  // Eyes (angry looking)
  u8g2.drawDisc(59, 32, 2); // left eye
  u8g2.drawDisc(69, 32, 2); // right eye
  u8g2.drawLine(57, 30, 61, 28); // left brow
  u8g2.drawLine(67, 28, 71, 30); // right brow

  // Mouth (serious face)
  u8g2.drawLine(60, 40, 68, 40); // straight mouth

  // Text
  u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.drawStr(38, 62, "I'm built different 💪🌵");

  u8g2.sendBuffer();
}

void u8g2_triangle(uint8_t a) {
  uint16_t offset = a;
  u8g2.drawStr( 0, 0, "drawTriangle");
  u8g2.drawTriangle(14,7, 45,30, 10,40);
  u8g2.drawTriangle(14+offset,7-offset, 45+offset,30-offset, 57+offset,10-offset);
  u8g2.drawTriangle(57+offset*2,10, 45+offset*2,30, 86+offset*2,53);
  u8g2.drawTriangle(10+offset,40+offset, 45+offset,30+offset, 86+offset,53+offset);
}

void u8g2_ascii_1() {
  char s[2] = " ";
  uint8_t x, y;
  u8g2.drawStr( 0, 0, "ASCII page 1");
  for( y = 0; y < 6; y++ ) {
    for( x = 0; x < 16; x++ ) {
      s[0] = y*16 + x + 32;
      u8g2.drawStr(x*7, y*10+10, s);
    }
  }
}

void u8g2_ascii_2() {
  char s[2] = " ";
  uint8_t x, y;
  u8g2.drawStr( 0, 0, "ASCII page 2");
  for( y = 0; y < 6; y++ ) {
    for( x = 0; x < 16; x++ ) {
      s[0] = y*16 + x + 160;
      u8g2.drawStr(x*7, y*10+10, s);
    }
  }
}

void u8g2_extra_page(uint8_t a)
{
  u8g2.drawStr( 0, 0, "Unicode");
  u8g2.setFont(u8g2_font_unifont_t_symbols);
  u8g2.setFontPosTop();
  u8g2.drawUTF8(0, 24, "☀ ☁");
  switch(a) {
    case 0:
    case 1:
    case 2:
    case 3:
      u8g2.drawUTF8(a*3, 36, "☂");
      break;
    case 4:
    case 5:
    case 6:
    case 7:
      u8g2.drawUTF8(a*3, 36, "☔");
      break;
  }
}

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

void u8g2_bitmap_overlay(uint8_t a) {
  uint8_t frame_size = 28;

  u8g2.drawStr(0, 0, "Bitmap overlay");

  u8g2.drawStr(0, frame_size + 12, "Solid / transparent");
  u8g2.setBitmapMode(false /* solid */);
  u8g2.drawFrame(0, 10, frame_size, frame_size);
  u8g2.drawXBMP(2, 12, cross_width, cross_height, cross_bits);
  if(a & 4)
    u8g2.drawXBMP(7, 17, cross_block_width, cross_block_height, cross_block_bits);

  u8g2.setBitmapMode(true /* transparent*/);
  u8g2.drawFrame(frame_size + 5, 10, frame_size, frame_size);
  u8g2.drawXBMP(frame_size + 7, 12, cross_width, cross_height, cross_bits);
  if(a & 4)
    u8g2.drawXBMP(frame_size + 12, 17, cross_block_width, cross_block_height, cross_block_bits);

}

void u8g2_bitmap_modes(uint8_t transparent) {
  const uint8_t frame_size = 24;

  u8g2.drawBox(0, frame_size * 0.5, frame_size * 5, frame_size);
  u8g2.drawStr(frame_size * 0.5, 50, "Black");
  u8g2.drawStr(frame_size * 2, 50, "White");
  u8g2.drawStr(frame_size * 3.5, 50, "XOR");
  
  if(!transparent) {
    u8g2.setBitmapMode(false /* solid */);
    u8g2.drawStr(0, 0, "Solid bitmap");
  } else {
    u8g2.setBitmapMode(true /* transparent*/);
    u8g2.drawStr(0, 0, "Transparent bitmap");
  }
  u8g2.setDrawColor(0);// Black
  u8g2.drawXBMP(frame_size * 0.5, 24, cross_width, cross_height, cross_bits);
  u8g2.setDrawColor(1); // White
  u8g2.drawXBMP(frame_size * 2, 24, cross_width, cross_height, cross_bits);
  u8g2.setDrawColor(2); // XOR
  u8g2.drawXBMP(frame_size * 3.5, 24, cross_width, cross_height, cross_bits);
}

uint8_t draw_state = 0;

void draw(void) {
  u8g2_prepare();
  switch(draw_state >> 3) {
    case 0: u8g2_happy(draw_state&7); break;
    case 1: u8g2_happy(draw_state&7); break;
    case 2: u8g2_sad(draw_state&7); break;
    case 3: u8g2_sad(draw_state&7); break;
    case 4: u8g2_wet(draw_state&7); break;
    case 5: u8g2_wet(draw_state&7); break;
    case 6: u8g2_wet(draw_state&7); break;
    case 7: u8g2_light_high(draw_state&7); break;
    case 8: u8g2_light_high(draw_state&7); break;
    // case 5: u8g2_triangle(draw_state&7); break;
    // case 6: u8g2_ascii_1(); break;
    // case 7: u8g2_ascii_2(); break;
    // case 8: u8g2_extra_page(draw_state&7); break;
    case 9: drawToughPlantFace(draw_state&7); break;
    case 10: drawToughPlantFace(draw_state&7); break;
    case 11: drawToughPlantFace(draw_state&7); break;
    // case 10: u8g2_bitmap_modes(1); break;
    // case 11: u8g2_bitmap_overlay(draw_state&7); break;
  }
}


// void setup(void) {
//   u8g2.begin();
// }

// void loop(void) {
//   // picture loop  
//   u8g2.clearBuffer();
//   draw();
//   u8g2.sendBuffer();
  
//   // increase the state
//   draw_state++;
//   if ( draw_state >= 12*8 )
//     draw_state = 0;

//   // delay between each page
//   delay(100);

// }

//NOTE: second code i tried - no wifi, no aws, just diplay and blink w debugging statements


void setup() {
  Serial.begin(115200);
  
  connectAWS();
  u8g2.begin(); // set up display
}

void loop() {
  // // picture loop  
  // maybe uncomment this later TODO sanj
  // u8g2.clearBuffer();
  // draw();
  // u8g2.sendBuffer();
  
  
  // increase the state
  // do not change the state 
  // draw_state++;
  // if ( draw_state >= 12*8 )
  //   draw_state = 0;

  // delay between each page
  // delay(100);
  client.loop();  // ← THIS is needed to trigger the callback

  delay(5000);
}



// // Method to print the reason by which ESP32 has been awaken from sleep
// void print_wakeup_reason(){
//   esp_sleep_wakeup_cause_t wakeup_reason;

//   wakeup_reason = esp_sleep_get_wakeup_cause();

//   switch(wakeup_reason)
//   {
//     case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
//     case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
//     case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
//     case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
//     case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
//     default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
//   }
// }

// // void goToSleep() {
// //   // Configure the wake up source: set our ESP32 to wake up TIME_TO_SLEEP seconds
// //   esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

// //   Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) +
// //   " Seconds");
// //   // delay(100);

// //   Serial.flush(); 
// //   // esp_deep_sleep_start(); // comment back later 
// // }

// void enableSoilSensor() {
//   Serial.println("Enabling soil sensor");
  
//   if (!ss.begin(0x36)) {
//     Serial.println("ERROR! Sensor not found");
//     while(1) delay(1);
//   } else {
//     Serial.print("Soil sensor started! Firmware Version: ");
//     Serial.println(ss.getVersion(), HEX);
//   }
// }

// void enableLightSensor() {
//   Serial.println("Enabling light sensor");
//   lightMeter.begin();
//   Serial.println("Enabled light sensor");
// }
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

  // char jsonBuffer[512];
  // serializeJson(doc, jsonBuffer); // print to client

  // client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);

  doc["status"] = "test";
  doc["message"] = "Hello from Plant Friend!";
  doc["timestamp"] = millis(); // just to include something dynamic

  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer); // print to client

  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
}


void messageHandler(char* topic, byte* payload, unsigned int length) {
  Serial.println("in message handler at least");
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, payload, length);

  if (error) {
    Serial.print("Deserialization failed: ");
    Serial.println(error.c_str());
    return;
  }

  String message = doc["message"].as<String>(); // extract as String

  Serial.print("Message received from AWS: ");
  Serial.println(message);


/*
too much light
too little light
too much water
too little water
too hot 
too cold
*/
  // shortcut 
  u8g2.clearBuffer();
  if (message == "happy") {
    Serial.println("the message is HAPPY and calling draw function now");
    u8g2_happy(0);
  } else if (message == "sad") {
    Serial.println("the message is SAD and calling draw function now");
    u8g2_sad(0);
  } else {
    Serial.println("else case ded");

  }
  // } else if (message == "wet") {
  //   Serial.println("the message is WET and calling draw function now");
  //   u8g2_wet(0);
  // } else if (message == "sunny") {
  //   Serial.println("the message is SUNNY and calling draw function now");
  //   u8g2_light_high(0);
  // } else if (message == "tough") {
  //   Serial.println("the message is TOUGH and calling draw function now");
  //   drawToughPlantFace(0);
  // } else {
  //   Serial.println("the message is unknown and calling draw function now");
  //   u8g2.drawStr(0, 30, "Unknown msg");
  // }
  
  u8g2.sendBuffer();
}



