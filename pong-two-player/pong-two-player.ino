
#include <espnow.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include "russell_esp8266_oled_common.h"
#include "russell_audio.h"
#include "pong_game_defs.h"
#include "russell_espnow_common.h"



/* ===================================================== */
/* ================== DISPLAY ========================== */
/* ===================================================== */

void drawPlayfield() {
  display.drawLine(0, 0, SCREEN_WIDTH, 0, WHITE);
  display.drawLine(0, SCREEN_HEIGHT - 1, SCREEN_WIDTH, SCREEN_HEIGHT - 1, WHITE);
}

void drawPaddles() {
  display.fillRect(LEFT_PADDLE_X, paddleLeftY, PADDLE_WIDTH, PADDLE_HEIGHT, WHITE);
  display.fillRect(RIGHT_PADDLE_X, paddleRightY, PADDLE_WIDTH, PADDLE_HEIGHT, WHITE);
}

/* ===================================================== */
/* ================== INPUT ============================ */
/* ===================================================== */

int readPaddleY() {
  int raw = analogRead(JOY_PIN);
  raw = 1023 - raw; // invert
  int y = map(raw, 0, 1023, 1, SCREEN_HEIGHT - PADDLE_HEIGHT - 1);
  return y;
}

/* helpers*/


void drawBall() {
  display.fillRect(ballX, ballY, BALL_SIZE, BALL_SIZE, WHITE);
}

void resetBall(bool towardLeft) {

  // ----- WIN CHECK -----
  if (scoreLeft > 10 || scoreRight > 10) {

    gameState = STATE_READY;

    localReady  = false;
    remoteReady = false;

    // who won?
    if (scoreLeft > 10) {
      Serial.println("LEFT PLAYER WINS!");
      winSound();
    } else {
      Serial.println("RIGHT PLAYER WINS!");
      looseSound();

    }

    sendForceReady();
    return;   // DO NOT spawn a new ball
  }

  // ----- NORMAL BALL RESET -----
  ballX = SCREEN_WIDTH / 2;
  ballY = random(10, SCREEN_HEIGHT - 10);

  ballVX = towardLeft ? -BALL_SPEED_X : BALL_SPEED_X;
  ballVY = random(-2, 3);
  if (ballVY == 0) ballVY = 1;
}


void drawScores() {
  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.setCursor(12, 2);
  display.print(scoreLeft);
  if (isMaster){
    display.print(" M");
  }
  else 
  {
    display.print(" C");
  }

  display.setCursor(SCREEN_WIDTH - 15, 2);
  display.print(scoreRight);
}



/* ===================================================== */
/* ================== SETUP ============================ */
/* ===================================================== */

void setup() {
  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  Serial.begin(9600);
  //delay(2000);               // give USB time to enumerate
  Serial.println("\n\n===== ESP8266 Pong starting =====");
  Serial.flush();           // force send

 WiFi.mode(WIFI_STA);
WiFi.disconnect();
delay(100);



  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for (;;);
  }

  display.clearDisplay();
  display.display();

  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_recv_cb(onDataRecv);

  randomSeed(ESP.getChipId());
  myNonce = random(1, 255);
      Serial.print("My nonce = ");
    Serial.println(myNonce);
    // in setup, after myNonce print:
Serial.println("Discovery broadcasts every ~800 ms until partner found");
}

/* ===================================================== */
/* ================== LOOP ============================= */
/* ===================================================== */

void loop() {

/* ===== READ LOCAL PADDLE ===== */
int localPaddleY = readPaddleY();

if (isMaster) {
  paddleLeftY = localPaddleY;
} else {
  paddleRightY = localPaddleY;
}

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
      // Optional: reset paddles to center
      paddleLeftY = paddleRightY = 26;
      lastPaddleLeftY = lastPaddleRightY = 26;
    }
  }


  /* ===== SEND INPUT ===== */
  if (partnerFound && millis() - lastInputSend > 30) {
    sendInput(localPaddleY);
    lastInputSend = millis();
  }

  /* ===== DISCOVERY ===== */
if (!partnerFound && millis() - lastDiscoverySend > 800) {  // was 500
  sendDiscovery();
  lastDiscoverySend = millis();
Serial.println("Discovery packet sent");
}

/* ===== DISPLAY ===== */
display.clearDisplay();
drawPlayfield();
drawPaddles();   // always draw paddles — looks nice even in ready state

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

if (gameState == STATE_PLAYING) {
  drawBall();
}

// Scores can be shown in READY too (helps debugging)
drawScores();


if (gameState == STATE_READY) {
  if (localReady && remoteReady) {

    if (isMaster) {
      scoreLeft  = 0;
      scoreRight = 0;

      ballX = 10;
      ballY = SCREEN_HEIGHT / 2;
      ballVX = BALL_SPEED_X;
      ballVY = random(-1, 2);
      if (ballVY == 0) ballVY = 1;
    }

    gameState = STATE_PLAYING;
  }
}


if (gameState == STATE_PLAYING && isMaster) {
  // Compute paddle velocities from last frame to current (before physics)
  int leftPaddleVY  = paddleLeftY  - lastPaddleLeftY;
  int rightPaddleVY = paddleRightY - lastPaddleRightY;

  switch (ballState) {
    case BALLINPLAY:
      ballX += ballVX;
      ballY += ballVY;

      // Bounce top/bottom
      if (ballY <= 1 || ballY >= SCREEN_HEIGHT - BALL_SIZE - 1) {
        ballVY = -ballVY;
      }

      // ----- LEFT PADDLE COLLISION (ENHANCED) -----
      if (ballVX < 0 &&
          ballX <= LEFT_PADDLE_X + PADDLE_WIDTH &&
          ballX >= LEFT_PADDLE_X &&
          ballY + BALL_SIZE >= paddleLeftY &&
          ballY <= paddleLeftY + PADDLE_HEIGHT) {

        ballVX = -ballVX;

        int hitPos = ballY - paddleLeftY;
        ballVY = map(hitPos, 0, PADDLE_HEIGHT, -2, 2);
        
        // ← ADD: Apply paddle motion to ball
        ballVY += leftPaddleVY * PADDLE_MOTION_INFLUENCE;  // 0.6 = tunable factor (try 0.4-0.8)
        ballVY = vlimit(ballVY);  // Clamp to sane speeds

        fireSound();
        Serial.print("Left hit! paddleVY="); Serial.print(leftPaddleVY);
        Serial.print(" new ballVY="); Serial.println(ballVY);  // Debug (remove later)
      }

      // ----- RIGHT PADDLE COLLISION (ENHANCED) -----
      if (ballVX > 0 &&
          ballX + BALL_SIZE >= RIGHT_PADDLE_X &&
          ballX + BALL_SIZE <= RIGHT_PADDLE_X + PADDLE_WIDTH &&
          ballY + BALL_SIZE >= paddleRightY &&
          ballY <= paddleRightY + PADDLE_HEIGHT) {

        ballVX = -ballVX;

        int hitPos = ballY - paddleRightY;
        ballVY = map(hitPos, 0, PADDLE_HEIGHT, -2, 2);
        
        // ← ADD: Apply paddle motion to ball
        ballVY += rightPaddleVY * PADDLE_MOTION_INFLUENCE;  // Same factor
        ballVY = vlimit(ballVY);  // Clamp
        fireSound();

        Serial.print("Right hit! paddleVY="); Serial.print(rightPaddleVY);
        Serial.print(" new ballVY="); Serial.println(ballVY);  // Debug (remove later)
      }

      // ----- MISS LEFT -----
      if (ballX < 0) {
        scoreRight++;
        missSound();
        ballState = WAITINGFORSERVE_RIGHT;  // wait for client to serve
        resetBall(true);
      }

      // ----- MISS RIGHT -----
      if (ballX > SCREEN_WIDTH) {
        scoreLeft++;
        missSound();
        ballState = WAITINGFORSERVE_LEFT;  // waiting for mater 
        resetBall(false);
      }
      break;

    case WAITINGFORSERVE_LEFT:
      // Stick the ball to the left paddle
      ballX = LEFT_PADDLE_X + PADDLE_WIDTH + 1;
      ballY = paddleLeftY + (PADDLE_HEIGHT / 2) - (BALL_SIZE / 2);
      
      if (digitalRead(BTN_PIN) == LOW) {
        // the master is serving, so we can start the ball moving. 
        ballState = BALLINPLAY;
        Serial.println("Master serve started!");
        // start ball moving right +include influence of paddle motion
        ballVX = BALL_SPEED_X;
        ballVY = 0;
        // apply paddle motion to ball
        ballVY += leftPaddleVY * PADDLE_MOTION_INFLUENCE;  // tunable factor
        ballVY = vlimit(ballVY);  // clamp
      }
      break;

    case WAITINGFORSERVE_RIGHT:
      // Stick the ball to the right paddle
      ballX = RIGHT_PADDLE_X - BALL_SIZE - 1;
      ballY = paddleRightY + (PADDLE_HEIGHT / 2) - (BALL_SIZE / 2);
      break;

    case CLIENT_SERVING:
      // the client has indicated it's ready to serve, so we can start the ball moving.
      ballState = BALLINPLAY;
      Serial.println("Client serve started!");
      // start ball moving left +include influence of paddle motion
      ballVX = -BALL_SPEED_X;
      ballVY = 0;
      // apply paddle motion to ball
      ballVY += rightPaddleVY * PADDLE_MOTION_INFLUENCE;  // same factor
      ballVY = vlimit(ballVY);  // clamp
      break;
  }
  sendState();

  // ← ADD: Update last positions for NEXT frame
  lastPaddleLeftY  = paddleLeftY;
  lastPaddleRightY = paddleRightY;
}

if (gameState == STATE_PLAYING) {
  drawBall();
}
drawScores();


  display.display();
  updateSound();


}
