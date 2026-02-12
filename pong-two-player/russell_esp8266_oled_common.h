/* ===== OLED ===== */
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
#define D7 13   // fire button

/* ===== INPUT ===== */
#define BTN_PIN D7 // fire button , better name 
#define JOY_PIN A0  // analog joystick vertical axis

