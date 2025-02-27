#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <ESPAsyncWebServer.h>

// Camera pins for AI Thinker ESP32-CAM
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// PIR Sensor pin
#define PIR_PIN           16

// EEPROM size and addresses
#define EEPROM_SIZE       512
#define WIFI_SSID_ADDR    0
#define WIFI_PASS_ADDR    32
#define DEVICE_ID_ADDR    64

// Server details
const char* SERVER_URL = "https://well-scallop-cybergenii-075601d4.koyeb.app";
const char* UPLOAD_ENDPOINT = "/api/camera/upload";

// Global variables
String deviceId;
bool isConfigured = false;
AsyncWebServer server(80);

// Camera configuration
camera_config_t config;
// define fuunctions

void setupCamera();
void loadConfig();
void setupAP();
bool captureAndSendImage();

void setupCamera() {
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

  // Image quality settings
  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 10;  // 0-63, lower means higher quality
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
}

void loadConfig() {
  EEPROM.begin(EEPROM_SIZE);
  String ssid = "";
  String pass = "";
  deviceId = "";

  // Read SSID
  for (int i = 0; i < 32; i++) {
    char c = EEPROM.read(WIFI_SSID_ADDR + i);
    if (c == 0) break;
    ssid += c;
  }

  // Read password
  for (int i = 0; i < 32; i++) {
    char c = EEPROM.read(WIFI_PASS_ADDR + i);
    if (c == 0) break;
    pass += c;
  }

  // Read device ID
  for (int i = 0; i < 32; i++) {
    char c = EEPROM.read(DEVICE_ID_ADDR + i);
    if (c == 0) break;
    deviceId += c;
  }

  if (ssid.length() > 0 && pass.length() > 0) {
    WiFi.begin(ssid.c_str(), pass.c_str());
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      isConfigured = true;
      Serial.println("\nConnected to WiFi");
    }
  }
}

void setupAP() {
  String apName = "SmartCam-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  WiFi.softAP(apName.c_str());
  
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    String html = "<html><body>";
    html += "<h1>Smart Camera Setup</h1>";
    html += "<form action='/setup' method='post'>";
    html += "WiFi SSID: <input type='text' name='ssid'><br>";
    html += "Password: <input type='password' name='pass'><br>";
    html += "<input type='submit' value='Save'>";
    html += "</form></body></html>";
    request->send(200, "text/html", html);
  });

  server.on("/setup", HTTP_POST, [](AsyncWebServerRequest *request){
    String ssid = request->arg("ssid");
    String pass = request->arg("pass");
    
    // Generate device ID
    deviceId = "CAM-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    
    // Save to EEPROM
    EEPROM.begin(EEPROM_SIZE);
    
    // Write SSID
    for (int i = 0; i < ssid.length(); i++) {
      EEPROM.write(WIFI_SSID_ADDR + i, ssid[i]);
    }
    EEPROM.write(WIFI_SSID_ADDR + ssid.length(), 0);
    
    // Write password
    for (int i = 0; i < pass.length(); i++) {
      EEPROM.write(WIFI_PASS_ADDR + i, pass[i]);
    }
    EEPROM.write(WIFI_PASS_ADDR + pass.length(), 0);
    
    // Write device ID
    for (int i = 0; i < deviceId.length(); i++) {
      EEPROM.write(DEVICE_ID_ADDR + i, deviceId[i]);
    }
    EEPROM.write(DEVICE_ID_ADDR + deviceId.length(), 0);
    
    EEPROM.commit();
    
    request->send(200, "text/plain", "Settings saved. Device will restart.");
    delay(2000);
    ESP.restart();
  });

  server.begin();
}

bool captureAndSendImage() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return false;
  }

  HTTPClient http;
  http.begin(String(SERVER_URL) + String(UPLOAD_ENDPOINT));
  http.addHeader("Content-Type", "multipart/form-data");
  http.addHeader("X-Device-ID", deviceId);

  int httpResponseCode = http.POST(fb->buf, fb->len);
  esp_camera_fb_return(fb);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Image uploaded successfully");
    return true;
  } else {
    Serial.println("Image upload failed");
    return false;
  }
}

void setup() {
  Serial.begin(115200);
  
  // Initialize PIR sensor
  pinMode(PIR_PIN, INPUT);
  
  // Initialize camera
  setupCamera();
  
  // Load saved configuration
  loadConfig();
  
  // If not configured, start AP mode
  if (!isConfigured) {
    setupAP();
  }
}

void loop() {
  if (!isConfigured) {
    // In setup mode, handle web server
    delay(100);
    return;
  }

  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost. Attempting to reconnect...");
    loadConfig();
    delay(5000);
    return;
  }

  // Check PIR sensor
  if (digitalRead(PIR_PIN) == HIGH) {
    Serial.println("Motion detected!");
    if (captureAndSendImage()) {
      // Wait a bit before checking for motion again
      delay(5000);
    }
  }

  delay(100);  // Small delay to prevent too frequent checks
}