#include <WiFi.h>
#include <HTTPClient.h>
#include <esp_camera.h>

// WiFi bilgileri
const char* ssid = "Bünyamin";
const char* password = "12345678";

// Flutter uygulamasının IP adresi (Hotspot açan telefonun IP'si)
const char* phone_ip = "10.152.158.35";

// Buton pini
#define BUTTON_PIN 1

// Kamera pinleri (Xiao ESP32S3 Mini için)
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

bool cameraInitialized = false;
bool lastButtonState = HIGH;
unsigned long lastDebounce = 0;
const int debounceDelay = 50;
bool buttonHandled = false;

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  WiFi.begin(ssid, password);
  Serial.print("WiFi bağlanıyor");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[OK] WiFi bağlı");

  initCamera();
}

void initCamera() {
  if (cameraInitialized) return;

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
//FRAMESIZE_QVGA  320x240 (çok hızlı, düşük kalite)
//FRAMESIZE_VGA 640x480 (orta seviye)
//FRAMESIZE_SVGA  800x600 (dengeli)
//FRAMESIZE_XGA 1024x768
//FRAMESIZE_UXGA  1600x1200 (yüksek kalite, yavaş)
  config.jpeg_quality = 5;

  config.fb_count = 1;

  if (esp_camera_init(&config) == ESP_OK) {
    cameraInitialized = true;
    Serial.println("[OK] Kamera başlatıldı");
  } else {
    Serial.println("[HATA] Kamera başlatılamadı");
  }
}

void loop() {
  bool reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonState) {
    lastDebounce = millis();
  }

  if ((millis() - lastDebounce) > debounceDelay) {
    if (reading == LOW && !buttonHandled) {
      buttonHandled = true;
      Serial.println("[BUTON] Basıldı, fotoğraf çekiliyor...");
      if (sendPhotoToPhone()) {
        Serial.println("[OK] Gönderildi");
      } else {
        Serial.println("[HATA] Gönderilemedi");
      }
    } else if (reading == HIGH) {
      buttonHandled = false;
    }
  }
  lastButtonState = reading;
}
bool sendPhotoToPhone() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[HATA] WiFi bağlı değil");
    return false;
  }

  Serial.println("[INFO] Fotoğraf çekiliyor...");
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb || fb->len == 0) {
    Serial.println("[HATA] Fotoğraf alınamadı");
    if (fb) esp_camera_fb_return(fb);
    return false;
  }

  Serial.printf("[INFO] Fotoğraf alındı - Boyut: %d byte\n", fb->len);

  WiFiClient client;
  HTTPClient http;
  String url = String("http://") + phone_ip + ":8080/upload";

  Serial.print("[INFO] HTTP başlatılıyor: ");
  Serial.println(url);

  if (!http.begin(client, url)) {
    Serial.println("[HATA] HTTP başlatılamadı");
    esp_camera_fb_return(fb);
    return false;
  }

  String bodyStart = "--bound\r\n"
                     "Content-Disposition: form-data; name=\"photo\"; filename=\"photo.jpg\"\r\n"
                     "Content-Type: image/jpeg\r\n\r\n";
  String bodyEnd = "\r\n--bound--\r\n";

  size_t totalLen = bodyStart.length() + fb->len + bodyEnd.length();
  uint8_t* body = (uint8_t*)malloc(totalLen);
  if (!body) {
    Serial.println("[HATA] RAM yetersiz - body malloc başarısız");
    esp_camera_fb_return(fb);
    http.end();
    return false;
  }

  memcpy(body, bodyStart.c_str(), bodyStart.length());
  memcpy(body + bodyStart.length(), fb->buf, fb->len);
  memcpy(body + bodyStart.length() + fb->len, bodyEnd.c_str(), bodyEnd.length());

  http.addHeader("Content-Type", "multipart/form-data; boundary=bound");
  http.setTimeout(15000); // 15 saniye zaman aşımı

  Serial.println("[INFO] POST gönderiliyor...");
  int code = http.POST(body, totalLen);
  Serial.printf("[INFO] HTTP Yanıt Kodu: %d\n", code);

  free(body);
  esp_camera_fb_return(fb);
  http.end();

  if (code == 200) {
    Serial.println("[OK] Fotoğraf başarıyla gönderildi");
    return true;
  } else {
    Serial.println("[HATA] Fotoğraf gönderilemedi");
    return false;
  }
}
