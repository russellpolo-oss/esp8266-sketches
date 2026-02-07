// ESP8266 - ADS1115 (0x48) for X + ESP8266 A0 for Y + SSD1306 OLED (0x3C)
// X from ADS1115 A1 (inverted), Y from ESP8266 A0
// 15% edge deadzones with snap to edges outside 15–85%

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_ADS1X15.h>

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define OLED_ADDR     0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_ADS1115 ads;

int prevX = -1;
int prevY = -1;

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\nOLED Cursor - X: ADS1115 A1  |  Y: ESP8266 A0");

  Wire.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("SSD1306 failed"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("X = ADS A1   Y = ESP A0"));
  display.println(F("Inverted X + Edge Snap"));
  display.display();
  delay(2000);

  if (!ads.begin(0x48)) {
    Serial.println("ADS1115 init failed!");
    for(;;);
  }
  ads.setGain(GAIN_ONE);  // ±4.096V range for X

  // ESP8266 A0 is ready by default - no init needed
  // analogRead(A0) uses the single ADC pin (0-1023)

  display.clearDisplay();
}

void loop() {
  // X from ADS1115 channel 1 (A0)
  int16_t adcX = ads.readADC_SingleEnded(0);

  // Y from ESP8266 built-in A0 pin (10-bit: 0-1023)
  int adcY = analogRead(A0);

  // Optional debug print
  // Serial.print("X (ADS A1): "); Serial.print(adcX);
  // Serial.print("   Y (ESP A0): "); Serial.println(adcY);

  // ─────────────────────────────────────────────
  // Deadzone / edge bracketing - separate ranges
  // ─────────────────────────────────────────────

  // For X (ADS1115 16-bit)
  const int16_t xMin   = 0;
  const int16_t xMax   = 20000;
  const float deadband = 0.15;
  const int16_t xLow   = xMin + (int)((xMax - xMin) * deadband);    // ~4915
  const int16_t xHigh  = xMax - (int)((xMax - xMin) * deadband);    // ~27852

  int16_t clampedX = adcX;
  if (clampedX < xLow)  clampedX = xMin;
  if (clampedX > xHigh) clampedX = xMax;

  // For Y (ESP8266 10-bit)
  const int yMin   = 0;
  const int yMax   = 1023;
  const int yLow   = yMin + (int)((yMax - yMin) * deadband);   // ~153
  const int yHigh  = yMax - (int)((yMax - yMin) * deadband);   // ~870

  int clampedY = adcY;
  if (clampedY < yLow)  clampedY = yLow;
  if (clampedY > yHigh) clampedY = yHigh;

  // ─────────────────────────────────────────────
  // Map to screen coordinates
  // ─────────────────────────────────────────────
  // X inverted: high raw → left side
   int x = map(clampedX, xLow, xHigh, SCREEN_WIDTH - 4, 0);

  // Y normal: low raw → top of screen
  int y = map(clampedY, yHigh,yLow, 0, SCREEN_HEIGHT - 4);

  x = constrain(x, 0, SCREEN_WIDTH - 4);
  y = constrain(y, 0, SCREEN_HEIGHT - 4);

  // Redraw only if position changed
  if (x != prevX || y != prevY) {
    display.clearDisplay();
    display.fillRect(x, y, 4, 4, SSD1306_WHITE);
display.setTextSize(1);
display.setTextColor(SSD1306_WHITE);
display.setCursor(0, 0);           // top-left corner

display.print(F("X = "));
display.print(x);                  // directly print the integer variable
display.print(F("  Y = "));
display.println(y);                  // same for y

display.print(F("Raw X = "));
display.print(adcX);                  // directly print the integer variable
display.print(F("Raw Y = "));
display.println(adcY);  

    display.display();

    prevX = x;
    prevY = y;
  }

  delay(40);  // ~25 updates/sec
}