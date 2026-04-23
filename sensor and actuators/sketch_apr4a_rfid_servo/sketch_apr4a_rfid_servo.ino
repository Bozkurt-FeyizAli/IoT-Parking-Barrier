#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h> // ESP32 için özel servo kütüphanesi

// ESP32 30-Pin Standart Bağlantıları
#define SS_PIN    5   // RC522 SDA (SS)
#define RST_PIN   22  // RC522 Reset
#define SERVO_PIN 13  // Servo Sinyal Pini

Servo motor;
MFRC522 rfid(SS_PIN, RST_PIN);

byte ID[4] = {179, 15, 241, 44}; // Yetkili Kart ID

void setup() {
  Serial.begin(115200); // ESP32 için önerilen hız
  SPI.begin();          // Standart SPI başlatma (SCK:18, MISO:19, MOSI:23)
  rfid.PCD_Init();
  
  motor.setPeriodHertz(50);           // Standart 50Hz servo
  motor.attach(SERVO_PIN, 500, 2400); // Pin ve darbe genişlikleri
  motor.write(0); 
}

void loop() {
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