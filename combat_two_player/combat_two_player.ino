#include <espnow.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_ADS1X15.h>
#include <ESP8266WiFi.h>

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




#include "russell_esp8266_oled_common.h"
#include "russell_audio.h"
#include "pong_game_defs.h"
#include "russell_espnow_common.h"
#include "combat_defs.h"




// Tank state locally stored as floats. 
float tank_M_x = 10.0f;                     // start near left edge
float tank_M_y = (SCREEN_HEIGHT - 8) / 2.0f;  // ~28.0

float tank_C_x = 118.0f;                    // start near right edge
float tank_C_y = (SCREEN_HEIGHT - 8) / 2.0f;

float shell_M_x = 0.0f;         // tack shell postions as floats.            
float shell_M_y = 0.0f; 
int shell_M_n = 0;           // track life of shell in frames (0 = not active, >0 = active)   
int8_t shell_M_dir = 0;       // direction of shell (0-15) if active    
float shell_C_x = 0.0f;                   
float shell_C_y = 0.0f;  
int shell_C_n = 0;
int8_t shell_C_dir = 0;

#define MAX_SHELL_LIFE  120   // frames before shell disappears

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

 WiFi.mode(WIFI_STA);
WiFi.disconnect();
delay(100);
  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW init failed");
    for(;;);
  } 

  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_recv_cb(onDataRecv);  // ← ESP-NOW receive callback active

Serial.println("Discovery broadcasts every ~800 ms until partner found");

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

/// conflicts or timeout ?? 

// Check for detected conflict (from incoming INPUT)
if (roleConflictDetected && partnerFound) {
  partnerFound = false;
  roleConflictDetected = false;
  gameState = STATE_SEARCHING;
  localReady = remoteReady = false;
  // Optional: clear peer list if you want (but not strictly needed)
  Serial.println("Conflict reset → back to Searching");
}
/* check for timeout */
  if (partnerFound && gameState != STATE_SEARCHING) {
    if (millis() - lastPacketFromPartner > PARTNER_TIMEOUT_MS) {
      Serial.println("No packets from partner for 5s → timeout! Reverting to Searching...");
      partnerFound = false;
      localReady = remoteReady = false;
      roleConflictDetected = false;
      gameState = STATE_SEARCHING;
     
      
    }
  }

  /* ===== DISCOVERY ===== */
if (!partnerFound && millis() - lastDiscoverySend > 800) {  // was 500
  sendDiscovery();
  lastDiscoverySend = millis();
Serial.println("Discovery packet sent");
}


if (!isMaster) {
  /* ===== SEND INPUT ===== */
  if (partnerFound && millis() - lastInputSend > 30) {
    sendCombatInput(); // reads all the raw data and sends to master for processing
    lastInputSend = millis();
  }
} // client just sents raw values to master
else { 
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

    float dx = dx_table[combat.tank_M_dir];
    float dy = dy_table[combat.tank_M_dir];

    float new_x = tank_M_x + dx * speed;
    float new_y = tank_M_y + dy * speed;
    // Clamp to screen edges (tank is 8×8)
    tank_M_x = constrain(new_x, 0.0f, (float)SCREEN_WIDTH - 8.0f);
    tank_M_y = constrain(new_y, 0.0f, (float)SCREEN_HEIGHT - 8.0f);

  }

  // detect fire button press to launch shell
  if (fire_value == LOW && shell_M_n == 0) { // only fire if no active shell
    shell_M_n = 1; // activate shell
    shell_M_x = tank_M_x + 4.0f; // start at center of tank
    shell_M_y = tank_M_y + 4.0f;
    shell_M_dir = combat.tank_M_dir; // fire in direction tank is facing
    combat.shell_M_X = (int8_t)shell_M_x; // update combat struct for network transmission
    combat.shell_M_Y = (int8_t)shell_M_y;
    shellSound();
  }

// FIXED shell movement - with CLAMP + proper bounce detection
if (shell_M_n > 0) {
    float shell_speed = 2.0f;
    // Move shell
    shell_M_x += dx_table[shell_M_dir] * shell_speed;
    shell_M_y += dy_table[shell_M_dir] * shell_speed;   
    
    // ← NEW: CLAMP positions to screen bounds (prevents deep overshoot & oscillation)
    shell_M_x = constrain(shell_M_x, 0.0f, 127.0f);
    shell_M_y = constrain(shell_M_y, 0.0f, 63.0f);
    
    // Update combat struct (integer position for drawing/network)
    combat.shell_M_X = (int8_t)shell_M_x;
    combat.shell_M_Y = (int8_t)shell_M_y;
    
    // Life tracking
    shell_M_n++;
    if (shell_M_n > MAX_SHELL_LIFE) {
        shell_M_n = 0;
        combat.shell_M_X = -1;
        combat.shell_M_Y = -1;
    }
    else{
    // Wall collision check at CLAMPED integer position
    if (wall_hit(frame_bitmap[current_background_frame], combat.shell_M_X, combat.shell_M_Y)) {
        // ← FIXED: Proper detection - vertical wall if AT LEAST ONE horizontal neighbor is OPEN (empty)
        // Works perfectly for thin vertical walls (borders/middle) vs. full horizontal walls (top/bottom)
        bool hit_vertical = (!wall_hit(frame_bitmap[current_background_frame], combat.shell_M_X - 1, combat.shell_M_Y)) ||
                            (!wall_hit(frame_bitmap[current_background_frame], combat.shell_M_X + 1, combat.shell_M_Y));
        
        if (hit_vertical) {
            shell_M_dir = bounce_table_v[shell_M_dir];  // Reverse horizontal component
        } else {
            shell_M_dir = bounce_table_h[shell_M_dir];  // Reverse vertical component
        }
        // bounceSound();  // ← SUGGEST: rename from fireSound() to avoid confusion
        fireSound();
      }
    };
}
// send client update 
if (partnerFound && millis() - lastInputSend > 30) {
    sendCombatState(); // master sends full state to client every frame for drawing and client input processing 
    lastInputSend = millis();
    };
}; // end of master-only logic

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
  
  // draw shells if active
  if (shell_M_n > 0) {
    display.fillCircle(combat.shell_M_X + 2, combat.shell_M_Y + 2, 2, SSD1306_WHITE); // simple circle for shell
  }
  if (shell_C_n > 0) {
    display.fillCircle(combat.shell_C_X + 2, combat.shell_C_Y + 2, 2, SSD1306_WHITE);
  } 
  
 // Draw negoations and status
 if (gameState == STATE_SEARCHING) {
  display.setCursor(20, 28);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.print("Searching...");
}

if (gameState == STATE_READY) {
  // Button handling — always run when in READY
  static bool lastBtn = HIGH;
  bool btn = digitalRead(BTN_PIN);

  if (lastBtn == HIGH && btn == LOW && !localReady) {
    localReady = true;
    sendReady();
    Serial.println("Local ready pressed & sent PKT_READY");
  }
  lastBtn = btn;

  // Draw ready screen elements
  display.setCursor(50, 22);
  display.print("Ready?");

  display.setCursor(30, 34);
  display.print(localReady  ? "You: OK" : "You: --");

  display.setCursor(30, 44);
  display.print(remoteReady ? "Them: OK" : "Them: --");

}

  
  display.display();
  updateSound();

  delay(10);  // ~100 fps loop
}