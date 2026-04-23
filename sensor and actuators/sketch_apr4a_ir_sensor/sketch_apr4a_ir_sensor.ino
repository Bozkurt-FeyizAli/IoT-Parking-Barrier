const int sensorPin = 12;

void setup() {
pinMode(sensorPin, INPUT);
Serial.begin(115200);
}

void loop() {
int sensorValue = digitalRead(sensorPin);

if (sensorValue == LOW) {
Serial.println("Object detected");
} else {
Serial.println("No object detected");
}

delay(1000);
}