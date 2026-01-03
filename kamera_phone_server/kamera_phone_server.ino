#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>

// ============================================
// YAPILANDIRMA DEĞİŞKENLERİ
// ============================================
const char* serverUrl = "http://192.168.1.5:8080/upload";
const char* ssid = "Hidden Network";
const char* password = "123omer_";
const size_t fb_count = 2;
// Kamera ayarları
const int xclk_freq_hz = 20000000; // orj 2
//#define FRAME_SIZE FRAMESIZE_QXGA // 2048x1536 (bir üst çözünürlük)
//#define FRAME_SIZE FRAMESIZE_5MP    // 2592x1944 (OV5640 gibi bazı sensörlerde)
//#define FRAME_SIZE FRAMESIZE_QSXGA  // 2592x1944 (kütüphaneye bağlı olarak)
//#define FRAME_SIZE FRAMESIZE_UXGA   // 1600x1200
#define FRAME_SIZE FRAMESIZE_SXGA   // 1280x1024
//#define FRAME_SIZE FRAMESIZE_HD     // 1280x720 // 1600x1200
//#define JPEG_QUALITY 4 low is quility
#define JPEG_QUALITY 10
#define CHUNK_SIZE 16384


// Zamanlama
#define CAPTURE_INTERVAL 10000  // 15 saniye (ms)

// XIAO ESP32S3 Sense kamera pinleri
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     10
#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39
#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15
#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13

// ============================================
// SETUP
// ============================================
void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n╔════════════════════════════════╗");
  Serial.println("║   ESP32-CAM Başlatılıyor...   ║");
  Serial.println("╚════════════════════════════════╝\n");
  
  // WiFi bağlantısı
  Serial.print("📡 WiFi'ye bağlanıyor: ");
  Serial.println(ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("✅ WiFi bağlandı!");
    Serial.print("   IP Adresi: ");
    Serial.println(WiFi.localIP());
    Serial.print("   Sunucu: ");
    Serial.println(serverUrl);
  } else {
    Serial.println("❌ WiFi bağlanamadı! Yeniden başlatılıyor...");
    delay(3000);
    ESP.restart();
  }
  
  delay(1000);
  
  // Kamera konfigürasyonu
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
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = xclk_freq_hz;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY; 
  config.frame_size = FRAME_SIZE;
  config.jpeg_quality = JPEG_QUALITY;
  config.fb_count = fb_count;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  
  Serial.println("📷 Kamera başlatılıyor...");
  
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("❌ Kamera hatası: 0x%x\n", err);
    delay(3000);
    ESP.restart();
    return;
  }
  
  Serial.println("✅ Kamera başlatıldı!");
  Serial.println("   • Çözünürlük: 2048x1536 (QXGA)");
  Serial.println("   • Kalite: Maksimum (Q=4)");
  Serial.println("   • Chunk: 16KB");
  Serial.println("   • Aralık: 15 saniye");
  
  Serial.println("\n╔════════════════════════════════╗");
  Serial.println("║   🚀 Sistem Hazır!            ║");
  Serial.println("╚════════════════════════════════╝\n");
  delay(1000);
}

// ============================================
// GÖRÜNTÜ GÖNDERME FONKSİYONU
// ============================================
bool sendImageInChunks() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ WiFi bağlantısı yok!");
    return false;
  }
  
  // Bu resim için benzersiz session ID oluştur
  String sessionId = String(millis()) + "_" + String(random(1000, 9999));
  
  Serial.println("📸 Resim çekiliyor...");
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("❌ Kamera görüntüsü alınamadı!");
    return false;
  }
  
  float sizeKB = fb->len / 1024.0;
  Serial.printf("✅ Görüntü: %.1f KB (%dx%d)\n", sizeKB, fb->width, fb->height);
  Serial.printf("🔑 Session: %s\n", sessionId.c_str());
  
  HTTPClient http;
  http.setTimeout(20000);
  
  int totalChunks = (fb->len + CHUNK_SIZE - 1) / CHUNK_SIZE;
  Serial.printf("📦 %d chunk'a bölünecek (her chunk ~16KB)\n\n", totalChunks);
  
  bool success = true;
  unsigned long startTime = millis();
  
  // Her chunk'ı gönder
  for (int i = 0; i < totalChunks; i++) {
    int chunkStart = i * CHUNK_SIZE;
    int chunkEnd = min(chunkStart + CHUNK_SIZE, (int)fb->len);
    int currentChunkSize = chunkEnd - chunkStart;
    
    String url = String(serverUrl) + 
                 "?chunk=" + String(i) + 
                 "&total=" + String(totalChunks) + 
                 "&size=" + String(fb->len) +
                 "&session=" + sessionId;
    
    http.begin(url);
    http.addHeader("Content-Type", "application/octet-stream");
    
    // Retry mekanizması
    int retries = 3;
    int httpCode = -1;
    
    while (retries > 0) {
      httpCode = http.POST(&fb->buf[chunkStart], currentChunkSize);
      
      if (httpCode == 200) {
        break;
      }
      
      retries--;
      if (retries > 0) {
        Serial.printf("  ⚠️  Chunk %d tekrar deneniyor... (%d)\n", i, retries);
        delay(500);
      }
    }
    
    if (httpCode == 200) {
      int progress = ((i + 1) * 100) / totalChunks;
      Serial.printf("  ✓ [%3d%%] Chunk %2d/%2d (%5d bytes)\n", 
                    progress, i + 1, totalChunks, currentChunkSize);
    } else {
      Serial.printf("  ✗ Chunk %d BAŞARISIZ (HTTP: %d)\n", i, httpCode);
      success = false;
      break;
    }
    
    http.end();
    delay(5);
  }
  
  unsigned long elapsed = millis() - startTime;
  float speedKBps = (sizeKB * 1000.0) / elapsed;
  
  Serial.println();
  if (success) {
    Serial.println("╔════════════════════════════════╗");
    Serial.println("║   ✅ BAŞARILI!                ║");
    Serial.println("╚════════════════════════════════╝");
    Serial.printf("⏱️  Süre: %lu ms\n", elapsed);
    Serial.printf("🚀 Hız: %.1f KB/s\n", speedKBps);
  } else {
    Serial.println("╔════════════════════════════════╗");
    Serial.println("║   ❌ BAŞARISIZ!               ║");
    Serial.println("╚════════════════════════════════╝");
  } 
  
  esp_camera_fb_return(fb);
  
  return success;
}

// ============================================
// ANA DÖNGÜ
// ============================================
void loop() {
  // WiFi kontrolü
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️  WiFi bağlantısı kesildi, yeniden bağlanıyor...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("✅ WiFi yeniden bağlandı!");
    } else {
      Serial.println("❌ WiFi bağlanamadı, 10 saniye bekleniyor...");
      delay(10000);
      return;
    }
  }
  
  Serial.println("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  sendImageInChunks();
  Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
  
  delay(CAPTURE_INTERVAL);
}