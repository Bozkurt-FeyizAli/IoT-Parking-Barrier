#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h> // ESP32 için özel servo kütüphanesi

const int trigPin = 13;
const int echoPin = 12;

#define SOUND_SPEED 0.034
#define CM_TO_INCH 0.393701

long duration;
int distanceCm;
int distanceInch;

// ESP32 30-Pin Standart Bağlantıları
#define SS_PIN    5   // RC522 SDA (SS)
#define RST_PIN   4  // RC522 Reset
#define SERVO_PIN 26  // Servo Sinyal Pini


Servo motor;
MFRC522 rfid(SS_PIN, RST_PIN);

byte ID[4] = {4, 30, 71, 154}; // Yetkili Kart ID

void setup() {
  Serial.begin(115200);
  SPI.begin();          // Standart SPI başlatma (SCK:18, MISO:19, MOSI:23)
  rfid.PCD_Init();
  

  motor.setPeriodHertz(50);           // Standart 50Hz servo
  motor.attach(SERVO_PIN, 500, 2400); // Pin ve darbe genişlikleri
  motor.write(0); 

  pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin, INPUT); // Sets the echoPin as an Input

  
  delay(500);

  
}

void loop() {

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
  // Prints the distance in the Serial Monitor
  Serial.print("Distance (cm): ");
  Serial.println(distanceCm);
  Serial.print("Distance (inch): ");
  Serial.println(distanceInch);




  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial())
    return;
  
  if (rfid.uid.uidByte[0] == ID[0] && 
      rfid.uid.uidByte[1] == ID[1] &&
      rfid.uid.uidByte[2] == ID[2] &&
      rfid.uid.uidByte[3] == ID[3]) {
    
    Serial.println("Kapi acildi");
    ekranaYazdir();
    motor.write(180);
    delay(3000);
    motor.write(0);
    delay(1000);
  } else {
    Serial.println("Yetkisiz Kart");
    ekranaYazdir();
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