#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

/* hardware assumptions
OLED display on I2C (D1=SCL, D2=SDA)
Buzzer output  D6 pin 12

inputs:
Analog joystick on A0 for vertical movement (motion is inverted)
Fire button on D7 pin 13(active low)
Shockwave fire on D3 pin 0 (active low)
Joystick fire on D5 pin 14 (active low) (used for game restart )

*/


/* ===== OLED CONFIG ===== */
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

#define D1 5
#define D2 4
#define BUZZER_PIN 12  // D6
#define D7 13   // fire button
#define D5 14   // Joystick fire button
#define SWITCH_PIN 0   // D3 

#define MINY 16
#define SHIPSIZE 5
#define TARGETSIZE 8

#define MAX_TARGETS 5
#define MAX_BULLETS 50
#define BULLET_SPEED 4
#define EXPLOSIONSPEED 4 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/* ===== SOUND STATE ===== */
bool soundActive = false;
unsigned long soundEndTime = 0;
int currentSoundFreq = 0;

int shockwaveX =0 ;
int shockwavePoints=0; 
int shockwaveactive=0;
int shockwavesound=0;
#define POINTS_FOR_SHOCKWAVE 10
#define SHOCKWAVE_SPEED 18
#define SHOCKWAVE_SIZE 25

int looptimer =0;
#define MINLOOPTIME 40  // loop time should be at-least 40 milisecods. 

//gamemodes
#define GAMEMODE_SELECTOR 0
#define GAMEMODE_BLASTER GAMEMODE_SELECTOR+1  
struct GameEntry {
  const char* name;
  int mode;
};

GameEntry gameList[] = {
  {"Blaster", GAMEMODE_BLASTER},
  {"(future) Mode 2", 2},
  {"(future) Mode 3", 3},
  {"(future) Mode 4", 4},
  {"(future) Mode 5", 5},
  {"(future) Mode 6", 6},
  {"(future) Mode 7", 7}
};

const int GAME_COUNT = sizeof(gameList) / sizeof(gameList[0]);

int gamemode=GAMEMODE_SELECTOR; // default to selector
int selectedGame = 0;
unsigned long lastNavTime = 0;
#define NAV_DELAY 180   // ms debounce for joystick movement



/* ===== SHIP BITMAP (5x5) ===== */
const uint8_t shipBitmap[] PROGMEM = {
  0b10000000,
  0b01100000,
  0b01111000,
  0b01100000,
  0b10000000
};


/* ===== Explosion bitmap (8x8 ===== */
const uint8_t EXPLODETOP[] PROGMEM = {
  0b00011000,
  0b00111100,
  0b01111110,
  0b01100110,
  0b11000111,
  0b11000101,
  0b10100000,
  0b10010000
};

const uint8_t EXPLODEBOT[] PROGMEM = {
  0b10010000,
  0b10100000,
  0b11000101,
  0b11000111,
  0b01100110,
  0b01111110,
  0b00111100,
  0b00011000
};




const uint8_t bulletFrames[][4] PROGMEM = {

  // Frame 0
  {
    0b00000000,  // 0000
    0b01100000,  // 0110
    0b01100000,  // 0110
    0b00000000   // 0000
  },

  // Frame 1
  {
    0b10000000,  // 1000
    0b01100000,  // 0110
    0b01100000,  // 0110
    0b01110000   // 0111
  },

  // Frame 2
  {
    0b01100000,  // 0110
    0b01100000,  // 0110
    0b01100000,  // 0110
    0b01100000   // 0110
  },

  // Frame 3
  {
    0b00010000,  // 0001
    0b01100000,  // 0110
    0b01100000,  // 0110
    0b10000000   // 1000
  }
};

#define BULLET_FRAME_COUNT 4

#include <Arduino.h>

// 8x8 rotating ring animation (vertical axis)
// Each frame is 8 rows (top to bottom)
const uint8_t ringFrames[][8] PROGMEM = {

  // Frame 0: full ring
  {
    0b00111100,
    0b01000010,
    0b10000001,
    0b10000001,
    0b10000001,
    0b10000001,
    0b01000010,
    0b00111100
  },

  // Frame 1: slightly turned
  {
    0b00011100,
    0b00100010,
    0b01000001,
    0b01000001,
    0b01000001,
    0b01000001,
    0b00100010,
    0b00011100
  },

  // Frame 2: more edge-on
  {
    0b00001100,
    0b00010010,
    0b00100001,
    0b00100001,
    0b00100001,
    0b00100001,
    0b00010010,
    0b00001100
  },

  // Frame 3: edge-on (vertical line)
  {
    0b00000100,
    0b00000100,
    0b00000100,
    0b00000100,
    0b00000100,
    0b00000100,
    0b00000100,
    0b00000100
  },

  // Frame 4: mirror of frame 2
  {
    0b00001100,
    0b01001000,
    0b10000100,
    0b10000100,
    0b10000100,
    0b10000100,
    0b01001000,
    0b00001100
  },

  // Frame 5: mirror of frame 1
  {
    0b00011100,
    0b01000100,
    0b10000010,
    0b10000010,
    0b10000010,
    0b10000010,
    0b01000100,
    0b00011100
  }
};

const uint8_t RING_FRAME_COUNT = sizeof(ringFrames) / 8;



/* ===== GAME STATE ===== */
bool gameOver = false;
int score = 0;
unsigned long gameStartTime = 0;
unsigned long gameEndSec = 0;

/* ===== BULLET ===== */
bool bulletActive = false;
int bulletX, bulletY;
uint8_t bulletFrame = 0;


/* ===== TARGET STRUCT ===== */
struct Target {
  bool active;
  int x;
  int y;
  int frame; // current frame in the animation
};

Target targets[MAX_TARGETS];
int current_max_bullets=1; 
int current_min_bullets=1;

/* ===== bullet STRUCT ===== */
struct bullet_struct {
  bool active;
  int x;
  int y;
  int frame; // current frame in the animation
};

bullet_struct bullets[MAX_BULLETS];

/* ===== Explosions STRUCT ===== */
struct Explosion {
  bool active;
  int x;
  int y;
  int lowery;
};

Explosion explosions[MAX_TARGETS];

/* ===== TIMING ===== */
unsigned long lastSpawn = 0;

/* ===== ROWS ===== */
int rows[] = {16, 24, 32, 40, 48,54};
const int ROWCOUNT = 6;

/*needed to compute diffaculy*/
unsigned long elapsedSeconds() {
  return (millis() - gameStartTime) / 1000;
}

void startSound(int freq, unsigned long durationMs) {
  if (soundActive) return;   // don't interrupt existing sound

  currentSoundFreq = freq;
  soundEndTime = millis() + durationMs;
  soundActive = true;

  tone(BUZZER_PIN, freq);
}

void fireSound() {
  startSound(2200, 40);   // short, sharp laser
}

void hitSound() {
  startSound(900, 90);    // slightly longer impact
}

void shockwaveSound(){
    startSound(600,300);
    shockwavesound=1;
}

void updateSound() {
  if (shockwavesound>0) {
    // Static / buzz effect
    int jitter = random(-100, 300);
    tone(BUZZER_PIN, 600 + jitter);
  };


  if (soundActive && millis() >= soundEndTime) {
    noTone(BUZZER_PIN);
    soundActive = false;
    shockwavesound=0;
  }
}




void soundCrash() {
  for (int f = 800; f > 200; f -= 20) {
    tone(BUZZER_PIN, f);
    delay(10);
  }
  noTone(BUZZER_PIN);
}


/* ===== HELPERS ===== */
int targetSpeed() {
  int t = elapsedSeconds();
  int speed = 1 + (t / 20);   // +1 speed every 20 seconds
  if (speed > 5) speed = 5;  // cap
  return speed;
}

unsigned long spawnDelay() {
  unsigned long t = elapsedSeconds();
  long d = 1400 - (t * 15);   // very slow reduction
  if (d < 350) d = 350;       // cap
  return d;
}


void spawnTarget() {
  for (int i = 0; i < MAX_TARGETS; i++) {
    if (!targets[i].active) {
      targets[i].active = true;
      targets[i].x = SCREEN_WIDTH - TARGETSIZE;
      targets[i].y = rows[random(ROWCOUNT)];
      targets[i].frame=0;
      return;
    }
  }
}

void spawnBullet(int x , int y) {
  for (int i = 0; i < current_max_bullets; i++) {
    if (!bullets[i].active) {
      bullets[i].active = true;
      bullets[i].x = x;
      bullets[i].y = y;
      bullets[i].frame=0;
      fireSound();
      return;
    }
  }
}



void spawnExplosion(int startx, int starty) {
  for (int i = 0; i < MAX_TARGETS; i++) {
    if (!explosions[i].active) {
      explosions[i].active = true;
      explosions[i].x = startx;
      explosions[i].y = starty;
      explosions[i].lowery = starty;
      return;
    }
  }
}



void resetGame() {
  score = 0;
  gameOver = false;
  bulletActive = false;
  current_max_bullets=1; 
  shockwaveX =0 ;
  shockwavePoints=0; 
  shockwaveactive=0;
  shockwavesound=0;

  for (int i = 0; i < MAX_TARGETS; i++) {
    targets[i].active = false;
    explosions[i].active=false;
  }
  for (int i = 0; i < MAX_BULLETS; i++) {
    bullets[i].active = false;
  }
  

  gameStartTime = millis();
  lastSpawn = millis();
  gameEndSec=0;
}


/* ===== SETUP ===== */
void setup() {
  randomSeed(analogRead(A0));
  pinMode(D7, INPUT_PULLUP);
  pinMode(D5, INPUT_PULLUP);
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  Wire.begin(D2, D1);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay();
  display.display();
  pinMode(BUZZER_PIN, OUTPUT);
  resetGame();
}

void game_selector() {
  display.clearDisplay();
  display.setTextSize(1);

  /* ===== JOYSTICK NAVIGATION ===== */
  int adc = 1024-analogRead(A0);  // reversed direction

  // Dead zone centered roughly around middle
  if (millis() - lastNavTime > NAV_DELAY) {
    if (adc < 350) {               // joystick UP
      selectedGame--;
      if (selectedGame < 0) selectedGame = GAME_COUNT - 1;
      lastNavTime = millis();
    }
    else if (adc > 700) {          // joystick DOWN
      selectedGame++;
      if (selectedGame >= GAME_COUNT) selectedGame = 0;
      lastNavTime = millis();
    }
  }

  /* ===== DRAW MENU ===== */
  int centerY = 32;
  int lineHeight = 12;

  for (int i = -1; i <= 1; i++) {
    int idx = selectedGame + i;
    if (idx < 0 || idx >= GAME_COUNT) continue;

    int y = centerY + (i * lineHeight);

    if (i == 0) {
      // Selected item (white background, black text)
      display.fillRect(0, y - 2, SCREEN_WIDTH, 10, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }

    display.setCursor(10, y);
    display.print(gameList[idx].name);
  }

  display.setTextColor(SSD1306_WHITE);

  /* ===== SELECT GAME ===== */
  if (digitalRead(SWITCH_PIN) == LOW) {
    resetGame();
    gamemode = gameList[selectedGame].mode;
  }
}




void blaster_game()
{
    /* ===== GAME OVER SCREEN ===== */
  if (gameOver) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(32, 24);
    display.println("GAME OVER");

    display.setCursor(32, 40);
    display.print("Score: ");
    display.print(score);
    display.setCursor(32, 50);
    display.print("T:");
    if (gameEndSec==0){gameEndSec=elapsedSeconds();
                       soundCrash();
                       };
    display.print(gameEndSec);
    display.display();

    if (digitalRead(SWITCH_PIN) == LOW) {
      delay(250);
      resetGame();
    }
    return;
  }

  if (digitalRead(SWITCH_PIN) == LOW){
      //display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(32, 24);
    display.println("JOYSTICK CLICK");
     display.display();
     //delay(5000);
      }


  if ((digitalRead(D5) == LOW)&&(shockwavePoints>=POINTS_FOR_SHOCKWAVE)){
      shockwaveX=-SHOCKWAVE_SIZE;
      shockwaveactive=1; 
      shockwavePoints=0;
      shockwaveSound();
      }


  /* ===== SHIP POSITION ===== */
  int adc = analogRead(A0);
  int shipY = MINY + (64 - (adc >> 4));
  if (shipY < MINY) shipY = MINY;
  if (shipY > 64 - SHIPSIZE) shipY = 64 - SHIPSIZE;

  /* ===== FIRE BULLET ===== */
  if ( digitalRead(D7) == LOW) { // need to add some one fire per click logic here 
    //bulletActive = true;
    //bulletX = SHIPSIZE;
    //bulletY = shipY + 2;
    spawnBullet(SHIPSIZE,shipY+2);
    //fireSound(); // moved into spawn, only play if fire successful
  }

  /* ===== MOVE BULLETS ===== */
 for (int i = 0; i < MAX_BULLETS; i++) {
    if (bullets[i].active) {
      bullets[i].x += BULLET_SPEED;
      bullets[i].frame++;
          if (bullets[i].frame>=BULLET_FRAME_COUNT){bullets[i].frame=0;};

      if (bullets[i].x > SCREEN_WIDTH) {
        bullets[i].active = false;
        // target missed
        if (current_min_bullets<current_max_bullets){current_max_bullets--;};
      }
    }
  }

// Move shockwave
 if (shockwaveactive>0){
   shockwaveX=shockwaveX+SHOCKWAVE_SPEED; 
  // kill targets in range
  for (int i = 0; i < MAX_TARGETS; i++) {
    if (!targets[i].active) continue;
     
    if ((targets[i].x >= shockwaveX) && (targets[i].x<= shockwaveX+SHOCKWAVE_SIZE))
        {
          targets[i].active = false;     
          score++;
          spawnExplosion(targets[i].x,targets[i].y);
        };
  }; //for alltargets
   if (shockwaveX>SCREEN_WIDTH){
       shockwaveX=0;
       shockwaveactive=0;
       };
 }


  /* ===== SPAWN TARGETS ===== */
  if (millis() - lastSpawn > spawnDelay()) {
    spawnTarget();
    lastSpawn = millis();
  }

  /* ===== MOVE TARGETS ===== */
  for (int i = 0; i < MAX_TARGETS; i++) {
    if (targets[i].active) {
      targets[i].x -= targetSpeed();
      targets[i].frame++;
          if (targets[i].frame>RING_FRAME_COUNT){targets[i].frame=0;};

      if (targets[i].x < -TARGETSIZE) {
        targets[i].active = false;
         // target missed
        if (current_min_bullets<current_max_bullets){current_max_bullets--;};
      }
    }
  }


  /* ===== MOVE Explosions ===== */
  for (int i = 0; i < MAX_TARGETS; i++) {
    if (explosions[i].active) {
      explosions[i].y -= EXPLOSIONSPEED;
      explosions[i].lowery += EXPLOSIONSPEED;
      if ((explosions[i].y < -TARGETSIZE) &&(explosions[i].lowery> SCREEN_HEIGHT-TARGETSIZE))  {
          explosions[i].active = false; // disable when *BOTH parts are beyond screen 
      }
    }
  }


  /* ===== COLLISIONS ===== */
  for (int i = 0; i < MAX_TARGETS; i++) {
    if (!targets[i].active) continue;
    
    for (int b=0;b<MAX_BULLETS;b++){  // for all bullets
    // Bullet hit
    if (bullets[b].active &&
        bullets[b].x >= targets[i].x &&
        bullets[b].x <= targets[i].x + TARGETSIZE &&
        bullets[b].y >= targets[i].y &&
        bullets[b].y <= targets[i].y + TARGETSIZE) {

      bullets[b].active=false; // one hit per bullet
      targets[i].active = false;
      bulletActive = false;
      score++;
      if (current_max_bullets<MAX_BULLETS){current_max_bullets++;};
      shockwavePoints++;
      hitSound();
      spawnExplosion(targets[i].x,targets[i].y);
        } // for all bullets
    }  //for all targets

    // Ship hit
    if (targets[i].x <= SHIPSIZE &&
        targets[i].y + TARGETSIZE >= shipY &&
        targets[i].y <= shipY + SHIPSIZE) {

      gameOver = true;
    }
  }

  /* ===== DRAW ===== */
  display.clearDisplay();

  // Score bar
display.setCursor(0, 0);
display.print("Score:");
display.print(score);
display.print(" T:");
display.print(elapsedSeconds());

display.print(" B:");
display.print(current_max_bullets);

if (shockwavePoints>0)
   {
    display.fillRect(max(SCREEN_WIDTH-POINTS_FOR_SHOCKWAVE,0),0, min( POINTS_FOR_SHOCKWAVE, shockwavePoints), 10, SSD1306_WHITE);
    if (shockwavePoints>=POINTS_FOR_SHOCKWAVE)
       {
        display.setTextColor(SSD1306_BLACK);
        display.setCursor(SCREEN_WIDTH-POINTS_FOR_SHOCKWAVE+2, 2);
        display.print("S");
        display.setTextColor(SSD1306_WHITE); 
       } 
   }
// check for extra bullets
if ((elapsedSeconds()/60)>current_min_bullets){current_min_bullets=elapsedSeconds()/60;};


  // Ship
  display.drawBitmap(0, shipY, shipBitmap, 5, 5, SSD1306_WHITE);

  // Bullet
  for (int i = 0; i < MAX_BULLETS; i++) 
  {
 if (bullets[i].active) {
  display.drawBitmap(
    bullets[i].x - 1,
    bullets[i].y - 1,
    bulletFrames[bullets[i].frame],
    4,
    4,
    SSD1306_WHITE
  );
}
  };// for all bullets

  // Targets
  for (int i = 0; i < MAX_TARGETS; i++) {
    if (targets[i].active) {
      //display.fillRect(targets[i].x, targets[i].y,
      //                 TARGETSIZE, TARGETSIZE, SSD1306_WHITE);
      display.drawBitmap(targets[i].x, targets[i].y, ringFrames[targets[i].frame], TARGETSIZE, TARGETSIZE, SSD1306_WHITE);

    }
  }

//explosions
  for (int i = 0; i < MAX_TARGETS; i++) {
    if (explosions[i].active) {
     display.drawBitmap(explosions[i].x, explosions[i].y,EXPLODETOP,TARGETSIZE, TARGETSIZE, SSD1306_WHITE);
     display.drawBitmap(explosions[i].x, explosions[i].lowery,EXPLODEBOT,TARGETSIZE, TARGETSIZE, SSD1306_WHITE);

    }
  }

//  shockwave
   if (shockwaveactive){
    display.fillRect(max(shockwaveX,0),0, SHOCKWAVE_SIZE, SCREEN_HEIGHT, SSD1306_WHITE);   
   };

}
/* ===== LOOP ===== */
void loop() {
  looptimer=millis(); // record start of loop time
  switch (gamemode) {
    case GAMEMODE_SELECTOR:
      game_selector();
      break;  
    case GAMEMODE_BLASTER:
      blaster_game();
      break;
 
  }

  
  display.display();
  updateSound();

  while (millis()<looptimer+MINLOOPTIME)
    {
  delay(2);
  updateSound();
    };
}
