#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

/* ===== OLED CONFIG ===== */
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

#define D1 5   // SCL
#define D2 4   // SDA

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/* ===== WIFI CONFIG ===== */
struct WiFiCred {
  const char* ssid;
  const char* password;
};

WiFiCred wifiList[] = {
  {"ssid_one", "password_one"},
  {"ssid_two", "password_two"},
  {"ssid_three", "password_three"}
};

const int WIFI_COUNT = sizeof(wifiList) / sizeof(wifiList[0]);
int currentWiFi = 0;
bool wifiConnected = false;

/* ===== WEB SERVER ===== */
ESP8266WebServer server(80);

/* ===== DISPLAY STATE ===== */
bool showingMessage = false;
unsigned long messageTime = 0;
const unsigned long MESSAGE_DISPLAY_TIME = 5000;

/* ===== WEB HANDLERS ===== */
void handleRoot() {
  String page =
    "<html><body>"
    "<h2>ESP8266 Control</h2>"
    "<form action='/submit' method='GET'>"
    "Text: <input type='text' name='text'><br><br>"
    "Number: <input type='number' name='number'><br><br>"
    "<input type='submit' value='Send'>"
    "</form>"
    "</body></html>";

  server.send(200, "text/html", page);
}

void handleSubmit() {
  String textValue = server.hasArg("text") ? server.arg("text") : "";
  String numberValue = server.hasArg("number") ? server.arg("number") : "";

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);

  if (textValue.length()) {
    display.println("Text:");
    display.println(textValue);
  }

  if (numberValue.length()) {
    display.println();
    display.println("Number:");
    display.println(numberValue);
  }

  display.display();

  showingMessage = true;
  messageTime = millis();

  server.sendHeader("Location", "/");
  server.send(303);
}

/* ===== OLED SCREENS ===== */
void showTryingScreen(const char* ssid) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Trying WiFi:");
  display.println(ssid);
  display.display();
}

void showConnectedScreen(const char* ssid) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Connected to:");
  display.println(ssid);
  display.println();
  display.print("IP: ");
  display.println(WiFi.localIP());
  display.display();
}

/* ===== WIFI LOGIC ===== */
void connectToWiFi() {
  wifiConnected = false;

  for (currentWiFi = 0; currentWiFi < WIFI_COUNT; currentWiFi++) {
    WiFi.disconnect();
    delay(200);

    showTryingScreen(wifiList[currentWiFi].ssid);

    WiFi.begin(
      wifiList[currentWiFi].ssid,
      wifiList[currentWiFi].password
    );

    unsigned long startAttempt = millis();

    while (millis() - startAttempt < 8000) {  // 8s per SSID
      if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        showConnectedScreen(wifiList[currentWiFi].ssid);
        Serial.println("Connected to:");
        Serial.println(wifiList[currentWiFi].ssid);
        return;
      }
      delay(250);
    }
  }

  // If all fail, wait and retry from beginning
  delay(2000);
}

/* ===== SETUP ===== */
void setup() {
  Serial.begin(9600);
  delay(100);

  /* OLED INIT */
  Wire.begin(D2, D1);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED failed");
    while (true);
  }

  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Booting...");
  display.display();

  /* WIFI INIT */
  WiFi.mode(WIFI_STA);
  connectToWiFi();

  /* WEB SERVER */
  server.on("/", handleRoot);
  server.on("/submit", handleSubmit);
  server.begin();
}

/* ===== LOOP ===== */
void loop() {
  server.handleClient();

  // Restore default screen after message
  if (showingMessage && millis() - messageTime >= MESSAGE_DISPLAY_TIME) {
    showingMessage = false;
    showConnectedScreen(wifiList[currentWiFi].ssid);
  }

  // Detect WiFi drop
  if (wifiConnected && WiFi.status() != WL_CONNECTED) {
    wifiConnected = false;
    connectToWiFi();
  }
}
