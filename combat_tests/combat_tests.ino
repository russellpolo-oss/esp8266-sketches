
#define D7 13   // fire button
// ================================================================
//  ESP8266 + Adafruit SSD1306 OLED (128×64) Combat Tank Directions Demo
//  Button on D7 (GPIO13) cycles through 0..15 directions
// ================================================================

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1          // or GPIO if you have a reset pin
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Button
#define BUTTON_PIN    D7          // GPIO13 on NodeMCU / ESP8266
#define DEBOUNCE_MS   40

// Your tank definitions (must be in PROGMEM)
#include "combat_defs.h"          // ← contains: const uint8_t tank[16][8] PROGMEM;

// State
uint8_t tank_dir = 0;
unsigned long last_press = 0;

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\nTank Directions Demo – Adafruit SSD1306");

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Initialize OLED (I2C address 0x3C is most common)
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);     // Don't proceed, loop forever
  }

  display.clearDisplay();
  display.display();   // show the blank screen
}

void loop() {
  // ──── Button handling (simple debounce, active LOW) ────
  static bool last_state = HIGH;
  bool current = digitalRead(BUTTON_PIN);

  if (current == LOW && last_state == HIGH) {
    unsigned long now = millis();
    if (now - last_press >= DEBOUNCE_MS) {
      tank_dir = (tank_dir + 1) % 16;
      last_press = now;
      Serial.printf("Direction → %d\n", tank_dir);
    }
  }
  last_state = current;

  // ──── Draw everything ────
  display.clearDisplay();

  // 1. Top row: all 16 directions (tiny 8×8 each)
  for (uint8_t d = 0; d < 16; d++) {
    int x = d * 8;
    // drawBitmap(x, y, bitmap PROGMEM ptr, width, height, color)
    display.drawBitmap(x, 0, tank[d], 8, 8, SSD1306_WHITE);
  }

  // Horizontal separator line
  display.drawFastHLine(0, 9, 128, SSD1306_WHITE);

  // 2. Center tank – normal size, nicely centered
  int center_x = (128 - 8) / 2;    // 60
  int center_y = 20;               // below top row + line

  display.drawBitmap(center_x, center_y, tank[tank_dir], 8, 8, SSD1306_WHITE);

  // Optional: 2× scaled version (blocky but more visible – uncomment if wanted)
  /*
  for (int sy = 0; sy < 8; sy++) {
    uint8_t row = pgm_read_byte(&tank[tank_dir][sy]);
    for (int sx = 0; sx < 8; sx++) {
      if (row & (1 << (7 - sx))) {   // MSB left
        display.fillRect(center_x + sx*2, center_y + sy*2, 2, 2, SSD1306_WHITE);
      }
    }
  }
  */

  // 3. Direction number below the tank
  char buf[12];
  snprintf(buf, sizeof(buf), "dir %d", tank_dir);

  display.setTextSize(1);                // normal size
  display.setTextColor(SSD1306_WHITE);   // white text
  display.setCursor(center_x, center_y + 10);
  display.print(buf);

  // Optional: show small icon of current direction next to number
  display.drawBitmap(center_x + 38, center_y + 9, tank[tank_dir], 8, 8, SSD1306_WHITE);

  display.display();   // send buffer to screen

  delay(20);   // ~50 fps – smooth enough
}