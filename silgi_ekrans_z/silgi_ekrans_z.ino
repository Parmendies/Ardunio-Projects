#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <esp_camera.h>

// WiFi bilgileri
const char* ssid = "Bünyamin";
const char* password = "12345678";
String botToken = dotenv.env['BOT_TOKEN']!;
String chat_id = "5943374104";
//5943374104
//1919049309

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

unsigned long lastDebounceTime = 0, debounceDelay = 50;
bool lastButtonState = HIGH, buttonHandled = false;
unsigned long lastSendTime = 0, cooldown = 2000;

// Kamera bir kez başlat, sürekli açık tut
bool cameraInitialized = false;

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // WiFi başlat
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[OK] WiFi bağlandı");

  // Kamerayı başlat ve açık tut
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
  
  // Maksimum kalite ayarları
  config.frame_size = FRAMESIZE_UXGA;    // 1600x1200 - maksimum çözünürlük
  config.jpeg_quality = 4;               // Çok yüksek kalite (4-6 arası ideal)
  config.fb_count = 1;                   // RAM tasarrufu

  if (esp_camera_init(&config) == ESP_OK) {
    Serial.println("[OK] Kamera başlatıldı");
    
    // Yakın çekim için sensor ayarları
    sensor_t * s = esp_camera_sensor_get();
    s->set_brightness(s, 0);     // Parlaklık (0 = normal)
    s->set_contrast(s, 1);       // Kontrast artır
    s->set_saturation(s, 0);     // Renk doygunluğu
    s->set_special_effect(s, 0); // Efekt yok
    s->set_whitebal(s, 1);       // Beyaz dengesi otomatik
    s->set_awb_gain(s, 1);       // AWB gain
    s->set_wb_mode(s, 0);        // WB modu otomatik
    s->set_exposure_ctrl(s, 1);  // Pozlama kontrolü
    s->set_aec2(s, 0);           // AEC DSP
    s->set_ae_level(s, 0);       // AE seviyesi
    s->set_aec_value(s, 300);    // Pozlama değeri
    s->set_gain_ctrl(s, 1);      // Gain kontrolü
    s->set_agc_gain(s, 0);       // AGC gain
    s->set_gainceiling(s, (gainceiling_t)2); // Gain tavan
    s->set_bpc(s, 0);            // BPC kapalı
    s->set_wpc(s, 1);            // WPC açık
    s->set_raw_gma(s, 1);        // Raw GMA
    s->set_lenc(s, 1);           // Lens düzeltme
    s->set_hmirror(s, 0);        // Yatay ayna
    s->set_vflip(s, 0);          // Dikey çevirme
    s->set_dcw(s, 1);            // DCW
    s->set_colorbar(s, 0);       // Color bar kapalı
    
    cameraInitialized = true;
    delay(2000); // Sensör ayarları oturmaya kadar bekle
  } else {
    Serial.println("[HATA] Kamera başlatılamadı!");
  }
}

void loop() {
  bool reading = digitalRead(BUTTON_PIN);
  
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading == LOW && !buttonHandled && millis() - lastSendTime > cooldown) {
      Serial.println("[FOTO] Çekiliyor...");
      buttonHandled = true;
      lastSendTime = millis();
      
      if (sendPhotoToTelegram()) {
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

bool sendPhotoToTelegram() {
  // WiFi kontrolü
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[HATA] WiFi yok");
    return false;
  }

  // RAM kontrolü - yüksek kalite için daha fazla RAM gerekli
  if (ESP.getFreeHeap() < 200000) {
    Serial.println("[HATA] Yetersiz RAM");
    return false;
  }

  // Kamera hazır mı kontrol et
  if (!cameraInitialized) {
    initCamera();
    if (!cameraInitialized) return false;
  }

  // Fotoğraf çek
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb || fb->len == 0) {
    Serial.println("[HATA] Fotoğraf çekilemedi");
    if (fb) esp_camera_fb_return(fb);
    return false;
  }

  // HTTP client hazırla
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;
  
  String serverPath = "https://api.telegram.org/bot" + botToken + "/sendPhoto";
  
  if (!https.begin(client, serverPath)) {
    Serial.println("[HATA] HTTPS başlatılamadı");
    esp_camera_fb_return(fb);
    return false;
  }

  // Multipart body hazırla
  String bodyStart = "--RandomBoundary\r\n"
                     "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n" + chat_id + "\r\n"
                     "--RandomBoundary\r\n"
                     "Content-Disposition: form-data; name=\"photo\"; filename=\"photo.jpg\"\r\n"
                     "Content-Type: image/jpeg\r\n\r\n";

  String bodyEnd = "\r\n--RandomBoundary--\r\n";
  size_t totalLen = bodyStart.length() + fb->len + bodyEnd.length();
  
  uint8_t* body = (uint8_t*)malloc(totalLen);
  if (!body) {
    Serial.println("[HATA] Malloc başarısız");
    esp_camera_fb_return(fb);
    https.end();
    return false;
  }

  // Body'yi birleştir
  memcpy(body, bodyStart.c_str(), bodyStart.length());
  memcpy(body + bodyStart.length(), fb->buf, fb->len);
  memcpy(body + bodyStart.length() + fb->len, bodyEnd.c_str(), bodyEnd.length());

  // POST gönder
  https.addHeader("Content-Type", "multipart/form-data; boundary=RandomBoundary");
  https.setTimeout(15000);
  int httpCode = https.POST(body, totalLen);

  // Temizlik
  free(body);
  esp_camera_fb_return(fb);
  https.end();

  // Sonuç kontrol
  if (httpCode == 200) {
    return true;
  } else {
    Serial.printf("[HATA] HTTP: %d\n", httpCode);
    return false;
  }
}
