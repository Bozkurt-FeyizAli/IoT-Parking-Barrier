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

#define SOUND_SPEED 0.034
#define CM_TO_INCH 0.393701

long duration;
int distanceCm;
int distanceInch;

boolean isAuthorized = false;
boolean isDoorOpen = false;
boolean slot1Full = false;
boolean slot2Full = false;

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
  slot1Full = digitalRead(sensor1Pin) == LOW; // LOW ise dolu
  slot2Full = digitalRead(sensor2Pin) == LOW; // LOW ise dolu
  // Clears the trigPin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  // Sets the trigPin on HIGH state for 10 micro seconds
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  // Reads the echoPin, returns the sound wave travel time in microseconds
  duration = pulseIn(echoPin, HIGH);
  
  // Calculate the distance
  distanceCm = duration * SOUND_SPEED/2;
  
  // Convert to inches
  distanceInch = distanceCm * CM_TO_INCH;
  
  int sensor1Value = digitalRead(sensor1Pin);
  int sensor2Value = digitalRead(sensor2Pin);
  // // Prints the distance in the Serial Monitor
  // Serial.print("Distance (cm): ");
  // Serial.println(distanceCm);
  // Serial.print("Distance (inch): ");
  // Serial.println(distanceInch);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 5);
  //Display distance in cm
  display.print("distanceCm: ");
  display.print(distanceCm);
  display.println(" cm");

  display.println();

  //Display distance in cm
  display.print("distanceInch: ");
  display.print(distanceInch);
  display.print(" inch");

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

  if (distanceCm > 20) {
    Serial.println("Lutfen yaklasin");
    ekranaYazdir();
    delay(2000);
    return;
  }


  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()){
    if(publicLogin())
      Serial.println("Genel Kart ile Giris Basarili");
    return;
  }
  else{
  if (privateLogin()) 
    Serial.println("Ozel Kart ile Giris Basarili");
  }
 rfid.PICC_HaltA();


}

void ekranaYazdir() {
  Serial.print("ID Numarasi: ");
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
  if(slot1Full==true){
    Serial.println("Park yeri dolu");
    return false;
  }
  else{
    openDoor();
    slot1Full = true; // Park yeri dolu olarak işaretle
    ekranaYazdir();
    return true;
  }
}

boolean publicLogin() {
  if(slot2Full==true){
    Serial.println("Park yeri dolu");
    return false;
  }
  else{
    slot2Full = true; // Park yeri dolu olarak işaretle
    openDoor();
    return true;
  }
   
}

void openDoor() {
  motor.write(180); // Kapıyı aç
  delay(3000); // Kapının açık kalma süresi
  motor.write(0); // Kapıyı kapat
}