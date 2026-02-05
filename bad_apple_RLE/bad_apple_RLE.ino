#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "baframes.h"

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define OLED_ADDR     0x3C

// to use this you will need the baframes.h
// 
// Step 1: export frames from the video ( this command exports and resizes)
// ffmpeg -i Bad_apple.mp4 -r 10 -vf "scale=85:64,format=rgb24" -frames:v 1000 BA_frame_%04d.bmp
// step 2: run the perl script on the frames
// perl ba_bmp2_code_RLE.pl  BA_frame_* > baframes.h
// step 3: copy baframes.h to the same dir as this script

// Frames are Run-length-encoded delta frames
// this gives us about 50% compression ratio, which is enough to encode the entire bad-apple video at 10fps for playback on the esp8266

// Number of frames — you'll need to set this manually
// #define NUM_FRAMES // this value is set in include file

// keyframe frequency
// #define KEYFRAME_EVERY // this value is set in include file


// Frame size (from your Perl conversion)
#define FRAME_W  85
#define FRAME_H  64

// Global buffers (outside loop & setup)
static uint8_t prevBuffer[680];     // previous full frame (680 bytes needed)
static uint8_t workBuffer[680];     // current frame being built


// Centering: left offset = (screen width - frame width) / 2
#define X_OFFSET ((SCREEN_WIDTH - FRAME_W) / 2)   // = 21 pixels

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
  Wire.begin(4, 5);  // SDA = GPIO4, SCL = GPIO5 (common for many ESP8266 boards)

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    for (;;);  // halt if display init fails
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("Bad Apple"));
  display.println(F("Initializing..."));
  display.display();


// In setup() — add this after display.begin()
memset(prevBuffer, 0x00, sizeof(prevBuffer));   // start all black
memset(workBuffer, 0x00, sizeof(workBuffer));

  delay(1500);
}
void loop() {
  static int currentFrame = 0;

  uint32_t start = pgm_read_dword(&frame_offsets[currentFrame]);
  uint32_t end   = pgm_read_dword(&frame_offsets[currentFrame + 1]);

  // 1. Decompress the frame data (keyframe or delta) into workBuffer
  uint16_t buf_idx = 0;
  uint32_t i = start;

  while (i < end && buf_idx < 680) {
    uint8_t b = pgm_read_byte(&compressed_data[i++]);

    if (b == 0x55) {   // escape → run-length
      if (i + 1 >= end) break;   // safety
      uint8_t count = pgm_read_byte(&compressed_data[i++]);
      if (i     >= end) break;
      uint8_t val   = pgm_read_byte(&compressed_data[i++]);

      // Prevent overflow
      if (buf_idx + count > 680) count = 680 - buf_idx;

      for (uint8_t j = 0; j < count; j++) {
        workBuffer[buf_idx++] = val;
      }
    } else {
      // literal byte
      workBuffer[buf_idx++] = b;
    }
  }

  // 2. Combine with previous frame
  if (currentFrame % KEYFRAME_EVERY == 0) {  // value is set in the include file
    // Keyframe → just copy
    memcpy(prevBuffer, workBuffer, 680);
  } else {
    // Delta → XOR
    for (int k = 0; k < 680; k++) {
      prevBuffer[k] ^= workBuffer[k];
    }
  }

  // 3. Draw the current frame state (prevBuffer now holds latest image)
  display.clearDisplay();
  display.drawBitmap(X_OFFSET, 0, prevBuffer, 85, 64, SSD1306_WHITE);
  display.display();

  // 4. Advance
  currentFrame++;
  if (currentFrame >= NUM_FRAMES) {
    currentFrame = 0;
    // Optional: reset to black on loop
    // memset(prevBuffer, 0x00, sizeof(prevBuffer));
  }

  delay(67);   // ~15 fps  (adjust: 40 = 25fps aggressive, 80–100 = safer ~10–12 fps)
}