#include <ESP8266WiFi.h>
#include <espnow.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

/* ===== OLED ===== */
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
#define D7 13   // fire button

/* ===== INPUT ===== */
#define BTN_PIN D7
#define JOY_PIN A0
#define BUZZER_PIN 12  // D6
#define SWITCH_PIN 14   // D5 Left button
#define JOY_SWITCH_PIN 0   // D3  Joystick Fire button


/* sound triggers*/
#define TRIGGER_SOUND_NONE 0x00
#define TRIGGER_SOUND_BOUNCE 0x01
#define TRIGGER_SOUND_MISS 0x02
#define TRIGGER_SOUND_LOSE 0x03
#define TRIGGER_SOUND_WIN 0x04

/* ===== SOUND STATE ===== */
bool soundActive = false;
unsigned long soundEndTime = 0;
int currentSoundFreq = 0;
uint8_t triggered_sound=0;
int shockwavesound=0; // not used but refrenced in sound helper
int current_sound_index=0; // pointer to music notes array. 
int currently_in_sound_gap=0; // used for sound effects that have multiple notes, to track the gap between them.

#define G_freq   392
#define Eb_freq 311
#define c5_freq 523
#define e5_freq 659
#define g5_freq 784
#define c6_freq 1047



#define fire_freq 2200
#define miss_freq 42
#define note_gap  100 // gap between notes in ms, used for sound effects that have multiple notes.
#define short_gap 20 // for quick notes like trimph 
struct music_notes {
  int tone;   // freqency in Hz
  int duration; // duration in ms
  int gap_after; // gap after the note in ms, used for multi-note sound effects.

};



const music_notes music[] PROGMEM = { 
  {fire_freq, 40,0}, // short, sharp laser #FIRESOUND_INDEX=0
  {miss_freq, 800,0}, // low buzz #MISSOUND_INDEX=1
  {G_freq, 300,note_gap}, // Lose Sound , betoven. #LOSESOUND_INDEX=2
  {G_freq, 300,note_gap}, 
  {G_freq, 300,note_gap}, 
  {Eb_freq, 900,0},
  {c5_freq, 80,short_gap}, // triumph notes #WINSOUND_INDEX=6
  {e5_freq, 80,short_gap},
  {g5_freq, 80,short_gap},
  {c6_freq, 80,short_gap},
  {g5_freq, 400,0}, // hold last note
  {0, 0,0} // end marker
};



#define FIRESOUND_INDEX 0
#define MISSOUND_INDEX 1
#define LOSESOUND_INDEX 2
#define WINSOUND_INDEX 6


/* sound functions */

void startSound(int index) {
  //if (soundActive) return;   // don't interrupt existing sound

  current_sound_index = index;
  int freq = music[index].tone;
  int durationMs = music[index].duration;
  currentSoundFreq = freq;
  soundEndTime = millis() + durationMs;
  soundActive = true;
  currently_in_sound_gap=0; // we are playing a note, not a gap. 
  Serial.print("Starting sound index "); Serial.print(index);
  Serial.print(" freq "); Serial.print(freq);
  tone(BUZZER_PIN, freq);
}

void fireSound() {
  startSound(FIRESOUND_INDEX);   // short, sharp laser
  triggered_sound=TRIGGER_SOUND_BOUNCE;
}

void missSound() {
  startSound(MISSOUND_INDEX);   // low buzz
  triggered_sound=TRIGGER_SOUND_MISS;
}

void looseSound() {
  startSound(LOSESOUND_INDEX);   // betoven notes
  triggered_sound=TRIGGER_SOUND_LOSE;
}

void winSound() {
  startSound(WINSOUND_INDEX);   // trimph notes
  triggered_sound=TRIGGER_SOUND_WIN;
} 

void updateSound() {
  if (shockwavesound>0) {
    // Static / buzz effect
    int jitter = random(-100, 300);
    tone(BUZZER_PIN, 600 + jitter);
  };


  if (soundActive && millis() >= soundEndTime) {
    noTone(BUZZER_PIN);
    // check for multi-note sound effects
    if (music[current_sound_index].gap_after == 0) {
    soundActive = false;
    shockwavesound=0;
    Serial.println("Sound ended");
    } else {
      // did we play the gap between notes already? if not, play it now.
      if ((currently_in_sound_gap==0) && (music[current_sound_index].gap_after > 0)) {
        Serial.println("starting gap");
        // we just played a note, now play the gap before the next note.
        noTone(BUZZER_PIN); // silence for gaps
        currently_in_sound_gap=1; // we are in a gap between notes. 
        soundEndTime = millis() + music[current_sound_index].gap_after; // set end time for the gap
      } else {
        Serial.println("gap ended, play next");
        startSound(current_sound_index+1); // play the next note in the effect
      }
    }
}
}

void process_sound_trigger(uint8_t trigger){
 if (TRIGGER_SOUND_NONE == trigger){return;};
 if (TRIGGER_SOUND_BOUNCE == trigger){
   fireSound();
  return;
 }

}


void setup() {
  Serial.begin(115200);           // Very helpful for debugging
  delay(100);                     // Give serial time to settle

  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  pinMode(JOY_SWITCH_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  // Initialize I2C and OLED
  Wire.begin();                   // Uses default pins: D1 (SCL = GPIO5), D2 (SDA = GPIO4) on most ESP8266 boards

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {  // 0x3C is most common address
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);  // Don't continue — halt here so you know it's the display
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("Starting..."));
  display.display();
  Serial.println("OLED init OK");
}
void loop() {

  static bool lastBtn = HIGH;
  bool btn = digitalRead(BTN_PIN);
   if (lastBtn == HIGH && btn == LOW ) {
    display.clearDisplay();
    display.setCursor(50, 22);
    display.print("firesound");
    fireSound();  
  }
  lastBtn = btn;

  static bool lastSwitch = HIGH;
  btn = digitalRead(SWITCH_PIN);
   if (lastSwitch == HIGH && btn == LOW ) {
    display.clearDisplay();
    display.setCursor(50, 22);
    display.print("WinSound");
    winSound();  
  }
  lastSwitch = btn;

  static bool lastJoySwitch = HIGH;
  btn = digitalRead(JOY_SWITCH_PIN);
   if (lastJoySwitch == HIGH && btn == LOW ) {
    display.clearDisplay();
    display.setCursor(50, 22);
    display.print("LooseSound");
    looseSound();  
  }
  lastJoySwitch = btn;



 display.display();
  updateSound();
}
