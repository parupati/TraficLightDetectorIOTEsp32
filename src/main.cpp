#include <Arduino.h>
#include <WiFi.h>
#include <Adafruit_NeoPixel.h>
#include "esp_camera.h"
#include "camera_pins.h"
#include "color_detect.h"

// ===== WIFI CREDENTIALS =====
const char *ssid     = "YOUR_WIFI_SSID";
const char *password = "YOUR_WIFI_PASSWORD";
// =============================

// Onboard WS2812 LED
#define LED_PIN    48
#define NUM_LEDS   1
#define BRIGHTNESS 40
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Shared detection state (read by app_httpd.cpp)
extern volatile int g_detected;
extern volatile int g_red_count;
extern volatile int g_green_count;
extern volatile int g_total_pixels;

void startCameraServer();

// Camera for JPEG streaming uses one config.
// For detection we temporarily switch to RGB565, grab a frame, then switch back.
static bool camera_initialized = false;

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println("\n\nSignal Light Detector Starting...");

  // Init NeoPixel
  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.setPixelColor(0, strip.Color(0, 0, 50)); // blue = starting
  strip.show();

  // Camera config - start in JPEG for streaming
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode    = CAMERA_GRAB_LATEST;
  config.fb_location  = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 10;
  config.fb_count     = 2;

  if (psramFound()) {
    Serial.printf("PSRAM found: %d bytes\n", ESP.getPsramSize());
    config.frame_size = FRAMESIZE_SVGA;
  } else {
    Serial.println("WARNING: No PSRAM. Using lower resolution.");
    config.frame_size  = FRAMESIZE_VGA;
    config.fb_count    = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init FAILED: 0x%x\n", err);
    strip.setPixelColor(0, strip.Color(255, 0, 0));
    strip.show();
    return;
  }
  camera_initialized = true;
  Serial.println("Camera initialized");

  sensor_t *s = esp_camera_sensor_get();
  s->set_vflip(s, 0);
  s->set_brightness(s, 1);
  s->set_saturation(s, 0);

  // Connect WiFi
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  Serial.printf("Connecting to '%s'", ssid);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWiFi FAILED.");
    strip.setPixelColor(0, strip.Color(255, 80, 0)); // orange = no wifi
    strip.show();
    return;
  }

  Serial.printf("\nWiFi connected! IP: %s\n", WiFi.localIP().toString().c_str());

  startCameraServer();

  Serial.println("==========================================");
  Serial.printf("  Open: http://%s\n", WiFi.localIP().toString().c_str());
  Serial.println("  Signal light detection active");
  Serial.println("==========================================");

  // Signal ready - flash green
  strip.setPixelColor(0, strip.Color(0, 50, 0));
  strip.show();
}

void loop() {
  if (!camera_initialized) {
    delay(1000);
    return;
  }

  // Switch sensor to RGB565 for color analysis at low resolution
  sensor_t *s = esp_camera_sensor_get();
  s->set_pixformat(s, PIXFORMAT_RGB565);
  s->set_framesize(s, FRAMESIZE_QVGA); // 320x240 for fast processing

  // Small delay for sensor to adjust
  delay(50);

  camera_fb_t *fb = esp_camera_fb_get();
  if (fb && fb->format == PIXFORMAT_RGB565) {
    detect_result_t result = detect_signal_color(
        (uint16_t *)fb->buf, fb->width, fb->height, 2); // sample every 2nd pixel

    // Update shared state for web UI
    g_detected     = result.detected;
    g_red_count    = result.red_count;
    g_green_count  = result.green_count;
    g_total_pixels = result.total_pixels;

    // Update NeoPixel
    if (result.detected == 1) {
      strip.setPixelColor(0, strip.Color(255, 0, 0)); // RED
      Serial.printf("RED signal | r=%d g=%d total=%d\n",
                     result.red_count, result.green_count, result.total_pixels);
    } else if (result.detected == 2) {
      strip.setPixelColor(0, strip.Color(0, 255, 0)); // GREEN
      Serial.printf("GREEN signal | r=%d g=%d total=%d\n",
                     result.red_count, result.green_count, result.total_pixels);
    } else {
      strip.setPixelColor(0, strip.Color(0, 0, 30));  // dim blue = no signal
    }
    strip.show();
  }

  if (fb) esp_camera_fb_return(fb);

  // Switch back to JPEG for streaming
  s->set_pixformat(s, PIXFORMAT_JPEG);
  s->set_framesize(s, FRAMESIZE_SVGA);

  delay(300); // ~3 detections per second
}