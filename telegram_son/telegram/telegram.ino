
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

String botToken = dotenv.env['BOT_TOKEN']!;

String chat_id = "1919049309";

// OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);

// Buton
#define BUTTON_PIN 1

// Kamera pinleri
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

String currentMessage = "Waiting for message...";
unsigned long lastDebounceTime = 0, debounceDelay = 50;
bool lastButtonState = HIGH, buttonHandled = false;
unsigned long lastSendTime = 0, cooldown = 3000;
unsigned long lastMessageCheckTime = 0, messageCheckInterval = 5000;
int scrollX = SCREEN_WIDTH;
unsigned long lastScrollTime = 0, scrollInterval = 30;

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Wire.begin(44, 3);  // SDA = 3, SCL = 44
  
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  showStaticMessage("Connecting WiFi...");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  showStaticMessage("WiFi Connected");
  delay(500);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_UXGA;
  config.jpeg_quality = 7;
  config.fb_count = 1;

  if (esp_camera_init(&config) != ESP_OK) showError("Camera Error");
}

void loop() {
  bool reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonState) lastDebounceTime = millis();
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading == LOW && !buttonHandled && millis() - lastSendTime > cooldown) {
      buttonHandled = true;
      lastSendTime = millis();
      showStaticMessage("Foto cekiliyor...");
      if (sendPhotoToTelegram()) showStaticMessage("Foto gonderildi");
      else showError("Foto gonderilemedi");
      delay(500);
      scrollX = SCREEN_WIDTH;
    } else if (reading == HIGH) {
      buttonHandled = false;
    }
  }
  lastButtonState = reading;

  if (millis() - lastMessageCheckTime > messageCheckInterval) {
    String newMsg = getLastMessageFromTelegram();
    if (newMsg != "" && newMsg != currentMessage) {
      currentMessage = newMsg;
      scrollX = SCREEN_WIDTH;
    }
    lastMessageCheckTime = millis();
  }

  if (millis() - lastScrollTime > scrollInterval) {
    scrollMessage();
    lastScrollTime = millis();
  }
}

void scrollMessage() {
  display.clearDisplay();
  display.setCursor(scrollX, 10);
  display.println(convertTurkishChars(currentMessage));
  display.display();
  scrollX--;
  int textWidth = currentMessage.length() * 6;
  if (scrollX < -textWidth) scrollX = SCREEN_WIDTH;
}

void showStaticMessage(String msg) {
  display.clearDisplay();
  display.setCursor(0, 10);
  display.println(convertTurkishChars(msg));
  display.display();
}

void showError(String errorMsg) {
  display.clearDisplay();
  display.setCursor(0, 10);
  display.println(errorMsg);
  display.display();
  delay(1000);
}

bool sendPhotoToTelegram() {

  showStaticMessage("2");
  delay(1000);
  showStaticMessage("1");
  delay(1000);
  camera_fb_t *dummy = esp_camera_fb_get();
  if (dummy) esp_camera_fb_return(dummy);
  delay(600);
  
  if (ESP.getFreeHeap() < 150000) {
    Serial.println("[RAM] Yetersiz RAM, fotoğraf çekilmiyor.");
    return false;
  }

  Serial.println("[Kamera] Fotoğraf alınıyor...");
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("[Kamera] Fotoğraf alınamadı (NULL).");
    return false;
  }
  
  showStaticMessage("Foto gönderiliyor");

  Serial.printf("[Kamera] Fotoğraf boyutu: %d byte\n", fb->len);

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;

  String serverPath = "https://api.telegram.org/bot" + botToken + "/sendPhoto";

  if (!https.begin(client, serverPath)) {
    Serial.println("[HTTP] HTTPS başlatılamadı!");
    esp_camera_fb_return(fb);
    return false;
  }
  Serial.println("[HTTP] HTTPS bağlantı başarılı.");

  String bodyStart = "--RandomBoundary\r\n"
                     "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n" + chat_id + "\r\n"
                     "--RandomBoundary\r\n"
                     "Content-Disposition: form-data; name=\"photo\"; filename=\"photo.jpg\"\r\n"
                     "Content-Type: image/jpeg\r\n\r\n";

  String bodyEnd = "\r\n--RandomBoundary--\r\n";

  // toplam boyutu hesapla
  size_t totalLen = bodyStart.length() + fb->len + bodyEnd.length();

  uint8_t* body = (uint8_t*)malloc(totalLen);
  if (!body) {
    Serial.println("[RAM] Body oluşturulamadı!");
    esp_camera_fb_return(fb);
    https.end();
    return false;
  }

  memcpy(body, bodyStart.c_str(), bodyStart.length());
  memcpy(body + bodyStart.length(), fb->buf, fb->len);
  memcpy(body + bodyStart.length() + fb->len, bodyEnd.c_str(), bodyEnd.length());

  https.addHeader("Content-Type", "multipart/form-data; boundary=RandomBoundary");
  
  Serial.println("[HTTP] POST gönderiliyor...");
  int httpCode = https.POST(body, totalLen);

  free(body);
  esp_camera_fb_return(fb);
  https.end();

  Serial.printf("[HTTP] POST sonucu: %d\n", httpCode);

  if (httpCode > 0) {
    Serial.println("[OK] Fotoğraf gönderildi.");
    return true;
  } else {
    Serial.printf("[HATA] Gönderilemedi: %s\n", https.errorToString(httpCode).c_str());
    return false;
  }
}



String getLastMessageFromTelegram() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;
  String serverPath = "https://api.telegram.org/bot" + botToken + "/getUpdates";

  if (!https.begin(client, serverPath)) return "";

  int httpResponseCode = https.GET();
  String payload = "";

  if (httpResponseCode == 200) {
    payload = https.getString();
    StaticJsonDocument<4096> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      JsonArray results = doc["result"];
      if (results.size() > 0) {
        JsonObject last = results[results.size() - 1];
        String text = last["message"]["text"];
        https.end();
        return text;
      }
    }
  }
  https.end();
  return "";
}

String convertTurkishChars(String text) {
  text.replace("ç", "c");
  text.replace("Ç", "C");
  text.replace("ğ", "g");
  text.replace("Ğ", "G");
  text.replace("ı", "i");
  text.replace("İ", "I");
  text.replace("ö", "o");
  text.replace("Ö", "O");
  text.replace("ş", "s");
  text.replace("Ş", "S");
  text.replace("ü", "u");
  text.replace("Ü", "U");
  return text;
}
