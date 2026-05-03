#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h> // ESP32 için özel servo kütüphanesi

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

const int trigPin = 13;
const int echoPin = 12;

#define buttonPin 33
boolean lastButtonState = false;
int buttonPushedTime = 0;
int doorOpenTime = 0;

#define SOUND_SPEED 0.034
#define CM_TO_INCH 0.393701

long duration;
int distanceCm;
int distanceInch;

// boolean isAuthorized = false;
// boolean isDoorOpen = false;
int slot1Full = false;
int slot2Full = false;

// ESP32 30-Pin Standart Bağlantıları
#define SS_PIN    5   // RC522 SDA (SS)
#define RST_PIN   4  // RC522 Reset
#define SERVO_PIN 26  // Servo Sinyal Pini

#define sensor1Pin 27
#define sensor2Pin 14

Servo motor;
MFRC522 rfid(SS_PIN, RST_PIN);

byte ID[4] = {4, 30, 71, 154}; // Yetkili Kart ID

void setup() {
  Serial.begin(115200);
  SPI.begin();          // Standart SPI başlatma (SCK:18, MISO:19, MOSI:23)
  rfid.PCD_Init();

  pinMode(sensor1Pin, INPUT);
  pinMode(sensor2Pin, INPUT);
  pinMode(buttonPin, INPUT);
  pinMode(buttonPushed, INPUT);

  motor.setPeriodHertz(50);           // Standart 50Hz servo
  motor.attach(SERVO_PIN, 500, 2400); // Pin ve darbe genişlikleri
  motor.write(0);

  pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin, INPUT); // Sets the echoPin as an Input

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.clearDisplay();

  display.setTextSize(2);
  display.setTextColor(WHITE);

  delay(500);


}

void loop() {
  slot1Full = digitalRead(sensor1Pin); // LOW ise dolu
  slot2Full = digitalRead(sensor2Pin); // LOW ise dolu

  distanceCm = measureDistance();

  boolean buttonState= digitalRead(buttonPin);

  if(buttonPushedTime>0 && (millis() - buttonPushedTime) > 5000){
    closeDoor();
    buttonPushedTime = 0;
  }
  if(buttonState&& !lastButtonState){
      openDoor();
  }
  else{
      closeDoor();
  }
  lastButtonState = buttonState;


  if (isDoorOpen && (millis() - doorOpenTime) > 5000) {
    closeDoor();
    isDoorOpen = false;
  }

  int sensor1Value = digitalRead(sensor1Pin);
  int sensor2Value = digitalRead(sensor2Pin);

  if (sensor1Value != slot1Full) {
    if (sensor1Value == HIGH) {
      OpenDoor();
      isDoorOpen = true;
      doorOpenTime = millis();
      slot1Full = HIGH;
    }
  }
  if (sensor2Value != slot2Full) {
    if (sensor2Value == HIGH) {
      OpenDoor();
      isDoorOpen = true;
      doorOpenTime = millis();
      slot2Full = HIGH;
    }
  }

  displayOled(distanceCm, sensor1Value, sensor2Value);

  if (distanceCm > 20) {
    Serial.println("Please approach the parking lot");
    ekranaYazdir();
    delay(2000);
    return;
  }

  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()){
    if(publicLogin())
      Serial.println("Public user entrance successful");
    return;
  }
  else{
  if (privateLogin())
    Serial.println("Private user entrance successful");
  }
  rfid.PICC_HaltA();


}

void ekranaYazdir() {
  Serial.print("Card UID:");
  for (int sayac = 0; sayac < 4; sayac++) {
    Serial.print(rfid.uid.uidByte[sayac]);
    Serial.print(" ");
  }
  Serial.println("");
}

boolean privateLogin() {
  if (rfid.uid.uidByte[0] != ID[0] &&
      rfid.uid.uidByte[1] != ID[1] &&
      rfid.uid.uidByte[2] != ID[2] &&
      rfid.uid.uidByte[3] != ID[3]) {
    return false;
  }
  if(slot1Full==HIGH){
    Serial.println("Park slot 1 is full");
    return false;
  }
  else{
    openDoor();
    doorOpenTime = millis();
    slot1Full = HIGH; // Park yeri dolu olarak işaretle
    ekranaYazdir();
    return true;
  }
}

boolean publicLogin() {
  if(slot2Full==HIGH){
    Serial.println("Park slot 2 is full");
    return false;
  }
  else{
    slot2Full = HIGH; // Park yeri dolu olarak işaretle
    openDoor();
    doorOpenTime = millis();
    return true;
  }

}

void openDoor() {
  motor.write(90); // Kapıyı aç
  // delay(3000); // Kapının açık kalma süresi
  // motor.write(0); // Kapıyı kapat
}

void closeDoor() {
  motor.write(0); // Kapıyı kapat
}



void displayOled(int distanceCm, int sensor1Value, int sensor2Value) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 5);
  //Display distance in cm
  display.print("distanceCm: ");
  display.print(distanceCm);
  display.println(" cm");


  display.println();

  display.print("slot1: ");
  if (sensor1Value == HIGH) {
  display.print("free");
  } else {
  display.print("full");
  }

  display.println();

  display.print("slot2: ");
  if (sensor2Value == HIGH) {
  display.print("free");
  } else {
  display.print("full");
  }

  display.display();
}

int measureDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH);
  distanceCm = duration * SOUND_SPEED / 2;
  return distanceCm;
}
