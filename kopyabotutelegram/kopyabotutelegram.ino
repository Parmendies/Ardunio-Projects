#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <esp_camera.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>

// WiFi bilgileri
const char* ssid = "Bünyamin";
const char* password = "12345678";

// Telegram bot bilgileri
String botToken = "-********************";
String chat_id = "5943374104";

// OLED ekran
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);

// Buton pini
#define BUTTON_PIN 9

// Kamera pinleri (XIAO ESP32S3)
#define PWDN_GPIO_NUM  -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM  10
#define SIOD_GPIO_NUM  40
#define SIOC_GPIO_NUM  39
#define Y9_GPIO_NUM    48
#define Y8_GPIO_NUM    11
#define Y7_GPIO_NUM    12
#define Y6_GPIO_NUM    14
#define Y5_GPIO_NUM    16
#define Y4_GPIO_NUM    18
#define Y3_GPIO_NUM    17
#define Y2_GPIO_NUM    15
#define VSYNC_GPIO_NUM 38
#define HREF_GPIO_NUM  47
#define PCLK_GPIO_NUM  13

void setupCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA;
  config.fb_count = 1;

  esp_camera_init(&config);
}

void sendPhotoToTelegram() {
  Serial.println("Fotoğraf çekiliyor...");
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Kamera fotoğrafı alamadı.");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  String url = "https://api.telegram.org/bot" + botToken + "/sendPhoto";
  HTTPClient https;
  https.begin(client, url);
  https.addHeader("Content-Type", "multipart/form-data; boundary=123456");

  String bodyStart = "--123456\r\nContent-Disposition: form-data; name=\"chat_id\"\r\n\r\n" + chat_id + "\r\n";
  bodyStart += "--123456\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"photo.jpg\"\r\n";
  bodyStart += "Content-Type: image/jpeg\r\n\r\n";

  String bodyEnd = "\r\n--123456--\r\n";

  int contentLength = bodyStart.length() + fb->len + bodyEnd.length();
  https.addHeader("Content-Length", String(contentLength));

  int code = https.sendRequest("POST");

  WiFiClient *stream = https.getStreamPtr();
  stream->print(bodyStart);
  stream->write(fb->buf, fb->len);
  stream->print(bodyEnd);

  esp_camera_fb_return(fb);
  https.end();

  Serial.println("Fotoğraf gönderildi.");
  display.clearDisplay();
  display.setCursor(0, 10);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.println("GONDERILDI");
  display.display();
}

String getLastMessageText() {
  HTTPClient http;
  String url = "https://api.telegram.org/bot" + botToken + "/getUpdates";
  http.begin(url);
  int httpCode = http.GET();
  String payload;

  if (httpCode == 200) {
    payload = http.getString();
    JsonDocument doc;
    deserializeJson(doc, payload);
    if (doc["result"].size() > 0) {
      int last = doc["result"].size() - 1;
      return doc["result"][last]["message"]["text"].as<String>();
    }
  }

  http.end();
  return "";
}

void scrollMessage(String msg) {
  int textX = SCREEN_WIDTH;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  while (textX + msg.length() * 6 > 0) {
    display.clearDisplay();
    display.setCursor(textX, 10);
    display.print(msg);
    display.display();
    textX -= 2;
    delay(100);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Wire.begin(44, 3);  // SDA, SCL

  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.print("WiFi baglaniyor...");
  display.display();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi baglandi");

  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Baglandi");
  display.display();

  setupCamera();
}

void loop() {
  if (digitalRead(BUTTON_PIN) == LOW) {
    Serial.println("Butona basildi");
    sendPhotoToTelegram();
    delay(5000);
  }

  String message = getLastMessageText();
  if (message.length() > 0) {
    scrollMessage(message);
  }

  delay(10000);
}
