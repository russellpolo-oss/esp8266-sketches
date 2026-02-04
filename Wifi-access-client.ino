#include <ESP8266WiFi.h>
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

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/* ===== WIFI CONFIG ===== */
const char* serverSSID = "ESP8266_Server";   // AP ESP
const uint16_t serverPort = 80;

/* ===== GLOBALS ===== */
WiFiClient client;
bool fetched = false;

/* ===== OLED ===== */
void showStatus(const String& l1, const String& l2) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(l1);
  display.println();
  display.println(l2);
  display.display();
}

void showIPs(IPAddress clientIP, IPAddress serverIP) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("WiFi CONNECTED");
  display.print("Client: ");
  display.println(clientIP);
  display.print("Server: ");
  display.println(serverIP);
  display.display();
}

void setup() {
  Serial.begin(115200);
  Serial.println();

  /* ===== OLED INIT ===== */
  Wire.begin(D2, D1);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED not found");
    while (1);
  }

  showStatus("CLIENT MODE", "Scanning...");

  /* ===== WIFI CLIENT ===== */
  WiFi.mode(WIFI_STA);
  WiFi.begin(serverSSID);   // open AP

  Serial.print("Connecting to ");
  Serial.println(serverSSID);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  IPAddress clientIP = WiFi.localIP();
  IPAddress serverIP = WiFi.gatewayIP();   // AP = server

  Serial.println("\nConnected");
  Serial.print("Client IP: ");
  Serial.println(clientIP);
  Serial.print("Server IP: ");
  Serial.println(serverIP);

  showIPs(clientIP, serverIP);
}

void loop() {
  if (WiFi.status() == WL_CONNECTED && !fetched) {
    IPAddress serverIP = WiFi.gatewayIP();

    Serial.println("Requesting web page...");

    if (client.connect(serverIP, serverPort)) {
      client.println("GET / HTTP/1.1");
      client.print("Host: ");
      client.println(serverIP);
      client.println("Connection: close");
      client.println();

      bool body = false;
      Serial.println("----- PAGE BODY -----");

      while (client.connected() || client.available()) {
        if (client.available()) {
          String line = client.readStringUntil('\n');

          if (line == "\r") {   // end of headers
            body = true;
            continue;
          }

          if (body) {
            Serial.println(line);
          }
        }
      }

      Serial.println("----- END -----");
      client.stop();
      fetched = true;
    } else {
      Serial.println("Server connection failed");
    }
  }
}
