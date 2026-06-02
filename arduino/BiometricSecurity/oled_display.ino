#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"

Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void oledInit() {
  oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  oled.display();
}

void oledShow(const char* line1,
              const char* line2,
              const char* line3) {
  oled.clearDisplay();
  oled.setCursor(0, 0);  oled.println(line1);
  oled.setCursor(0, 22); oled.println(line2);
  oled.setCursor(0, 44); oled.println(line3);
  oled.display();
}

void oledShowLarge(const char* text) {
  oled.clearDisplay();
  oled.setTextSize(2);
  oled.setCursor(0, 20);
  oled.println(text);
  oled.setTextSize(1);
  oled.display();
}