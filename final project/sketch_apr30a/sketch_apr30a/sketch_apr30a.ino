/*
   Smart Parking Barrier
   CSE 328 Term Project – Feyiz Ali Bozkurt
*/

#include "thingProperties.h"
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- OLED ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --- Ultrasonic ---
const int trigPin = 13;
const int echoPin = 12;
#define SOUND_SPEED 0.034

// --- RFID ---
#define SS_PIN    5
#define RST_PIN   4
MFRC522 rfid(SS_PIN, RST_PIN);
byte authorizedID[4] = {4, 30, 71, 154};

// --- Servo ---
#define SERVO_PIN 26
Servo motor;
bool isDoorOpen = false;
unsigned long doorOpenTime = 0;
unsigned long lastDoorAction = 0;
const unsigned long doorCooldown = 2000;   // 2 seconds cooldown

// --- Button (internal pull-up: LOW = pressed) ---
#define buttonPin 33
bool lastButtonState = HIGH;               // unpressed
unsigned long buttonDebounceTime = 0;
const unsigned long debounceDelay = 50;    // 50 ms

// --- IR sensors ---
#define sensor1Pin 27
#define sensor2Pin 14

// --- Slot LEDs ---
#define ledSlot1 32
#define ledSlot2 25

// --- Exit tracking ---
bool last_s1_full = false;
bool last_s2_full = false;
unsigned long exitTimerSlot2 = 0;
bool waitingExitSlot2 = false;
bool waitingExitSlot1 = false;

// --- Gate activity state machine ---
int gateState = 0;                // 0=idle,1=opening,2=open,3=closing
unsigned long gateStateTimer = 0;

// --- Cloud timing ---
unsigned long lastCloudPublishTime = 0;

unsigned long gateActivityTime = 0;

// --- Function prototypes ---
int measureDistance();
void openDoor();
void closeDoor();
void updateLEDs(bool s1, bool s2);
void displayOLED(int dist, bool s1, bool s2);
String getRFIDString();
void sanitiseMode();

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(sensor1Pin, INPUT);
  pinMode(sensor2Pin, INPUT);
  pinMode(buttonPin, INPUT_PULLUP);   // LOW when pressed

  pinMode(ledSlot1, OUTPUT);
  pinMode(ledSlot2, OUTPUT);
  digitalWrite(ledSlot1, LOW);
  digitalWrite(ledSlot2, LOW);

  motor.setPeriodHertz(50);
  motor.attach(SERVO_PIN, 500, 2400);
  motor.write(0);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED init failed"));
    while (1);
  }
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(20, 20);
  display.println("Parking");
  display.println(" System");
  display.display();
  delay(2000);

  initProperties();
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);

  // Sanitise cloud variables
  sanitiseMode();
  barrier_switch = false;
  message = "System Ready";
  unauthorized_attempts = 0;
  gate_activity = 0;
  rfid_uid = "";
  slot1 = false;
  slot2 = false;
  distance = 0;

  Serial.println("Setup complete.");
}

// ==================== LOOP ====================
void loop() {
  ArduinoCloud.update();

  if (ArduinoCloud.connected()) {
    lastCloudPublishTime = millis();
  }

  // --- Read sensors ---
  int dist = measureDistance();
  bool s1_full = (digitalRead(sensor1Pin) == LOW);   // LOW = blocked
  bool s2_full = (digitalRead(sensor2Pin) == LOW);

  // Update cloud variables
  distance = dist;
  slot1 = s1_full;
  slot2 = s2_full;
  updateLEDs(s1_full, s2_full);

  // Debug output
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug > 1000) {
    Serial.print("Dist:"); Serial.print(dist);
    Serial.print(" S1:"); Serial.print(s1_full);
    Serial.print(" S2:"); Serial.print(s2_full);
    Serial.print(" Mod:"); Serial.print(mode);
    Serial.print(" Switch:"); Serial.print(barrier_switch);
    Serial.print(" Btn:"); Serial.print(digitalRead(buttonPin));
    Serial.print(" Door:"); Serial.println(isDoorOpen ? "OPEN" : "CLOSE");
    lastDebug = millis();
  }

  // Handle exit detection
  if (last_s1_full && !s1_full) {
    waitingExitSlot1 = true;
    message = "Private exit: tap card";
  }
  if (last_s2_full && !s2_full) {
    exitTimerSlot2 = millis();
    waitingExitSlot2 = true;
    message = "Public exit: opening...";
  }
  last_s1_full = s1_full;
  last_s2_full = s2_full;

  // Auto-open public exit after 1 second
  if (waitingExitSlot2 && (millis() - exitTimerSlot2 > 1000)) {
    if (!isDoorOpen && (millis() - lastDoorAction > doorCooldown)) {
      openDoor();
      waitingExitSlot2 = false;
      message = "Public exit allowed";
    }
  }

  // Handle RFID scans
  if (dist > 0 && dist < 20) {
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      String uid = getRFIDString();
      rfid_uid = uid;

      bool authorized = true;
      for (int i = 0; i < 4; i++) {
        if (rfid.uid.uidByte[i] != authorizedID[i]) {
          authorized = false;
          break;
        }
      }

      // Private exit waiting for card
      if (waitingExitSlot1 && authorized) {
        if (!isDoorOpen && (millis() - lastDoorAction > doorCooldown)) {
          openDoor();
          waitingExitSlot1 = false;
          message = "Private exit: " + uid;
          gate_activity = 1;
          gateActivityTime = millis(); 
        }
      }
      // AUTO mode entry
      else if (mode.equals("AUTO") && !waitingExitSlot1 && !waitingExitSlot2) {
        if (authorized) {
          if (!s1_full) {
            if (!isDoorOpen && (millis() - lastDoorAction > doorCooldown)) {
              openDoor();
              message = "Private entry: " + uid;
              gate_activity = 1;
              gateActivityTime = millis(); 
            }
          } else {
            message = "Slot1 full";
          }
        } else {
          unauthorized_attempts++;
          message = "Unauthorised card: " + uid;
        }
      }
      // MANUAL mode – cards ignored
      else if (mode.equals("MANUAL")) {
        message = "MANUAL: card ignored";
      }

      rfid.PICC_HaltA();
      delay(100);
    }
  }

  // Handle physical button press with debouncing
  static bool buttonState = HIGH;
  bool reading = digitalRead(buttonPin);

  if (reading != lastButtonState) {
    buttonDebounceTime = millis();
  }

  if ((millis() - buttonDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;

      if (buttonState == LOW) { // Button is pressed
        Serial.println("Button press detected");

        if (dist > 0 && dist < 20) { // Car is close enough
          
          if (mode.equals("AUTO")) {
            if (s2_full) {
              message = "Slot2 full";
            } else if (!isDoorOpen && (millis() - lastDoorAction > doorCooldown)) {
              // Override exit states and open gate
              waitingExitSlot1 = false;
              waitingExitSlot2 = false;

              openDoor();
              message = "Public entry (button)";
              gate_activity = 1;
              gateActivityTime = millis(); 
            }
          } 
          else if (mode.equals("MANUAL")) {
            if (!isDoorOpen && (millis() - lastDoorAction > doorCooldown)) {
              openDoor();
              message = "Manual button";
            }
          }
        } else {
          Serial.println("No car detected near the barrier.");
        }
      }
    }
  }

  lastButtonState = reading;

  // ========== MANUAL MODE – DASHBOARD SWITCH ==========
  if (mode == "MANUAL") {
    if (barrier_switch && !isDoorOpen && (millis() - lastDoorAction > doorCooldown)) {
      openDoor();
      message = "Dashboard override OPEN";
    } else if (!barrier_switch && isDoorOpen) {
      closeDoor();
    }
  }

  // ========== AUTO-CLOSE AFTER 5 SECONDS ==========
  if (mode == "AUTO" && isDoorOpen && (millis() - doorOpenTime > 5000)) {
    closeDoor();
  }

  // ========== GATE ACTIVITY RESET ==========
if (gate_activity == 1 && millis() - gateActivityTime > 3000) {
  gate_activity = 0;
}

   // ========== GATE ACTIVITY STATE MACHINE ==========
  static int gateState = 0;                    // internal state
  static unsigned long gateStateTimer = 0;

  switch (gateState) {
    case 0: // idle – nothing to do
      break;

    case 1: // opening – hold for 1 second, then switch to fully open
      gate_activity = 1;
      if (millis() - gateStateTimer > 1000) {
        gateState = 2;
        gateStateTimer = millis();
        gate_activity = 2;   // fully open
        ArduinoCloud.update(); // push immediately
      }
      break;

    case 2: // fully open – stay here until door closes
      gate_activity = 2;
      if (!isDoorOpen) {     // door closed by auto-close or manual close
        gateState = 3;
        gateStateTimer = millis();
        gate_activity = 3;   // closing
        ArduinoCloud.update();
      }
      break;

    case 3: // closing – hold for 1 second, then back to idle
      gate_activity = 3;
      if (millis() - gateStateTimer > 1000) {
        gateState = 0;
        gate_activity = 0;
        ArduinoCloud.update();
      }
      break;
  }

  displayOLED(dist, s1_full, s2_full);
  
  delay(50);
}

// ==================== HELPER FUNCTIONS ====================
int measureDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 30000UL);
  if (duration == 0) return -1;
  return (int)(duration * SOUND_SPEED / 2);
}

void openDoor() {
  motor.write(90);
  isDoorOpen = true;
  doorOpenTime = millis();
  lastDoorAction = millis();

  // Start gate activity sequence
  gateState = 1;
  gateStateTimer = millis();
  gate_activity = 1;
  ArduinoCloud.update();
}

void closeDoor() {
  motor.write(0);
  isDoorOpen = false;
  lastDoorAction = millis();
}

void updateLEDs(bool s1_full, bool s2_full) {
  digitalWrite(ledSlot1, s1_full ? HIGH : LOW);
  digitalWrite(ledSlot2, s2_full ? HIGH : LOW);
}

String getRFIDString() {
  String uidStr = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uidStr += "0";
    uidStr += String(rfid.uid.uidByte[i], HEX);
    if (i < rfid.uid.size - 1) uidStr += ":";
  }
  return uidStr;
}

void displayOLED(int dist, bool s1_full, bool s2_full) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);

  display.print("Distance: ");
  if (dist < 0) display.print("ERR");
  else { display.print(dist); display.print(" cm"); }
  display.println();

  display.print("S1: "); display.print(s1_full ? "FULL" : "FREE");
  display.print("  S2: "); display.println(s2_full ? "FULL" : "FREE");

  display.print("IP: "); display.println(WiFi.localIP().toString());
  display.print("RSSI: "); display.print(WiFi.RSSI()); display.println(" dBm");

  display.print("Cloud: "); display.print(ArduinoCloud.connected() ? "ON" : "OFF");
  unsigned long sec = (millis() - lastCloudPublishTime) / 1000;
  display.print("  Last: "); display.print(sec); display.println("s");

  display.print("Mode: "); display.print(mode);
  display.display();
}

void sanitiseMode() {
  if (!mode.equals("AUTO") && !mode.equals("MANUAL")) {
    mode = "AUTO";
  }
}

// Arduino Cloud callbacks
void onModeChange() {
  sanitiseMode();
}
void onBarrierSwitchChange() { }