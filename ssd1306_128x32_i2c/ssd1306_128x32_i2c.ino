#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_ADDRESS 0x3C

#define BUTTON_PIN 1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);  // Buton aktif LOW

  Wire.begin(44, 3);  // SDA = 3, SCL = 44

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("OLED bulunamadi veya baslatilamadi");
    while (true); // Dur
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("OLED ve Buton Testi");
  display.display();
}

void loop() {
  bool pressed = digitalRead(BUTTON_PIN) == LOW;

  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(2);
  if (pressed) {
    display.println("BASILDI");
    Serial.println("BUTON: BASILDI");
  } else {
    display.println("BEKLIYOR");
    Serial.println("BUTON: DEGIL");
  }
  display.display();

  delay(200);
}
