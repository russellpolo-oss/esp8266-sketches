// ESP8266 Serial Command Interpreter - Extended Version
// Commands (case insensitive):
//   W<pin> <0/1>       → digitalWrite(pin, value)
//   R<pin>             → read digital pin and print value
//   PM<pin> <0/1/2>    → pinMode(pin, mode)  0=INPUT, 1=OUTPUT, 2=INPUT_PULLUP
//   K <0/1>            → write value to ALL known pins
//   U <0/1>            → write value to ALL unknown pins
//
// Example:
//   w14 1
//   r5
//   K 1
//   u 0
//   pm5 2

#include <Arduino.h>

#define MAXPIN 20

// Known / grouped pins
const int known[]   = {2,4,5,12,13,14,15,16};
const int unknown[] = {10, 17, 18, 19, 20};
const int do_not_touch[] = {1,3,6,7,8,9,11};

const int numKnown   = sizeof(known)   / sizeof(known[0]);
const int numUnknown = sizeof(unknown) / sizeof(unknown[0]);

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\nESP8266 Serial Command Parser ready (case insensitive)");
  Serial.println("Commands: W<pin> <0/1>   R<pin>   PM<pin> <0/1/2>   K <0/1>   U <0/1>");
  Serial.println("Known group  : " + String(known[0])   + "," + String(known[1])   + "," +
                 String(known[2])   + "," + String(known[3]));
  Serial.println("Unknown group: " + String(unknown[0]) + "," + String(unknown[1]) + "," +
                 String(unknown[2]) + "," + String(unknown[3]) + "," + String(unknown[4]));
  Serial.println("----------------------------------------");
}

void loop() {
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();

    if (line.length() == 0) return;

    // Make command case-insensitive
    String cmd = line;
    cmd.toUpperCase();

    Serial.print("> ");
    Serial.println(line);           // echo original (with original case)

    // Remove any \r
    cmd.replace("\r", "");

    if (cmd.startsWith("W")) {
      // ─── Write single pin ─────────────────────────────────────
      int spacePos = cmd.indexOf(' ');
      if (spacePos == -1) {
        Serial.println("ERR: W needs pin and value  e.g. W12 1");
        return;
      }

      String pinStr = cmd.substring(1, spacePos);
      String valStr = cmd.substring(spacePos + 1);
      valStr.trim();

      int pin   = pinStr.toInt();
      int value = valStr.toInt();

      if (pin < 0 || pin > MAXPIN) {
        Serial.print("ERR: Pin must be 0–"); Serial.print(MAXPIN); Serial.println();
        return;
      }
      if (value != 0 && value != 1) {
        Serial.println("ERR: Value must be 0 or 1");
        return;
      }

      pinMode(pin, OUTPUT);
      digitalWrite(pin, value);

      Serial.print("W"); Serial.print(pin); Serial.print(" → "); Serial.println(value);
    }

    else if (cmd.startsWith("R")) {
      // ─── Read single pin ──────────────────────────────────────
      String pinStr = cmd.substring(1);
      pinStr.trim();
      int pin = pinStr.toInt();

      if (pin < 0 || pin > MAXPIN) {
        Serial.print("ERR: Pin must be 0–"); Serial.print(MAXPIN); Serial.println();
        return;
      }

      int val = digitalRead(pin);

      Serial.print("R"); Serial.print(pin); Serial.print(" = "); Serial.println(val);
    }

    else if (cmd.startsWith("PM")) {
      // ─── pinMode ──────────────────────────────────────────────
      int spacePos = cmd.indexOf(' ');
      if (spacePos == -1) {
        Serial.println("ERR: PM needs pin and mode  e.g. PM5 2");
        return;
      }

      String pinStr  = cmd.substring(2, spacePos);
      String modeStr = cmd.substring(spacePos + 1);
      pinStr.trim(); modeStr.trim();

      int pin  = pinStr.toInt();
      int mode = modeStr.toInt();

      if (pin < 0 || pin > MAXPIN) {
        Serial.print("ERR: Pin must be 0–"); Serial.print(MAXPIN); Serial.println();
        return;
      }

      uint8_t arduinoMode;
      if      (mode == 0) arduinoMode = INPUT;
      else if (mode == 1) arduinoMode = OUTPUT;
      else if (mode == 2) arduinoMode = INPUT_PULLUP;
      else {
        Serial.println("ERR: Mode: 0=INPUT, 1=OUTPUT, 2=INPUT_PULLUP");
        return;
      }

      pinMode(pin, arduinoMode);
      Serial.print("pinMode("); Serial.print(pin); Serial.print(") = "); Serial.println(mode);
    }

    else if (cmd.startsWith("K")) {
      // ─── Write to all Known pins ──────────────────────────────
      String valStr = cmd.substring(1);
      valStr.trim();
      int value = valStr.toInt();

      if (value != 0 && value != 1) {
        Serial.println("ERR: K needs 0 or 1");
        return;
      }

      for (int i = 0; i < numKnown; i++) {
        int pin = known[i];
        pinMode(pin, OUTPUT);
        digitalWrite(pin, value);
      }

      Serial.print("K → all known pins set to "); Serial.println(value);
    }

    else if (cmd.startsWith("U")) {
      // ─── Write to all Unknown pins ────────────────────────────
      String valStr = cmd.substring(1);
      valStr.trim();
      int value = valStr.toInt();

      if (value != 0 && value != 1) {
        Serial.println("ERR: U needs 0 or 1");
        return;
      }

      for (int i = 0; i < numUnknown; i++) {
        int pin = unknown[i];
        pinMode(pin, OUTPUT);
        digitalWrite(pin, value);
      }

      Serial.print("U → all unknown pins set to "); Serial.println(value);
    }     else if (cmd.startsWith("T")) {
    String pinStr = cmd.substring(1);
    pinStr.trim();
    int targetPin = pinStr.toInt();

    // For safety — only allow 1 or 3
    if (targetPin != 1 && targetPin != 3) {
        Serial.println("ERR: T only supports pin 1 (TX) or 3 (RX)");
        
    }
   else{
    Serial.println("!!! WARNING !!!");
    Serial.println(" Toggling serial pin — communication will be corrupted for ~2 seconds !!!");
    Serial.println(" You may see garbage or lose connection temporarily.");
    Serial.flush();  // Try to empty buffer before messing up

    // Remember original state
    int originalState = digitalRead(targetPin);

    pinMode(targetPin, OUTPUT);

    // Blink: 100 ms on / 100 ms off × 10 cycles = 2 seconds
    for (int i = 0; i < 10; i++) {
        digitalWrite(targetPin, HIGH);
        delay(100);
        digitalWrite(targetPin, LOW);
        delay(100);
    }

    // Restore
    digitalWrite(targetPin, originalState);
    //pinMode(targetPin, INPUT);  // or whatever makes sense — UART will re-take it anyway
    Serial.begin(115200);
    Serial.println("Toggle test finished — pin restored");
    Serial.println("If serial is garbled now → reset the board or re-open monitor");
   };//valid pin forflash
   }

    else {
      Serial.println("Unknown command. Supported: W R PM K U");
    }

    Serial.println();
  }
}
