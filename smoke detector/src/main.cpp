#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

// Pin definitions
#define SMOKE_SENSOR_PIN 0  // GPIO0 for smoke sensor

// Constants
#define EEPROM_SIZE 512
#define AP_PREFIX "SmartSmoke_"

// Global variables
String deviceId;
String ssid = "";
String password = "";
String hubSsid = "";
String hubPassword = "";
bool isConfigured = false;
float smokeLevel = 0;
bool alarmTriggered = false;
unsigned long lastReadingTime = 0;
unsigned long lastHeartbeatTime = 0;

// Objects
ESP8266WebServer server(80);
WebSocketsClient webSocket;

// Function prototypes
void setupAP();
void handleRoot();
void handleSetup();
void connectToHub();
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length);
void sendSensorData();
void readSensor();
void saveConfiguration();
void loadConfiguration();
String generateUniqueId();

void setup() {
  Serial.begin(115200);
  
  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);
  
  // Set pin modes
  pinMode(SMOKE_SENSOR_PIN, INPUT);
  
  // Generate or load device ID
  deviceId = generateUniqueId();
  
  // Load configuration if available
  loadConfiguration();
  
  if (!isConfigured) {
    setupAP();
    server.on("/", HTTP_GET, handleRoot);
    server.on("/setup", HTTP_GET, handleSetup);
    server.begin();
    Serial.println("HTTP server started in AP mode");
  } else {
    connectToHub();
  }
}

void loop() {
  if (!isConfigured) {
    server.handleClient();
  } else {
    webSocket.loop();
    
    // Read sensor periodically
    if (millis() - lastReadingTime > 2000) {  // every 2 seconds
      readSensor();
      lastReadingTime = millis();
    }
    
    // Send heartbeat periodically
    if (millis() - lastHeartbeatTime > 30000) {  // every 30 seconds
      DynamicJsonDocument doc(128);
      doc["type"] = "heartbeat";
      doc["deviceId"] = deviceId;
      
      String jsonString;
      serializeJson(doc, jsonString);
      webSocket.sendTXT(jsonString);
      lastHeartbeatTime = millis();
    }
    
    // Reconnect to hub if connection lost
    if (!webSocket.isConnected()) {
      connectToHub();
    }
  }
}

void setupAP() {
  String apName = AP_PREFIX + deviceId.substring(0, 6);
  WiFi.softAP(apName.c_str(), deviceId.c_str());
  Serial.println("Access Point Started");
  Serial.print("SSID: ");
  Serial.println(apName);
  Serial.print("Password: ");
  Serial.println(deviceId);
}

void handleRoot() {
  String html = "<!DOCTYPE html><html>";
  html += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<style>body{font-family:Arial;margin:20px}";
  html += "input,button{margin:10px 0;padding:8px;width:100%}</style></head>";
  html += "<body><h1>Smoke Sensor Setup</h1>";
  html += "<form action='/setup'>";
  html += "Hub SSID:<br><input name='hubssid' required><br>";
  html += "Hub Password:<br><input name='hubpass' type='password' required><br>";
  html += "<button type='submit'>Save</button></form></body></html>";
  server.send(200, "text/html", html);
}

void handleSetup() {
  if (server.hasArg("hubssid") && server.hasArg("hubpass")) {
    hubSsid = server.arg("hubssid");
    hubPassword = server.arg("hubpass");
    isConfigured = true;
    saveConfiguration();
    
    String html = "<!DOCTYPE html><html>";
    html += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    html += "<style>body{font-family:Arial;margin:20px}</style></head>";
    html += "<body><h1>Setup Complete</h1>";
    html += "<p>Device will now connect to the hub.</p></body></html>";
    server.send(200, "text/html", html);
    
    delay(2000);
    connectToHub();
  }
}

void connectToHub() {
  WiFi.begin(hubSsid.c_str(), hubPassword.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to hub's network");
    
    // Connect to hub's WebSocket server
    webSocket.begin(WiFi.gatewayIP().toString(), 81, "/ws");
    webSocket.onEvent(webSocketEvent);
    webSocket.setReconnectInterval(5000);
    
    // Send registration message
    DynamicJsonDocument doc(256);
    doc["type"] = "registration";
    doc["deviceId"] = deviceId;
    doc["deviceType"] = "smoke_sensor";
    
    String jsonString;
    serializeJson(doc, jsonString);
    webSocket.sendTXT(jsonString);
  }
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("Disconnected from hub");
      break;
      
    case WStype_CONNECTED:
      Serial.println("Connected to hub");
      break;
      
    case WStype_TEXT: {
      String message = String((char*)payload);
      DynamicJsonDocument doc(512);
      DeserializationError error = deserializeJson(doc, message);
      
      if (!error) {
        String msgType = doc["type"];
        if (msgType == "command") {
          String command = doc["command"];
          if (command == "read_sensor") {
            sendSensorData();
          }
        }
      }
      break;
    }
  }
}

void readSensor() {
  // Read analog value from smoke sensor
  smokeLevel = analogRead(SMOKE_SENSOR_PIN);
  
  // Check if smoke level exceeds threshold
  if (smokeLevel > 500 && !alarmTriggered) {  // Adjust threshold as needed
    alarmTriggered = true;
    
    // Send alert to hub
    DynamicJsonDocument doc(256);
    doc["type"] = "alert";
    doc["deviceId"] = deviceId;
    doc["alertType"] = "smoke_detected";
    doc["value"] = smokeLevel;
    
    String jsonString;
    serializeJson(doc, jsonString);
    webSocket.sendTXT(jsonString);
  } else if (smokeLevel <= 500 && alarmTriggered) {
    alarmTriggered = false;
  }
}

void sendSensorData() {
  DynamicJsonDocument doc(256);
  doc["type"] = "status";
  doc["deviceId"] = deviceId;
  doc["status"] = String(smokeLevel);
  
  String jsonString;
  serializeJson(doc, jsonString);
  webSocket.sendTXT(jsonString);
}

void saveConfiguration() {
  int addr = 0;
  
  // Save isConfigured flag
  EEPROM.write(addr, isConfigured ? 1 : 0);
  addr += 1;
  
  // Save hub SSID
  EEPROM.write(addr, hubSsid.length());
  addr += 1;
  for (int i = 0; i < hubSsid.length(); i++) {
    EEPROM.write(addr + i, hubSsid[i]);
  }
  addr += hubSsid.length();
  
  // Save hub password
  EEPROM.write(addr, hubPassword.length());
  addr += 1;
  for (int i = 0; i < hubPassword.length(); i++) {
    EEPROM.write(addr + i, hubPassword[i]);
  }
  
  EEPROM.commit();
}

void loadConfiguration() {
  int addr = 0;
  
  // Load isConfigured flag
  isConfigured = EEPROM.read(addr) == 1;
  addr += 1;
  
  if (isConfigured) {
    // Load hub SSID
    int ssidLen = EEPROM.read(addr);
    addr += 1;
    hubSsid = "";
    for (int i = 0; i < ssidLen; i++) {
      hubSsid += (char)EEPROM.read(addr + i);
    }
    addr += ssidLen;
    
    // Load hub password
    int passLen = EEPROM.read(addr);
    addr += 1;
    hubPassword = "";
    for (int i = 0; i < passLen; i++) {
      hubPassword += (char)EEPROM.read(addr + i);
    }
  }
}

String generateUniqueId() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char id[13];
  sprintf(id, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(id);
}