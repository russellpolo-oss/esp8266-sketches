#include <espnow.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_ADS1X15.h>
#include <ESP8266WiFi.h>
#include "russell_esp8266_oled_common.h"
#include "russell_audio.h"
#include "pong_game_defs.h"
#include "russell_espnow_common.h"
#include "combat_defs.h"

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

// Tank state locally stored as floats. 
float tank_M_x = 10.0f;                     // start near left edge
float tank_M_y = (SCREEN_HEIGHT - 8) / 2.0f;  // ~28.0

float tank_C_x = 118.0f;                    // start near right edge
float tank_C_y = (SCREEN_HEIGHT - 8) / 2.0f;

// Timing for rotation rate limiting
unsigned long last_rot_time = 0;


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

  // set starting tank directions. 
combat.tank_M_dir = 4;                       // 0 = up, 4 = right, etc.
combat.tank_C_dir = 12;                      // 12 = left  



  display.clearDisplay();
  display.display();
}

void loop() {
  unsigned long now = millis();

  // ─── Read inputs ───
  int16_t rot_value = ads.readADC_SingleEnded(0);   // ADS1115 A0 (0..32767)
  int     joy_value = analogRead(JOY_PIN);          // ESP8266 A0 (0..1023)
  int    fire_value = digitalRead(BUTTON_PIN);    // LOW when pressed

  if (fire_value == LOW) {
    // Fire button pressed - for demo, just trigger the sound effect
    engineSound();
  }

  // ─── Rotation (simple threshold + rate limit) ───
  if (rot_value > ROT_LEFT_TH) {
    if (now - last_rot_time > 120) {  // 120 ms cooldown – adjust as needed
      if (combat.tank_M_dir == 0) combat.tank_M_dir = 15;
      else combat.tank_M_dir--;
      last_rot_time = now;
    }
  }
  else if (rot_value < ROT_RIGHT_TH) {
    if (now - last_rot_time > 120) {
      combat.tank_M_dir = (combat.tank_M_dir + 1) % 16;
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
    float dx = dx_table[combat.tank_M_dir];
    float dy = dy_table[combat.tank_M_dir];

    float new_x = tank_M_x + dx * speed;
    float new_y = tank_M_y + dy * speed;
    // Clamp to screen edges (tank is 8×8)
    tank_M_x = constrain(new_x, 0.0f, (float)SCREEN_WIDTH - 8.0f);
    tank_M_y = constrain(new_y, 0.0f, (float)SCREEN_HEIGHT - 8.0f);

  }

  // ─── Draw ───
  display.clearDisplay();

  // draw the playfield
  display.drawBitmap(0, 0, frame_bitmap[current_background_frame], 128, 64, SSD1306_WHITE);
  
      // save int values in the combat struct for drawing and network transmission
    combat.tank_M_X = (int)tank_M_x;
    combat.tank_M_Y = (int)tank_M_y; 

    combat.tank_C_X = (int)tank_C_x;
    combat.tank_C_Y = (int)tank_C_y;   


  // Master tank – truncate float to int for drawing
  display.drawBitmap(combat.tank_M_X, combat.tank_M_Y, tank[combat.tank_M_dir], 8, 8, SSD1306_WHITE);
  // Client tank  
  display.drawBitmap(combat.tank_C_X, combat.tank_C_Y, tank[combat.tank_C_dir], 8, 8, SSD1306_WHITE);
  display.display();
  updateSound();

  delay(10);  // ~100 fps loop
}