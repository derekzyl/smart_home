#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <ESP8266mDNS.h>
#include <WebSocketsServer.h>
#include <ESP8266WiFiMulti.h>

// Constants
#define RELAY_PIN 2
#define EEPROM_SIZE 512
#define CONFIG_MAGIC_BYTE 0x42  // Magic byte to validate configuration

// EEPROM address offsets
#define ADDR_MAGIC_BYTE 0
#define ADDR_IS_CONFIGURED 1
#define ADDR_DEVICE_NAME 2
#define ADDR_HOME_WIFI_SSID 34
#define ADDR_HOME_WIFI_PASSWORD 66
#define ADDR_HUB_HOTSPOT_SSID 130
#define ADDR_HUB_HOTSPOT_PASSWORD 162
#define ADDR_DEVICE_STATE 194

// Global objects
ESP8266WebServer server(80);
WebSocketsServer webSocket(81);  // For real-time communication with app
ESP8266WiFiMulti wifiMulti;
WiFiClient client;

// Configuration variables
char deviceId[16] = "";  // Will be set to ESP.getChipId() in setup
char deviceType[16] = "switch";
char deviceName[32] = "Smart Switch";
char homeWifiSSID[32] = "";
char homeWifiPassword[64] = "";
char hubHotspotSSID[32] = "";
char hubHotspotPassword[32] = "";
bool isConfigured = false;

// Device state
bool deviceState = false;

// define allthe functions

void notifyClients();
void sendStatusToHub();
void checkHubCommands();
void loadConfiguration();
void saveConfiguration();
void saveDeviceState();
void restoreDeviceState();
void updateRelayState();
void setupHotspot();
void connectToWiFi();
void setupWebServer();
void setupWebSocket();
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
void sendStateToClient(uint8_t num);
void handleRoot();
void handleSetup();
void handleNotFound();
void handleHubCheck();
void handleHubCommand();
void handleApiSetup();
void handleApiState();
void handleApiScan();
void handleControl();
void handleApiInfo();
void handleApiSetup();
void handleApiControl();
void handleApiScan();
// Timers
unsigned long lastHubCheckTime = 0;
const long hubCheckInterval = 5000;

void setup() {
  Serial.begin(115200);
  delay(100);
  
  // Initialize pins
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  
  // Generate device ID from chip ID
  sprintf(deviceId, "%08X", ESP.getChipId());
  
  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);
  loadConfiguration();
  
  if (isConfigured) {
    connectToWiFi();
  } else {
    setupHotspot();
  }
  
  // Set up web server routes
  setupWebServer();
  
  // Set up WebSocket server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  
  // Set up mDNS for easy device discovery
  if (MDNS.begin(deviceId)) {
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("ws", "tcp", 81);
    Serial.println("mDNS responder started");
  }
  
  // Restore device state from EEPROM
  restoreDeviceState();
  
  Serial.println("Device setup complete");
  Serial.print("Device ID: ");
  Serial.println(deviceId);
}

void loop() {
  server.handleClient();
  webSocket.loop();
  MDNS.update();
  
  // If configured and connected to WiFi, check for commands from hub
  if (isConfigured && WiFi.status() == WL_CONNECTED && WiFi.getMode() == WIFI_STA) {
    if (millis() - lastHubCheckTime > hubCheckInterval) {
      sendStatusToHub();
      checkHubCommands();
      lastHubCheckTime = millis();
    }
  }
}

void loadConfiguration() {
  if (EEPROM.read(ADDR_MAGIC_BYTE) == CONFIG_MAGIC_BYTE) {
    isConfigured = EEPROM.read(ADDR_IS_CONFIGURED);
    
    if (isConfigured) {
      // Read device name
      for (int i = 0; i < 32; i++) {
        deviceName[i] = EEPROM.read(ADDR_DEVICE_NAME + i);
      }
      
      // Read home WiFi SSID
      for (int i = 0; i < 32; i++) {
        homeWifiSSID[i] = EEPROM.read(ADDR_HOME_WIFI_SSID + i);
      }
      
      // Read home WiFi password
      for (int i = 0; i < 64; i++) {
        homeWifiPassword[i] = EEPROM.read(ADDR_HOME_WIFI_PASSWORD + i);
      }
      
      // Read hub hotspot SSID
      for (int i = 0; i < 32; i++) {
        hubHotspotSSID[i] = EEPROM.read(ADDR_HUB_HOTSPOT_SSID + i);
      }
      
      // Read hub hotspot password
      for (int i = 0; i < 32; i++) {
        hubHotspotPassword[i] = EEPROM.read(ADDR_HUB_HOTSPOT_PASSWORD + i);
      }
      
      Serial.println("Configuration loaded from EEPROM");
      Serial.print("Device name: ");
      Serial.println(deviceName);
      Serial.print("Home WiFi SSID: ");
      Serial.println(homeWifiSSID);
      Serial.print("Hub Hotspot SSID: ");
      Serial.println(hubHotspotSSID);
    }
  } else {
    isConfigured = false;
    Serial.println("No valid configuration found in EEPROM");
  }
}

void saveConfiguration() {
  // Write magic byte to validate configuration
  EEPROM.write(ADDR_MAGIC_BYTE, CONFIG_MAGIC_BYTE);
  EEPROM.write(ADDR_IS_CONFIGURED, isConfigured ? 1 : 0);
  
  // Write device name
  for (int i = 0; i < 32; i++) {
    EEPROM.write(ADDR_DEVICE_NAME + i, deviceName[i]);
  }
  
  // Write home WiFi SSID
  for (int i = 0; i < 32; i++) {
    EEPROM.write(ADDR_HOME_WIFI_SSID + i, homeWifiSSID[i]);
  }
  
  // Write home WiFi password
  for (int i = 0; i < 64; i++) {
    EEPROM.write(ADDR_HOME_WIFI_PASSWORD + i, homeWifiPassword[i]);
  }
  
  // Write hub hotspot SSID
  for (int i = 0; i < 32; i++) {
    EEPROM.write(ADDR_HUB_HOTSPOT_SSID + i, hubHotspotSSID[i]);
  }
  
  // Write hub hotspot password
  for (int i = 0; i < 32; i++) {
    EEPROM.write(ADDR_HUB_HOTSPOT_PASSWORD + i, hubHotspotPassword[i]);
  }
  
  EEPROM.commit();
  Serial.println("Configuration saved to EEPROM");
}

void saveDeviceState() {
  EEPROM.write(ADDR_DEVICE_STATE, deviceState ? 1 : 0);
  EEPROM.commit();
  Serial.println("Device state saved to EEPROM");
}

void restoreDeviceState() {
  if (EEPROM.read(ADDR_MAGIC_BYTE) == CONFIG_MAGIC_BYTE) {
    deviceState = EEPROM.read(ADDR_DEVICE_STATE) == 1;
    updateRelayState();
    Serial.print("Device state restored from EEPROM: ");
    Serial.println(deviceState ? "ON" : "OFF");
  }
}

void updateRelayState() {
  digitalWrite(RELAY_PIN, deviceState ? HIGH : LOW);
}

void setupHotspot() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(deviceId, deviceId);  // Use device ID as both SSID and password
  Serial.print("Hotspot created: ");
  Serial.println(deviceId);
  Serial.print("IP address: ");
  Serial.println(WiFi.softAPIP());
}

void connectToWiFi() {
  Serial.println("Attempting to connect to configured networks");
  
  // First try to connect to home WiFi
  wifiMulti.addAP(homeWifiSSID, homeWifiPassword);
  
  // Then try to connect to hub's hotspot if configured
  if (strlen(hubHotspotSSID) > 0) {
    wifiMulti.addAP(hubHotspotSSID, hubHotspotPassword);
  }
  
  Serial.print("Connecting to WiFi");
  
  // Wait for connection to either network
  int attempts = 0;
  while (wifiMulti.run() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.print("Connected to: ");
    Serial.println(WiFi.SSID());
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    // If connected to home WiFi, register with hub when needed
    if (WiFi.SSID() == String(homeWifiSSID)) {
      Serial.println("Connected to home WiFi, looking for hub...");
    }
  } else {
    Serial.println("");
    Serial.println("Failed to connect to WiFi, reverting to hotspot mode");
    setupHotspot();
  }
}

void setupWebServer() {
  // Root page - shows setup form or device control
  server.on("/", HTTP_GET, handleRoot);
  
  // Handle setup form submission
  server.on("/setup", HTTP_POST, handleSetup);
  
  // Handle device control
  server.on("/control", HTTP_POST, handleControl);
  
  // API endpoint for device info
  server.on("/api/info", HTTP_GET, handleApiInfo);
  
  // API endpoint for device setup
  server.on("/api/setup", HTTP_POST, handleApiSetup);
  
  // API endpoint for device control
  server.on("/api/control", HTTP_POST, handleApiControl);
  
  // API endpoint for device scan (used by app to find this device)
  server.on("/api/scan", HTTP_GET, handleApiScan);
  
  server.begin();
  Serial.println("Web server started");
}

String getSetupHtml() {
  String html = "<html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<style>body{font-family:Arial;margin:20px;} .form-group{margin-bottom:15px;} input{padding:5px;width:100%;max-width:300px;} button{padding:8px 16px;background:#4CAF50;color:white;border:none;cursor:pointer;}</style>";
  html += "</head><body>";
  html += "<h1>Smart Switch Setup</h1>";
  
  if (!isConfigured) {
    html += "<form action='/setup' method='post'>";
    html += "<div class='form-group'>Home WiFi SSID: <input type='text' name='ssid'></div>";
    html += "<div class='form-group'>Home WiFi Password: <input type='password' name='password'></div>";
    html += "<div class='form-group'>Hub Hotspot SSID: <input type='text' name='hubssid' value='SmartHomeHub'></div>";
    html += "<div class='form-group'>Hub Hotspot Password: <input type='password' name='hubpass' value='hubpassword'></div>";
    html += "<div class='form-group'>Device Name (optional): <input type='text' name='name' value='" + String(deviceName) + "'></div>";
    html += "<button type='submit'>Configure</button>";
    html += "</form>";
    html += "<p>You can also use the SmartHome App to configure this device.</p>";
  } else {
    html += "<p>Device is configured</p>";
    html += "<p>Status: <strong>" + String(deviceState ? "ON" : "OFF") + "</strong></p>";
    html += "<form action='/control' method='post'>";
    html += "<button type='submit' name='action' value='" + String(deviceState ? "off" : "on") + "'>" + String(deviceState ? "Turn OFF" : "Turn ON") + "</button>";
    html += "</form>";
    html += "<p><a href='/setup?reset=1'>Reset Configuration</a></p>";
  }
  
  html += "</body></html>";
  return html;
}

void handleRoot() {
  // Check if reset configuration was requested
  if (server.hasArg("reset") && server.arg("reset") == "1") {
    isConfigured = false;
    saveConfiguration();
    server.send(200, "text/html", "<html><body><h1>Configuration reset</h1><p>The device will restart now.</p></body></html>");
    delay(3000);
    ESP.restart();
    return;
  }
  
  server.send(200, "text/html", getSetupHtml());
}

void handleSetup() {
  if (server.hasArg("ssid") && server.hasArg("password") && 
      server.hasArg("hubssid") && server.hasArg("hubpass")) {
    
    server.arg("ssid").toCharArray(homeWifiSSID, 32);
    server.arg("password").toCharArray(homeWifiPassword, 64);
    server.arg("hubssid").toCharArray(hubHotspotSSID, 32);
    server.arg("hubpass").toCharArray(hubHotspotPassword, 32);
    
    if (server.hasArg("name") && server.arg("name").length() > 0) {
      server.arg("name").toCharArray(deviceName, 32);
    }
    
    isConfigured = true;
    saveConfiguration();
    
    server.send(200, "text/html", "<html><body><h1>Configuration saved</h1><p>The device will restart now.</p></body></html>");
    delay(3000);
    ESP.restart();
  } else {
    server.send(400, "text/html", "<html><body><h1>Bad request</h1><p>Missing parameters.</p></body></html>");
  }
}

void handleControl() {
  if (server.hasArg("action")) {
    String action = server.arg("action");
    
    if (action == "on") {
      deviceState = true;
      updateRelayState();
      saveDeviceState();
      server.send(200, "text/html", "<html><body><h1>Device turned ON</h1><a href='/'>Back</a></body></html>");
      notifyClients();
    } else if (action == "off") {
      deviceState = false;
      updateRelayState();
      saveDeviceState();
      server.send(200, "text/html", "<html><body><h1>Device turned OFF</h1><a href='/'>Back</a></body></html>");
      notifyClients();
    } else {
      server.send(400, "text/html", "<html><body><h1>Invalid action</h1><a href='/'>Back</a></body></html>");
    }
  } else {
    server.send(400, "text/html", "<html><body><h1>Bad request</h1><p>Missing action parameter.</p><a href='/'>Back</a></body></html>");
  }
}

void handleApiInfo() {
  DynamicJsonDocument doc(512);
  doc["id"] = deviceId;
  doc["type"] = deviceType;
  doc["name"] = deviceName;
  doc["configured"] = isConfigured;
  doc["state"] = deviceState;
  doc["ip"] = WiFi.localIP().toString();
  doc["ap_ip"] = WiFi.softAPIP().toString();
  doc["mac"] = WiFi.macAddress();
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleApiSetup() {
  if (server.hasArg("plain")) {
    String body = server.arg("plain");
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, body);
    
    if (!error) {
      if (doc.containsKey("ssid") && doc.containsKey("password") && 
          doc.containsKey("hubssid") && doc.containsKey("hubpass")) {
        
        strlcpy(homeWifiSSID, doc["ssid"], sizeof(homeWifiSSID));
        strlcpy(homeWifiPassword, doc["password"], sizeof(homeWifiPassword));
        strlcpy(hubHotspotSSID, doc["hubssid"], sizeof(hubHotspotSSID));
        strlcpy(hubHotspotPassword, doc["hubpass"], sizeof(hubHotspotPassword));
        
        if (doc.containsKey("name")) {
          strlcpy(deviceName, doc["name"], sizeof(deviceName));
        }
        
        isConfigured = true;
        saveConfiguration();
        
        // Respond with success
        DynamicJsonDocument responseDoc(128);
        responseDoc["success"] = true;
        responseDoc["message"] = "Configuration saved";
        
        String response;
        serializeJson(responseDoc, response);
        
        server.send(200, "application/json", response);
        
        // Schedule restart after sending response
        delay(2000);
        ESP.restart();
      } else {
        server.send(400, "application/json", "{\"success\":false,\"message\":\"Missing parameters\"}");
      }
    } else {
      server.send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
    }
  } else {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"No data provided\"}");
  }
}

void handleApiControl() {
  if (server.hasArg("plain")) {
    String body = server.arg("plain");
    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, body);
    
    if (!error) {
      if (doc.containsKey("action")) {
        String action = doc["action"];
        
        if (action == "on") {
          deviceState = true;
          updateRelayState();
          saveDeviceState();
          server.send(200, "application/json", "{\"success\":true,\"state\":true}");
          notifyClients();
        } else if (action == "off") {
          deviceState = false;
          updateRelayState();
          saveDeviceState();
          server.send(200, "application/json", "{\"success\":false,\"state\":false}");
          notifyClients();
        } else {
          server.send(400, "application/json", "{\"success\":false,\"message\":\"Invalid action\"}");
        }
      } else {
        server.send(400, "application/json", "{\"success\":false,\"message\":\"Missing action parameter\"}");
      }
    } else {
      server.send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
    }
  } else {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"No data provided\"}");
  }
}

void handleApiScan() {
  DynamicJsonDocument doc(512);
  doc["id"] = deviceId;
  doc["type"] = deviceType;
  doc["name"] = deviceName;
  doc["configured"] = isConfigured;
  doc["ip"] = WiFi.localIP().toString();
  doc["ap_ip"] = WiFi.softAPIP().toString();
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);
        
        // Send current state to newly connected client
        sendStateToClient(num);
      }
      break;
    case WStype_TEXT:
      {
        Serial.printf("[%u] Received text: %s\n", num, payload);
        
        DynamicJsonDocument doc(256);
        DeserializationError error = deserializeJson(doc, payload, length);
        
        if (!error) {
          if (doc.containsKey("action")) {
            String action = doc["action"];
            
            if (action == "on") {
              deviceState = true;
              updateRelayState();
              saveDeviceState();
              notifyClients();
            } else if (action == "off") {
              deviceState = false;
              updateRelayState();
              saveDeviceState();
              notifyClients();
            } else if (action == "get_state") {
              sendStateToClient(num);
            }
          }
        }
      }
      break;
  }
}

void sendStateToClient(uint8_t num) {
  DynamicJsonDocument doc(256);
  doc["type"] = "state";
  doc["state"] = deviceState;
  doc["id"] = deviceId;
  doc["device_type"] = deviceType;
  doc["name"] = deviceName;
  
  String response;
  serializeJson(doc, response);
  webSocket.sendTXT(num, response);
}

void notifyClients() {
  DynamicJsonDocument doc(256);
  doc["type"] = "state";
  doc["state"] = deviceState;
  doc["id"] = deviceId;
  doc["device_type"] = deviceType;
  doc["name"] = deviceName;
  
  String response;
  serializeJson(doc, response);
  webSocket.broadcastTXT(response);
}

void sendStatusToHub() {
  if (WiFi.SSID() == String(hubHotspotSSID)) {
    // We're connected to the hub's hotspot - send using local IP
    HTTPClient http;
    WiFiClient client;
    
    // Get hub's gateway IP (the hub itself)
    IPAddress gateway = WiFi.gatewayIP();
    String hubUrl = "http://" + gateway.toString() + "/api/device/status";
    
    http.begin(client, hubUrl);
    http.addHeader("Content-Type", "application/json");
    
    DynamicJsonDocument doc(256);
    doc["id"] = deviceId;
    doc["type"] = deviceType;
    doc["name"] = deviceName;
    doc["state"] = deviceState;
    doc["ip"] = WiFi.localIP().toString();
    
    String jsonStr;
    serializeJson(doc, jsonStr);
    
    int httpCode = http.POST(jsonStr);
    if (httpCode == HTTP_CODE_OK) {
      Serial.println("Status sent to hub successfully");
    } else {
      Serial.printf("Error sending status to hub: %d\n", httpCode);
    }
    
    http.end();
  }
}

void checkHubCommands() {
  if (WiFi.SSID() == String(hubHotspotSSID)) {
    // We're connected to the hub's hotspot
    HTTPClient http;
    WiFiClient client;
    
    // Get hub's gateway IP (the hub itself)
    IPAddress gateway = WiFi.gatewayIP();
    String hubUrl = "http://" + gateway.toString() + "/api/device/commands?id=" + String(deviceId);
    
    http.begin(client, hubUrl);
    
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      String response = http.getString();
      DynamicJsonDocument doc(512);
      DeserializationError error = deserializeJson(doc, response);
      
      if (!error && doc.containsKey("action")) {
        String action = doc["action"];
        
        if (action == "on" && !deviceState) {
          deviceState = true;
          updateRelayState();
          saveDeviceState();
          Serial.println("Turned ON via hub command");
          notifyClients();
        } else if (action == "off" && deviceState) {
          deviceState = false;
          updateRelayState();
          saveDeviceState();
          Serial.println("Turned OFF via hub command");
          notifyClients();
        }
      }
    }
    
    http.end();
  }
}