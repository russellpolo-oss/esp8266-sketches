// ================================================================
//  ESP8266 Tank Control Demo with Adafruit SSD1306 OLED
//  - ADS1115 A0: rotation (left/right)
//  - Joystick A0: forward/backward movement (analog speed)
//  - Floating point position for smooth motion
// ================================================================

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_ADS1X15.h>

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

Adafruit_ADS1115 ads;

// Pin definitions
#define D7 13   // fire button


#define JOY_PIN       A0        // ESP8266 analog joystick (vertical axis)
#define BUTTON_PIN    D7        // GPIO13 - fire button (unused for now)

// Thresholds
#define ROT_LEFT_TH   20000     // ADS < this → rotate left
#define ROT_RIGHT_TH  8000      // ADS > this → rotate right
#define JOY_CENTER    512
#define JOY_DEADZONE  80        // ±80 around center = no movement

// Movement tuning
#define MAX_SPEED     0.8f      // pixels per frame at full deflection
#define ROT_SPEED     120.0f    // milliseconds per direction change at full deflection

// Tank state
float tank_x = (SCREEN_WIDTH - 8) / 2.0f;   // ~60.0
float tank_y = (SCREEN_HEIGHT - 8) / 2.0f;  // ~28.0
uint8_t tank_dir = 0;                       // 0 = up, 4 = right, etc.

// Timing for rotation rate limiting
unsigned long last_rot_time = 0;

// Your tank bitmaps
#include "combat_defs.h"   // const uint8_t tank[16][8] PROGMEM;

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\nSmooth Tank Control Demo");

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 init failed"));
    for(;;);
  }

  if (!ads.begin()) {
    Serial.println("ADS1115 not found!");
    for(;;);
  }
  ads.setGain(GAIN_ONE);

  display.clearDisplay();
  display.display();
}

void loop() {
  unsigned long now = millis();

  // ─── Read inputs ───
  int16_t rot_value = ads.readADC_SingleEnded(0);   // ADS1115 A0 (0..32767)
  int     joy_value = analogRead(JOY_PIN);          // ESP8266 A0 (0..1023)

  // ─── Rotation (simple threshold + rate limit) ───
  if (rot_value > ROT_LEFT_TH) {
    if (now - last_rot_time > 120) {  // 120 ms cooldown – adjust as needed
      if (tank_dir == 0) tank_dir = 15;
      else tank_dir--;
      last_rot_time = now;
    }
  }
  else if (rot_value < ROT_RIGHT_TH) {
    if (now - last_rot_time > 120) {
      tank_dir = (tank_dir + 1) % 16;
      last_rot_time = now;
    }
  }

  // ─── Movement (analog speed with floating point position) ───
  float joy_norm = (float)(joy_value - JOY_CENTER) / (1023.0f - JOY_CENTER);

  float speed = 0.0f;
  if (fabs(joy_norm) > (JOY_DEADZONE / 512.0f)) {
    speed = constrain(joy_norm, -1.0f, 1.0f) * MAX_SPEED;
  }

  if (speed != 0.0f) {
 // 16-direction movement tables (direction 0 = UP, clockwise)
// Generated with sin/cos - direction 0 is straight up

static const float dx_table[16] = {
     0.000f,   // dir  0  ( -90.0°)
     0.383f,   // dir  1  ( -67.5°)
     0.707f,   // dir  2  ( -45.0°)
     0.924f,   // dir  3  ( -22.5°)
     1.000f,   // dir  4  (   0.0°)
     0.924f,   // dir  5  (  22.5°)
     0.707f,   // dir  6  (  45.0°)
     0.383f,   // dir  7  (  67.5°)
     0.000f,   // dir  8  (  90.0°)
    -0.383f,   // dir  9  ( 112.5°)
    -0.707f,   // dir 10  ( 135.0°)
    -0.924f,   // dir 11  ( 157.5°)
    -1.000f,   // dir 12  ( 180.0°)
    -0.924f,   // dir 13  ( 202.5°)
    -0.707f,   // dir 14  ( 225.0°)
    -0.383f,    // dir 15  ( 247.5°)
};

static const float dy_table[16] = {
    -1.000f,   // dir  0  ( -90.0°)
    -0.924f,   // dir  1  ( -67.5°)
    -0.707f,   // dir  2  ( -45.0°)
    -0.383f,   // dir  3  ( -22.5°)
     0.000f,   // dir  4  (   0.0°)
     0.383f,   // dir  5  (  22.5°)
     0.707f,   // dir  6  (  45.0°)
     0.924f,   // dir  7  (  67.5°)
     1.000f,   // dir  8  (  90.0°)
     0.924f,   // dir  9  ( 112.5°)
     0.707f,   // dir 10  ( 135.0°)
     0.383f,   // dir 11  ( 157.5°)
     0.000f,   // dir 12  ( 180.0°)
    -0.383f,   // dir 13  ( 202.5°)
    -0.707f,   // dir 14  ( 225.0°)
    -0.924f,    // dir 15  ( 247.5°)
};
    float dx = dx_table[tank_dir];
    float dy = dy_table[tank_dir];

    float new_x = tank_x + dx * speed;
    float new_y = tank_y + dy * speed;

    // Clamp to screen edges (tank is 8×8)
    tank_x = constrain(new_x, 0.0f, (float)SCREEN_WIDTH - 8.0f);
    tank_y = constrain(new_y, 0.0f, (float)SCREEN_HEIGHT - 8.0f);
  }

  // ─── Draw ───
  display.clearDisplay();

  // Top row: all 16 directions
  for (uint8_t d = 0; d < 16; d++) {
    int x = d * 8;
    display.drawBitmap(x, 0, tank[d], 8, 8, SSD1306_WHITE);
  }

  display.drawFastHLine(0, 9, 128, SSD1306_WHITE);

  // Main tank – truncate float to int for drawing
  display.drawBitmap((int)tank_x, (int)tank_y, tank[tank_dir], 8, 8, SSD1306_WHITE);

  // Info text (bottom left for more space)
  char buf[32];
  snprintf(buf, sizeof(buf), "dir %d  joy:%d rot:%d", tank_dir, joy_value, rot_value);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(4, SCREEN_HEIGHT - 10);
  display.print(buf);

  display.display();

  delay(10);  // ~100 fps loop
}