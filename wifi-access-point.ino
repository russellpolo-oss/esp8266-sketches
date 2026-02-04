#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

/* ===== OLED CONFIG ===== */
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define OLED_ADDR     0x3C

#define D1 5
#define D2 4

unsigned long lastOLED = 0;


Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/* ===== WIFI CONFIG ===== */
const char* apSSID = "ESP8266_Server";


ESP8266WebServer server(80);

/* ===== WEB PAGE ===== */
void handleRoot() {
  IPAddress clientIP = server.client().remoteIP();

  String page =
    "<!DOCTYPE html><html><head>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<title>ESP8266 Server</title></head><body>"
    "<h1>Connected to ESP8266</h1>"
    "<p><b>Your IP address:</b></p>"
    "<h2>" + clientIP.toString() + "</h2>"
    "<hr>"
    "<p>ESP IP: " + WiFi.softAPIP().toString() + "</p>"
    "</body></html>";

  server.send(200, "text/html", page);
}

/* ===== OLED DISPLAY ===== */
void showOLED(IPAddress ip) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println("ESP8266 AP MODE");

  display.println();
  display.print("SSID:");
  display.println(apSSID);

  display.print("PASS: <open>");
  

  display.println();
  display.print("IP:");
  display.println(ip);

  display.println();
  display.print("Clients: ");
  display.println(WiFi.softAPgetStationNum());

  display.display();
}


void setup() {
  Serial.begin(115200);
  Serial.println();

  /* ===== OLED INIT ===== */
  Wire.begin(D2, D1);  // SDA, SCL
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED not found");
    while (1);
  }

  /* ===== WIFI AP ===== */
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSSID, nullptr);  // explictly set no password
  IPAddress ip = WiFi.softAPIP();

  Serial.println("Access Point Started");
  Serial.print("SSID: ");
  Serial.println(apSSID);
  Serial.print("IP address: ");
  Serial.println(ip);

  /* ===== SHOW INFO ON OLED ===== */
  showOLED(ip);

  /* ===== WEB SERVER ===== */
  server.on("/", handleRoot);
  server.begin();

  Serial.println("Web server started");
}


  void loop() {
  server.handleClient();

  if (millis() - lastOLED > 1000) {   // update every 1s
    lastOLED = millis();
    showOLED(WiFi.softAPIP());
  }
}


