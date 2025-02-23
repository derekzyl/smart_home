/*
 * Smart Home Hub - Central Control System
 * Platform: ESP32
 * 
 * This hub manages communication between:
 * 1. Sub-devices (via local hotspot)
 * 2. Cloud server (via internet WiFi)
 * 3. Local interfaces (LCD, buttons, sensors)
 */

 #include <WiFi.h>
 #include <WiFiAP.h>
 #include <WebSocketsClient.h>
 #include <ArduinoJson.h>
 #include <DHT.h>
 #include <Wire.h>
 #include <LiquidCrystal_I2C.h>
 #include <EEPROM.h>
 #include <HTTPClient.h>
 #include <ESPAsyncWebServer.h>
 #include <AsyncWebSocket.h>

 
 // Pin definitions
 #define DHT_PIN 4        // DHT11 sensor connected to D4
 #define BUTTON1_PIN 26   // Button 1 connected to D26
 #define BUTTON2_PIN 27   // Button 2 connected to D27
 #define BUTTON3_PIN 25   // Button 3 connected to D25
 #define ALARM_PIN 23     // Alarm connected to D23
 
 // Constants
 #define EEPROM_SIZE 512
 #define AP_SSID_PREFIX "SmartHome_Hub_"
 #define AP_PASSWORD "12345678"  // Default password, will be changed during setup
 #define MAX_DEVICES 10
 #define LCD_COLS 16
 #define LCD_ROWS 4
 #define LCD_ADDR 0x27  // I2C address for LCD (may vary)
 #define WS_PORT 81     // Local WebSocket port for sub-devices
 
 // Global variables
 String internetSSID = "";
 String internetPassword = "";
 String username = "";
 String password = "";
 String uniqueId = "";
 bool isConfigured = false;
 bool alarmState = false;
 float temperature = 0;
 float humidity = 0;
 unsigned long lastTempReadTime = 0;
 unsigned long lastHeartbeatTime = 0;
 String connectedDevices[MAX_DEVICES];
 String deviceTypes[MAX_DEVICES];
 String deviceStatus[MAX_DEVICES];
 IPAddress deviceIPs[MAX_DEVICES];
 int numConnectedDevices = 0;
 
 // Initialize objects
 DHT dht(DHT_PIN, DHT11);
 LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);
 WebSocketsClient webSocket;          // Client for cloud server
 AsyncWebServer server(80);           // HTTP server for setup
 AsyncWebSocket ws("/ws");            // WebSocket server for sub-devices
 
 // Function prototypes
 void setupAP();
 void handleSetup();
 void sendAuthMessage();
 void connectToInternet();
 void connectToWebSocketServer();
 void webSocketEvent(WStype_t type, uint8_t * payload, size_t length);
 void processSubDeviceMessage(String message);
 void processServerMessage(String message);
 void sendHeartbeat();
 void updateLCD();
 void checkButtons();
 void readSensors();
 void triggerAlarm(bool state);
 void saveConfiguration();
 void loadConfiguration();
 void handleNewDevice(String deviceId, String deviceType, IPAddress ipAddress);
 String generateUniqueId();
 void handleWebSocketMessage(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
 void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
 void forwardCommandToDevice(String deviceId, String command);
 void confirmDeviceRegistration(String deviceId);
 void notifyServerNewDevice(String deviceId, String deviceType);
 void updateDeviceStatus(String deviceId, String status);
 void handleDeviceAlert(String deviceId, String alertType);
 void sendStatusUpdate();
 
 void setup() {
   // Initialize serial for debugging
   Serial.begin(115200);
   Serial.println("Smart Home Hub starting...");
   
   // Initialize EEPROM
   EEPROM.begin(EEPROM_SIZE);
   
   // Initialize pins
   pinMode(BUTTON1_PIN, INPUT_PULLUP);
   pinMode(BUTTON2_PIN, INPUT_PULLUP);
   pinMode(BUTTON3_PIN, INPUT_PULLUP);
   pinMode(ALARM_PIN, OUTPUT);
   digitalWrite(ALARM_PIN, LOW);
   
   // Initialize DHT sensor
   dht.begin();
   
   // Initialize LCD
   Wire.begin();
   lcd.init();
   lcd.backlight();
   lcd.clear();
   lcd.setCursor(0, 0);
   lcd.print("Smart Home Hub");
   lcd.setCursor(0, 1);
   lcd.print("Initializing...");
   
   // Load configuration if available
   loadConfiguration();
   
   if (!isConfigured) {
     // First time setup - generate unique ID and start AP mode
     uniqueId = generateUniqueId();
     setupAP();
     server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
       String html = "<!DOCTYPE html><html>";
       html += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
       html += "<style>body { font-family: Arial; margin: 20px; }";
       html += "input, button { margin: 10px 0; padding: 8px; width: 100%; }";
       html += "h1 { color: #0066cc; }</style></head>";
       html += "<body><h1>Smart Home Hub Setup</h1>";
       html += "<form action=\"/setup\" method=\"get\">";
       html += "<label for=\"ssid\">WiFi SSID:</label><br>";
       html += "<input type=\"text\" id=\"ssid\" name=\"ssid\" required><br>";
       html += "<label for=\"pass\">WiFi Password:</label><br>";
       html += "<input type=\"password\" id=\"pass\" name=\"pass\" required><br>";
       html += "<label for=\"user\">Dashboard Username:</label><br>";
       html += "<input type=\"text\" id=\"user\" name=\"user\" required><br>";
       html += "<label for=\"pwd\">Dashboard Password:</label><br>";
       html += "<input type=\"password\" id=\"pwd\" name=\"pwd\" required><br>";
       html += "<button type=\"submit\">Save Configuration</button>";
       html += "</form></body></html>";
       request->send(200, "text/html", html);
     });
     
     server.on("/setup", HTTP_GET, [](AsyncWebServerRequest *request){
       if (request->hasParam("ssid") && request->hasParam("pass") && 
           request->hasParam("user") && request->hasParam("pwd")) {
         internetSSID = request->getParam("ssid")->value();
         internetPassword = request->getParam("pass")->value();
         username = request->getParam("user")->value();
         password = request->getParam("pwd")->value();
         
         isConfigured = true;
         saveConfiguration();
         
         String html = "<!DOCTYPE html><html>";
         html += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
         html += "<style>body { font-family: Arial; margin: 20px; }";
         html += "h1 { color: #0066cc; } .success { color: green; }</style></head>";
         html += "<body><h1>Setup Complete</h1>";
         html += "<p class=\"success\">Configuration saved successfully!</p>";
         html += "<p>The hub will now connect to your WiFi network.</p>";
         html += "<p>You can close this page.</p></body></html>";
         request->send(200, "text/html", html);
         
         // Connect to WiFi after a short delay
         xTaskCreate([](void* parameter) {
           delay(3000);
           connectToInternet();
           connectToWebSocketServer();
           vTaskDelete(NULL);
         }, "connectTask", 4096, NULL, 1, NULL);
       } else {
         request->send(400, "text/plain", "Missing parameters");
       }
     });
     
     server.begin();
   } else {
     // Normal operation - connect to WiFi and server
     connectToInternet();
     connectToWebSocketServer();
     setupAP();  // Also start AP for sub-devices
   }
   
   // Setup WebSocket server for sub-devices
   ws.onEvent(onEvent);
   server.addHandler(&ws);
   Serial.println("WebSocket server started for sub-devices");
 }
 
 void loop() {
   // Handle WebSocket client communication with cloud server
   webSocket.loop();
   
   // Clean inactive WebSocket clients (for sub-devices)
   ws.cleanupClients();
   
   // Read sensor data periodically
   unsigned long currentMillis = millis();
   if (currentMillis - lastTempReadTime > 5000) {  // every 5 seconds
     readSensors();
     lastTempReadTime = currentMillis;
   }
   
   // Send heartbeat to server periodically
   if (currentMillis - lastHeartbeatTime > 30000) {  // every 30 seconds
     sendHeartbeat();
     lastHeartbeatTime = currentMillis;
   }
   
   // Check buttons for local control
   checkButtons();
   
   // Update LCD display
   updateLCD();
   
   // Handle WiFi reconnection if needed
   if (isConfigured && WiFi.status() != WL_CONNECTED) {
     Serial.println("WiFi connection lost. Reconnecting...");
     connectToInternet();
     connectToWebSocketServer();
   }
 }
 
 String generateUniqueId() {
   // Generate a unique ID based on MAC address
   uint8_t mac[6];
   WiFi.macAddress(mac);
   String id = "";
   for (int i = 0; i < 6; i++) {
     char buf[3];
     sprintf(buf, "%02X", mac[i]);
     id += buf;
   }
   return id;
 }
 
 void setupAP() {
   String apSSID = AP_SSID_PREFIX + uniqueId.substring(0, 6);
   
   Serial.println("Setting up Access Point...");
   Serial.print("SSID: ");
   Serial.println(apSSID);
   
   // Start AP with unique SSID
   WiFi.softAP(apSSID.c_str(), uniqueId.c_str());
   
   IPAddress IP = WiFi.softAPIP();
   Serial.print("AP IP address: ");
   Serial.println(IP);
   
   lcd.clear();
   lcd.setCursor(0, 0);
   lcd.print("AP Mode Active");
   lcd.setCursor(0, 1);
   lcd.print("SSID: " + apSSID);
   lcd.setCursor(0, 2);
   lcd.print("Pass: " + uniqueId);
 }
 
 void saveConfiguration() {
   // Save configuration to EEPROM
   int addr = 0;
   
   // Save isConfigured flag
   EEPROM.write(addr, isConfigured ? 1 : 0);
   addr += 1;
   
   // Save WiFi SSID (length + data)
   EEPROM.write(addr, internetSSID.length());
   addr += 1;
   for (int i = 0; i < internetSSID.length(); i++) {
     EEPROM.write(addr + i, internetSSID[i]);
   }
   addr += internetSSID.length();
   
   // Save WiFi password (length + data)
   EEPROM.write(addr, internetPassword.length());
   addr += 1;
   for (int i = 0; i < internetPassword.length(); i++) {
     EEPROM.write(addr + i, internetPassword[i]);
   }
   addr += internetPassword.length();
   
   // Save username (length + data)
   EEPROM.write(addr, username.length());
   addr += 1;
   for (int i = 0; i < username.length(); i++) {
     EEPROM.write(addr + i, username[i]);
   }
   addr += username.length();
   
   // Save password (length + data)
   EEPROM.write(addr, password.length());
   addr += 1;
   for (int i = 0; i < password.length(); i++) {
     EEPROM.write(addr + i, password[i]);
   }
   addr += password.length();
   
   // Save uniqueId (length + data)
   EEPROM.write(addr, uniqueId.length());
   addr += 1;
   for (int i = 0; i < uniqueId.length(); i++) {
     EEPROM.write(addr + i, uniqueId[i]);
   }
   
   EEPROM.commit();
   Serial.println("Configuration saved to EEPROM");
 }
 
 void loadConfiguration() {
   // Load configuration from EEPROM
   int addr = 0;
   
   // Load isConfigured flag
   isConfigured = EEPROM.read(addr) == 1;
   addr += 1;
   
   if (isConfigured) {
     // Load WiFi SSID
     int ssidLength = EEPROM.read(addr);
     addr += 1;
     internetSSID = "";
     for (int i = 0; i < ssidLength; i++) {
       internetSSID += (char)EEPROM.read(addr + i);
     }
     addr += ssidLength;
     
     // Load WiFi password
     int passLength = EEPROM.read(addr);
     addr += 1;
     internetPassword = "";
     for (int i = 0; i < passLength; i++) {
       internetPassword += (char)EEPROM.read(addr + i);
     }
     addr += passLength;
     
     // Load username
     int userLength = EEPROM.read(addr);
     addr += 1;
     username = "";
     for (int i = 0; i < userLength; i++) {
       username += (char)EEPROM.read(addr + i);
     }
     addr += userLength;
     
     // Load password
     int pwdLength = EEPROM.read(addr);
     addr += 1;
     password = "";
     for (int i = 0; i < pwdLength; i++) {
       password += (char)EEPROM.read(addr + i);
     }
     addr += pwdLength;
     
     // Load uniqueId
     int idLength = EEPROM.read(addr);
     addr += 1;
     uniqueId = "";
     for (int i = 0; i < idLength; i++) {
       uniqueId += (char)EEPROM.read(addr + i);
     }
     
     Serial.println("Configuration loaded from EEPROM");
     Serial.print("SSID: ");
     Serial.println(internetSSID);
     Serial.print("UniqueID: ");
     Serial.println(uniqueId);
   } else {
     Serial.println("No configuration found in EEPROM");
     uniqueId = generateUniqueId();
   }
 }
 
 void connectToInternet() {
   if (internetSSID.length() > 0) {
     Serial.println("Connecting to WiFi network...");
     lcd.clear();
     lcd.setCursor(0, 0);
     lcd.print("Connecting to");
     lcd.setCursor(0, 1);
     lcd.print(internetSSID);
     
     WiFi.begin(internetSSID.c_str(), internetPassword.c_str());
     
     int attempts = 0;
     while (WiFi.status() != WL_CONNECTED && attempts < 20) {
       delay(500);
       Serial.print(".");
       lcd.setCursor(attempts % 16, 2);
       lcd.print(".");
       attempts++;
     }
     
     if (WiFi.status() == WL_CONNECTED) {
       Serial.println("");
       Serial.println("WiFi connected");
       Serial.print("IP address: ");
       Serial.println(WiFi.localIP());
       
       lcd.clear();
       lcd.setCursor(0, 0);
       lcd.print("WiFi Connected");
       lcd.setCursor(0, 1);
       lcd.print(WiFi.localIP());
     } else {
       Serial.println("");
       Serial.println("WiFi connection failed");
       
       lcd.clear();
       lcd.setCursor(0, 0);
       lcd.print("WiFi Failed");
       lcd.setCursor(0, 1);
       lcd.print("Check settings");
     }
   } else {
     Serial.println("No WiFi credentials available");
   }
 }
 
 void connectToWebSocketServer() {
   if (WiFi.status() == WL_CONNECTED) {
     // Connect to WebSocket server
     Serial.println("Connecting to WebSocket server...");
     
     // Set server address and port - replace with your actual FastAPI server details
     webSocket.begin("your-smart-home-server.com", 8080, "/ws/hub/" + uniqueId);
     
     // Set event handler
     webSocket.onEvent(webSocketEvent);
     
     // Retry interval (ms)
     webSocket.setReconnectInterval(5000);
     
     Serial.println("WebSocket connection established");
   }
 }
 
 void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
   switch(type) {
     case WStype_DISCONNECTED:
       Serial.println("WebSocket disconnected from server");
       break;
       
     case WStype_CONNECTED:
       Serial.println("WebSocket connected to server");
       // Send authentication message
       sendAuthMessage();
       break;
       
     case WStype_TEXT:
       Serial.printf("WebSocket received text from server: %s\n", payload);
       processServerMessage(String((char*)payload));
       break;
       
     case WStype_ERROR:
       Serial.println("WebSocket error with server connection");
       break;
       
     default:
       break;
   }
 }
 
 void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
   switch (type) {
     case WS_EVT_CONNECT:
       Serial.printf("WebSocket client #%u connected from IP %s\n", client->id(), client->remoteIP().toString().c_str());
       break;
       
     case WS_EVT_DISCONNECT:
       Serial.printf("WebSocket client #%u disconnected\n", client->id());
       break;
       
     case WS_EVT_DATA:
       handleWebSocketMessage(server, client, type, arg, data, len);
       break;
       
     case WS_EVT_PONG:
     case WS_EVT_ERROR:
       break;
   }
 }
 
 void handleWebSocketMessage(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
   AwsFrameInfo *info = (AwsFrameInfo*)arg;
   if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
     data[len] = 0;
     String message = String((char*)data);
     Serial.printf("Received message from sub-device: %s\n", message.c_str());
     processSubDeviceMessage(message);
     
     // Store client IP for future communication
     for (int i = 0; i < numConnectedDevices; i++) {
       if (deviceIPs[i] == client->remoteIP()) {
         return; // IP already stored
       }
     }
   }
 }
 
 void sendAuthMessage() {
   // Create JSON authentication message
   DynamicJsonDocument doc(1024);
   doc["type"] = "auth";
   doc["hubId"] = uniqueId;
   doc["username"] = username;
   doc["password"] = password;
   
   String jsonString;
   serializeJson(doc, jsonString);
   
   // Send to server
   webSocket.sendTXT(jsonString);
   Serial.println("Sent authentication message to server");
 }
 
 void processServerMessage(String message) {
   // Parse JSON message
   DynamicJsonDocument doc(1024);
   DeserializationError error = deserializeJson(doc, message);
   
   if (error) {
     Serial.print("deserializeJson() failed: ");
     Serial.println(error.c_str());
     return;
   }
   
   String msgType = doc["type"];
   
   if (msgType == "control") {
     // Control command for a specific device
     String deviceId = doc["deviceId"];
     String command = doc["command"];
     
     // Forward the command to the sub-device
     forwardCommandToDevice(deviceId, command);
   } 
   else if (msgType == "status_request") {
     // Server requesting status of all devices
     sendStatusUpdate();
   }
   else if (msgType == "alarm") {
     // Alarm control command
     bool state = doc["state"];
     triggerAlarm(state);
   }
   else if (msgType == "auth_response") {
     // Authentication response
     bool success = doc["success"];
     if (success) {
       Serial.println("Authentication successful");
       // Send current status after successful authentication
       sendStatusUpdate();
     } else {
       Serial.println("Authentication failed");
       // Maybe implement retry or notification
     }
   }
 }
 
 void forwardCommandToDevice(String deviceId, String command) {
   // Find device in connected devices list
   int deviceIndex = -1;
   for (int i = 0; i < numConnectedDevices; i++) {
     if (connectedDevices[i] == deviceId) {
       deviceIndex = i;
       break;
     }
   }
   
   if (deviceIndex >= 0) {
     // Create JSON command message for the sub-device
     DynamicJsonDocument doc(512);
     doc["type"] = "command";
     doc["command"] = command;
     
     String jsonString;
     serializeJson(doc, jsonString);
     
     // Find the client with the matching IP and send the command
     bool messageSent = false;
     for (int i = 0; i < ws.getClients().length(); i++) {
       AsyncWebSocketClient *client = *ws.getClients().nth(i);
       if (client->remoteIP() == deviceIPs[deviceIndex]) {
         ws.text(client->id(), jsonString);
         messageSent = true;
         Serial.printf("Forwarded command to device %s at IP %s: %s\n", 
                       deviceId.c_str(), 
                       deviceIPs[deviceIndex].toString().c_str(),
                       command.c_str());
         break;
       }
     }
     
     if (!messageSent) {
       Serial.printf("Device %s found in list but no active WebSocket connection\n", deviceId.c_str());
     }
   } else {
     Serial.print("Device not found: ");
     Serial.println(deviceId);
   }
 }
 
 void processSubDeviceMessage(String message) {
   // Parse JSON message from sub-device
   DynamicJsonDocument doc(1024);
   DeserializationError error = deserializeJson(doc, message);
   
   if (error) {
     Serial.print("deserializeJson() failed: ");
     Serial.println(error.c_str());
     return;
   }
   
   String msgType = doc["type"];
   
   if (msgType == "registration") {
     // New device registration
     String deviceId = doc["deviceId"];
     String deviceType = doc["deviceType"];
     
     // Get the IP of the device that sent this message
     IPAddress ip;
     for (int i = 0; i < ws.getClients().length(); i++) {
       AsyncWebSocketClient *client = *ws.getClients().nth(i);
       if (client->id() == doc["clientId"].as<uint32_t>()) {
         ip = client->remoteIP();
         break;
       }
     }
     
     handleNewDevice(deviceId, deviceType, ip);
   }
   else if (msgType == "status") {
     // Status update from a device
     String deviceId = doc["deviceId"];
     String status = doc["status"];
     updateDeviceStatus(deviceId, status);
   }
   else if (msgType == "alert") {
     // Alert from a device (e.g., smoke detector)
     String deviceId = doc["deviceId"];
     String alertType = doc["alertType"];
     handleDeviceAlert(deviceId, alertType);
   }
   else if (msgType == "heartbeat") {
     // Heartbeat from a device
     String deviceId = doc["deviceId"];
     // Update last seen time if needed
     Serial.printf("Received heartbeat from device: %s\n", deviceId.c_str());
   }
 }
 
 void handleNewDevice(String deviceId, String deviceType, IPAddress ipAddress) {
   // Check if device already registered
   for (int i = 0; i < numConnectedDevices; i++) {
     if (connectedDevices[i] == deviceId) {
       Serial.println("Device already registered: " + deviceId);
       // Update IP address in case it changed
       deviceIPs[i] = ipAddress;
       return;
     }
   }
   
   // Add new device if space available
   if (numConnectedDevices < MAX_DEVICES) {
     connectedDevices[numConnectedDevices] = deviceId;
     deviceTypes[numConnectedDevices] = deviceType;
     deviceStatus[numConnectedDevices] = "Unknown"; // Initial status
     deviceIPs[numConnectedDevices] = ipAddress;
     numConnectedDevices++;
     
     Serial.println("New device registered: " + deviceId + " (" + deviceType + ") at IP " + ipAddress.toString());
     
     // Send registration confirmation to the device
     confirmDeviceRegistration(deviceId);
     
     // Update server about new device
     notifyServerNewDevice(deviceId, deviceType);
   } else {
     Serial.println("Cannot register new device, maximum reached");
   }
 }
 
 void confirmDeviceRegistration(String deviceId) {
   // Find the device and send confirmation
   for (int i = 0; i < numConnectedDevices; i++) {
     if (connectedDevices[i] == deviceId) {
       DynamicJsonDocument doc(256);
       doc["type"] = "registration_confirm";
       doc["deviceId"] = deviceId;
       doc["success"] = true;
       
       String jsonString;
       serializeJson(doc, jsonString);
       
       // Find the client with the matching IP and send confirmation
       for (int j = 0; j < ws.getClients().length(); j++) {
         AsyncWebSocketClient *client = *ws.getClients().nth(j);
         if (client->remoteIP() == deviceIPs[i]) {
           ws.text(client->id(), jsonString);
           Serial.printf("Sent registration confirmation to device %s\n", deviceId.c_str());
           break;
         }
       }
       break;
     }
   }
 }
 
 void notifyServerNewDevice(String deviceId, String deviceType) {
   if (webSocket.isConnected()) {
     // Create JSON notification message
     DynamicJsonDocument doc(512);
     doc["type"] = "device_added";
     doc["hubId"] = uniqueId;
     doc["deviceId"] = deviceId;
     doc["deviceType"] = deviceType;
     
     String jsonString;
     serializeJson(doc, jsonString);
     
     // Send to server
     webSocket.sendTXT(jsonString);
     Serial.println("Notified server about new device: " + deviceId);
   }
 }
 
 void updateDeviceStatus(String deviceId, String status) {
   // Update local status tracking
   bool deviceFound = false;
   for (int i = 0; i < numConnectedDevices; i++) {
     if (connectedDevices[i] == deviceId) {
       deviceStatus[i] = status;
       deviceFound = true;
       Serial.println("Updated status for device " + deviceId + ": " + status);
       break;
     }
   }
   
   if (!deviceFound) {
     Serial.println("Received status update for unknown device: " + deviceId);
     return;
   }
   
   // Forward to server
   if (webSocket.isConnected()) {
     // Create JSON status update message
     DynamicJsonDocument doc(512);
     doc["type"] = "device_status";
     doc["hubId"] = uniqueId;
     doc["deviceId"] = deviceId;
     doc["status"] = status;
     
     String jsonString;
     serializeJson(doc, jsonString);
     
     // Send to server
     webSocket.sendTXT(jsonString);
     Serial.println("Forwarded status update for device: " + deviceId);
   }
 }
 
 void handleDeviceAlert(String deviceId, String alertType) {
   // Handle alerts from devices (e.g., smoke detector)
   Serial.println("ALERT from device " + deviceId + ": " + alertType);
   
   // Trigger local alarm
   triggerAlarm(true);
   
   // Forward alert to server
   if (webSocket.isConnected()) {
     // Create JSON alert message
     DynamicJsonDocument doc(512);
     doc["type"] = "alert";
     doc["hubId"] = uniqueId;
     doc["deviceId"] = deviceId;
     doc["alertType"] = alertType;
     
     String jsonString;
     serializeJson(doc, jsonString);
     
     // Send to server
     webSocket.sendTXT(jsonString);
     Serial.println("Forwarded alert to server");
   }
   
   // Display alert on LCD
   lcd.clear();
   lcd.setCursor(0, 0);
   lcd.print("!!! ALERT !!!");
   lcd.setCursor(0, 1);
   lcd.print("Device: " + deviceId);
   lcd.setCursor(0, 2);
   lcd.print("Type: " + alertType);
 }
 
 void sendHeartbeat() {
   if (webSocket.isConnected()) {
     // Create JSON heartbeat message
     DynamicJsonDocument doc(256);
     doc["type"] = "heartbeat";
     doc["hubId"] = uniqueId;
     doc["time"] = millis();
     
     String jsonString;
     serializeJson(doc, jsonString);
     
     // Send to server
     webSocket.sendTXT(jsonString);
     Serial.println("Sent heartbeat to server");
   }
 }
 
 void sendStatusUpdate() {
   if (webSocket.isConnected()) {
     // Create JSON status update message
     DynamicJsonDocument doc(1024);
     doc["type"] = "hub_status";
     doc["hubId"] = uniqueId;
     doc["temperature"] = temperature;
     doc["humidity"] = humidity;
     doc["alarmState"] = alarmState;
     doc["connectedDevices"] = numConnectedDevices;
     
     // Add array of connected devices
     JsonArray devices = doc.createNestedArray("devices");
     for (int i = 0; i < numConnectedDevices; i++) {
       JsonObject device = devices.createNestedObject();
       device["id"] = connectedDevices[i];
       device["type"] = deviceTypes[i];
       device["status"] = deviceStatus[i];
     }
     
     String jsonString;
     serializeJson(doc, jsonString);
     
     // Send to server
     webSocket.sendTXT(jsonString);
     Serial.println("Sent status update to server");
   }
 }
 
 void readSensors() {
  // Read temperature and humidity from DHT11
  float newTemp = dht.readTemperature();
  float newHumidity = dht.readHumidity();
  
  // Check if reading was successful
  if (!isnan(newTemp) && !isnan(newHumidity)) {
    temperature = newTemp;
    humidity = newHumidity;
    Serial.printf("Sensor readings: Temperature %.1f°C, Humidity %.1f%%\n", temperature, humidity);
  } else {
    Serial.println("Failed to read from DHT sensor!");
  }
}

void triggerAlarm(bool state) {
  alarmState = state;
  digitalWrite(ALARM_PIN, state ? HIGH : LOW);
  
  Serial.printf("Alarm state set to: %s\n", state ? "ON" : "OFF");
  
  // Update LCD with alarm state
  updateLCD();
}

void checkButtons() {
  // Button 1 - Toggle local alarm
  if (digitalRead(BUTTON1_PIN) == LOW) {
    delay(50); // Debounce
    if (digitalRead(BUTTON1_PIN) == LOW) {
      triggerAlarm(!alarmState);
      // Wait for button release
      while (digitalRead(BUTTON1_PIN) == LOW) {
        delay(10);
      }
    }
  }
  
  // Button 2 - Cycle through connected devices on LCD
  static int currentDeviceIndex = 0;
  static unsigned long lastButtonPress = 0;
  
  if (digitalRead(BUTTON2_PIN) == LOW && millis() - lastButtonPress > 300) {
    delay(50); // Debounce
    if (digitalRead(BUTTON2_PIN) == LOW) {
      lastButtonPress = millis();
      
      if (numConnectedDevices > 0) {
        currentDeviceIndex = (currentDeviceIndex + 1) % numConnectedDevices;
        
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Device Info:");
        lcd.setCursor(0, 1);
        lcd.print(connectedDevices[currentDeviceIndex]);
        lcd.setCursor(0, 2);
        lcd.print(deviceTypes[currentDeviceIndex]);
        lcd.setCursor(0, 3);
        lcd.print(deviceStatus[currentDeviceIndex]);
      } else {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("No devices");
        lcd.setCursor(0, 1);
        lcd.print("connected");
      }
      
      // Wait for button release
      while (digitalRead(BUTTON2_PIN) == LOW) {
        delay(10);
      }
      
      // Reset LCD after 5 seconds
      delay(5000);
      updateLCD();
    }
  }
  
  // Button 3 - Send status update to server
  if (digitalRead(BUTTON3_PIN) == LOW) {
    delay(50); // Debounce
    if (digitalRead(BUTTON3_PIN) == LOW) {
      sendStatusUpdate();
      
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Status update");
      lcd.setCursor(0, 1);
      lcd.print("sent to server");
      
      // Wait for button release
      while (digitalRead(BUTTON3_PIN) == LOW) {
        delay(10);
      }
      
      // Reset LCD after 2 seconds
      delay(2000);
      updateLCD();
    }
  }
}

void updateLCD() {
  // Define LCD update states
  enum LCDState {
    SHOW_STATUS,
    SHOW_NETWORK,
    SHOW_DEVICES
  };
  
  static LCDState lcdState = SHOW_STATUS;
  static unsigned long lastLCDUpdate = 0;
  
  // Update LCD every 5 seconds (cycle through different screens)
  if (millis() - lastLCDUpdate > 5000) {
    lastLCDUpdate = millis();
    lcdState = (LCDState)((lcdState + 1) % 3);
  }
  
  lcd.clear();
  
  switch (lcdState) {
    case SHOW_STATUS:
      // Show hub status, temperature, humidity, alarm
      lcd.setCursor(0, 0);
      lcd.print("Smart Home Hub");
      lcd.setCursor(0, 1);
      lcd.printf("Temp: %.1fC", temperature);
      lcd.setCursor(0, 2);
      lcd.printf("Humidity: %.1f%%", humidity);
      lcd.setCursor(0, 3);
      lcd.print("Alarm: ");
      lcd.print(alarmState ? "ON" : "OFF");
      break;
      
    case SHOW_NETWORK:
      // Show network information
      lcd.setCursor(0, 0);
      lcd.print("Network Status");
      lcd.setCursor(0, 1);
      if (WiFi.status() == WL_CONNECTED) {
        lcd.print("WiFi: Connected");
        lcd.setCursor(0, 2);
        lcd.print(WiFi.localIP().toString());
      } else {
        lcd.print("WiFi: Disconnected");
      }
      lcd.setCursor(0, 3);
      lcd.print("AP: ");
      lcd.print(AP_SSID_PREFIX + uniqueId.substring(0, 6));
      break;
      
    case SHOW_DEVICES:
      // Show connected devices count
      lcd.setCursor(0, 0);
      lcd.print("Devices: ");
      lcd.print(numConnectedDevices);
      if (numConnectedDevices > 0) {
        // Show last 3 connected devices
        int startIdx = max(0, numConnectedDevices - 3);
        for (int i = startIdx; i < numConnectedDevices; i++) {
          lcd.setCursor(0, i - startIdx + 1);
          String displayText = connectedDevices[i];
          if (displayText.length() > 16) {
            displayText = displayText.substring(0, 13) + "...";
          }
          lcd.print(displayText);
        }
      } else {
        lcd.setCursor(0, 1);
        lcd.print("No devices");
      }
      break;
  }
}

// Helper function to find device by ID
int findDeviceById(String deviceId) {
  for (int i = 0; i < numConnectedDevices; i++) {
    if (connectedDevices[i] == deviceId) {
      return i;
    }
  }
  return -1;
}

// Helper function to broadcast message to all sub-devices
void broadcastToSubDevices(String message) {
  ws.textAll(message);
  Serial.println("Broadcasted message to all sub-devices: " + message);
}

// Function to handle specific device types
void handleDeviceTypeSpecificCommand(String deviceId, String deviceType, String command) {
  DynamicJsonDocument doc(256);
  doc["type"] = "command";
  doc["deviceId"] = deviceId;
  
  if (deviceType == "smart_switch" || deviceType == "smart_bulb") {
    // For relays, just forward the command (on/off)
    doc["command"] = command;
  }
  else if (deviceType == "window_blind") {
    // For window blinds, may need position info
    if (command == "up" || command == "down" || command == "stop") {
      doc["command"] = command;
    }
    else if (command.startsWith("position_")) {
      // Extract position percentage
      int position = command.substring(9).toInt();
      doc["command"] = "position";
      doc["value"] = position;
    }
  }
  else if (deviceType == "smoke_sensor") {
    // For sensors, usually just status requests
    if (command == "get_status") {
      doc["command"] = "read_sensor";
    }
    else if (command == "set_sensitivity") {
      doc["command"] = command;
      // Additional parameter parsing if needed
    }
  }
  else {
    // Unknown device type, forward command as is
    doc["command"] = command;
  }
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  // Find device and send command
  int deviceIndex = findDeviceById(deviceId);
  if (deviceIndex >= 0) {
    for (int i = 0; i < ws.getClients().length(); i++) {
      AsyncWebSocketClient *client = *ws.getClients().nth(i);
      if (client->remoteIP() == deviceIPs[deviceIndex]) {
        ws.text(client->id(), jsonString);
        Serial.printf("Sent type-specific command to %s device %s: %s\n", 
                     deviceType.c_str(), deviceId.c_str(), jsonString.c_str());
        break;
      }
    }
  }
}

// Function to check for inactive devices
void checkInactiveDevices() {
  // This would typically use timing to detect when devices haven't sent heartbeats
  // Simplified version that just logs connected devices
  Serial.println("Currently connected devices:");
  for (int i = 0; i < numConnectedDevices; i++) {
    Serial.printf("  %s (%s): %s\n", connectedDevices[i].c_str(), 
                 deviceTypes[i].c_str(), deviceStatus[i].c_str());
  }
}

// Handle factory reset (could be triggered by a specific button combination)
void factoryReset() {
  // Clear EEPROM
  for (int i = 0; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  
  // Reset variables
  isConfigured = false;
  internetSSID = "";
  internetPassword = "";
  username = "";
  password = "";
  uniqueId = generateUniqueId(); // Generate new ID
  
  Serial.println("Factory reset performed. Restarting...");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Factory Reset");
  lcd.setCursor(0, 1);
  lcd.print("Restarting...");
  
  delay(2000);
  ESP.restart();
}

// Check for button combination to trigger factory reset
void checkFactoryResetButtons() {
  // If all three buttons are pressed simultaneously for 5 seconds, perform factory reset
  if (digitalRead(BUTTON1_PIN) == LOW && 
      digitalRead(BUTTON2_PIN) == LOW && 
      digitalRead(BUTTON3_PIN) == LOW) {
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Hold buttons for");
    lcd.setCursor(0, 1);
    lcd.print("factory reset...");
    
    // Count down 5 seconds while buttons remain pressed
    int countdown = 5;
    while (countdown > 0 && 
           digitalRead(BUTTON1_PIN) == LOW && 
           digitalRead(BUTTON2_PIN) == LOW && 
           digitalRead(BUTTON3_PIN) == LOW) {
      
      lcd.setCursor(0, 2);
      lcd.printf("Resetting in %d...", countdown);
      
      delay(1000);
      countdown--;
    }
    
    // If buttons were held for full duration, reset
    if (countdown == 0) {
      factoryReset();
    } else {
      // Otherwise, return to normal operation
      updateLCD();
    }
  }
}



// Main loop additions to implement all functionality
void completeLoop() {
  // The existing loop code will run first, but we add these additional checks
  
  // Check for factory reset button combination
  checkFactoryResetButtons();
  
  // Periodically check for inactive devices (every 5 minutes)
  static unsigned long lastDeviceCheck = 0;
  if (millis() - lastDeviceCheck > 300000) {
    checkInactiveDevices();
    lastDeviceCheck = millis();
  }
  
  // Handle automatic reconnection to server if connection drops
  static bool wasConnected = false;
  if (webSocket.isConnected()) {
    wasConnected = true;
  } else if (wasConnected && WiFi.status() == WL_CONNECTED) {
    // If we lost connection but WiFi is still connected, try to reconnect
    Serial.println("Lost connection to server. Attempting to reconnect...");
    connectToWebSocketServer();
    wasConnected = false; // Wait for successful reconnection
  }
}