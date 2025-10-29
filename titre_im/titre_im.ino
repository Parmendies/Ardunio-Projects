const int sensorPin = 10;  // GPIO10

void setup() {
  Serial.begin(115200);
  pinMode(sensorPin, INPUT_PULLDOWN);  // Gerekirse pull-down aktif et
}

void loop() {
  int state = digitalRead(sensorPin);
  Serial.println(state);  // 0 veya 1 yazar
  delay(10);              // 10 ms bekle
}
