#include <Wire.h>

void setup() {
  Wire.begin(7, 8); // SDA, SCL
  Serial.begin(115200);
  while (!Serial);
}

void loop() {
  Serial.println("I2C cihaz taranıyor...");

  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      Serial.print("I2C cihaz bulundu: 0x");
      Serial.println(address, HEX);
    }
  }

  Serial.println("Tarama tamamlandı.");
  Serial.println("----------------------");
  delay(2000); // her 2 saniyede bir tarasın
}
