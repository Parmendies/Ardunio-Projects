#define BUTTON_PIN 2  // D10 = GPIO9

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void loop() {
  if (digitalRead(BUTTON_PIN) == LOW) {
    Serial.println("BASILDI 0");
  } 
  if (digitalRead(1) == LOW) {
    Serial.println("BASILDI 1");
  } 
  if (digitalRead(2) == LOW) {
    Serial.println("BASILDI 2");
  } 
  if (digitalRead(3) == LOW) {
   Serial.println("BASILDI 3");
  } 
  delay(200);


}
