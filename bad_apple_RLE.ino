#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "baframes.h"

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define OLED_ADDR     0x3C
// export command : ffmpeg -i Bad_apple.mp4  -frames:v 2000  BA_frame_%04d.bmp
// ffmpeg -i Bad_apple.mp4 -r 10 -frames:v 1000 -vf format=rgb24 BA_frame_%04d.bmp
// rescale command : for f in BA_frame_*.bmp; do   convert "$f" -resize x64 "$f"; done

// updated export and resiz at the same time : 
// ffmpeg -i Bad_apple.mp4 -r 10 -vf "scale=85:64,format=rgb24" -frames:v 1000 BA_frame_%04d.bmp


// Number of frames — you'll need to set this manually
// (or define it as a constant after generation, e.g. constexpr int NUM_FRAMES = 6572;)


// Frame size (from your Perl conversion)
#define FRAME_W  85
#define FRAME_H  64

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
  delay(1500);
}

void loop() {
  static int currentFrame = 0;

  display.clearDisplay();
uint8_t currentBuffer[704];
uint8_t prevBuffer[704];  // extra RAM for delta

// In setup: initialize prev to black (0x00)
memset(prevBuffer, 0, 704);

// In loop()
uint32_t start = pgm_read_dword(&frame_offsets[currentFrame]);
uint32_t end = pgm_read_dword(&frame_offsets[currentFrame + 1]);

// Decompress to temp delta
uint8_t deltaBuffer[704];
uint16_t buf_idx = 0;
for (uint32_t i = start; i < end; i++) {
  uint8_t b = pgm_read_byte(&compressed_data[i]);
  if (b == 0x55) {
    i++;
    uint8_t count = pgm_read_byte(&compressed_data[i]);
    i++;
    uint8_t val = pgm_read_byte(&compressed_data[i]);
    for (uint8_t j = 0; j < count; j++) {
      deltaBuffer[buf_idx++] = val;
    }
  } else {
    deltaBuffer[buf_idx++] = b;
  }
}

// Apply delta if not keyframe
memcpy(currentBuffer, prevBuffer, 704);
if (currentFrame % 100 != 0) {
  for (int i = 0; i < 704; i++) {
    currentBuffer[i] ^= deltaBuffer[i];
  }
} else {
  memcpy(currentBuffer, deltaBuffer, 704);
}

// Update prev
memcpy(prevBuffer, currentBuffer, 704);

// Draw
display.drawBitmap(X_OFFSET, 0, currentBuffer, 85, 64, SSD1306_WHITE);


  display.display();

  // Next frame (loop around)
  currentFrame++;
  if (currentFrame >= NUM_FRAMES) {
    currentFrame = 0;
  }

  // Frame rate control — adjust delay to taste
  // ~8–12 fps is typical for smooth playback without too much flicker on I²C
  delay(40);   // ≈12.5 fps    ← tune this value (50 = faster, 120 = slower)
}