#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

// Pin definitions
#define SHIFT_DATA 0   // GPIO0 (Data pin)
#define SHIFT_CLK 2    // GPIO2 (Clock/Latch pin)
#define MOTOR_STEPS 8  // Number of steps in the sequence
//define the functions
void shiftOut(byte data);
void setMotorPins(int step);
void moveMotor(int steps);
void setPosition(int percentage);
void calibrateBlind();
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length);
void connectToHub();
void handleRoot();
void connectToHub();
void handleRoot();
void handleSetup();
void handleCalibration();
void saveConfiguration();
void loadConfiguration();
String generateUniqueId();
void setupAP();



// Constants
#define EEPROM_SIZE 512
#define AP_PREFIX "SmartBlind_"
#define STEPS_PER_REVOLUTION 4096  // For 28BYJ-48 stepper motor
#define MAX_STEPS 20000  // Maximum steps (adjust based on your blind)

// Stepper motor sequence (half-step)
const byte stepSequence[8] = {
  0b0001,  // Q1
  0b0011,  // Q1+Q2
  0b0010,  // Q2
  0b0110,  // Q2+Q3
  0b0100,  // Q3
  0b1100,  // Q3+Q4
  0b1000,  // Q4
  0b1001   // Q4+Q1
};

// Global variables
String deviceId;
String hubSsid = "";
String hubPassword = "";
bool isConfigured = false;
bool isCalibrated = false;
int currentPosition = 0;  // 0 to 100 (percentage)
int totalSteps = 0;      // Calibrated total steps
int currentStep = 0;     // Current step position
unsigned long lastHeartbeatTime = 0;
bool isMoving = false;

// Objects
ESP8266WebServer server(80);
WebSocketsClient webSocket;

// Function to send data to shift register
void shiftOut(byte data) {
  // Set latch low to start data transfer
  digitalWrite(SHIFT_CLK, LOW);
  
  // Shift out each bit
  for (int i = 7; i >= 0; i--) {
    digitalWrite(SHIFT_DATA, (data >> i) & 1);
    digitalWrite(SHIFT_CLK, HIGH);
    digitalWrite(SHIFT_CLK, LOW);
  }
  
  // Set latch high to update outputs
  digitalWrite(SHIFT_CLK, HIGH);
}

void setMotorPins(int step) {
  shiftOut(stepSequence[step]);
}

void moveMotor(int steps) {
  int direction = steps > 0 ? 1 : -1;
  steps = abs(steps);
  
  for (int i = 0; i < steps; i++) {
    currentStep = (currentStep + direction + MOTOR_STEPS) % MOTOR_STEPS;
    setMotorPins(currentStep);
    delay(2); // Adjust delay for speed control
  }
  
  // Turn off all motor pins when done
  shiftOut(0);
}

void setPosition(int percentage) {
  if (!isCalibrated || isMoving || percentage < 0 || percentage > 100) {
    return;
  }
  
  isMoving = true;
  int targetStep = (percentage * totalSteps) / 100;
  int stepsToMove = targetStep - currentStep;
  
  moveMotor(stepsToMove);
  currentPosition = percentage;
  currentStep = targetStep;
  isMoving = false;
  
  // Send position update through WebSocket
  DynamicJsonDocument doc(128);
  doc["type"] = "position_update";
  doc["deviceId"] = deviceId;
  doc["position"] = currentPosition;
  
  String jsonString;
  serializeJson(doc, jsonString);
  webSocket.sendTXT(jsonString);
}

void calibrateBlind() {
  if (isMoving) return;
  
  isMoving = true;
  
  // Move to fully closed position
  moveMotor(-MAX_STEPS); // Move down until resistance
  delay(1000);
  
  // Move to fully open position and count steps
  totalSteps = 0;
  for (int i = 0; i < MAX_STEPS; i++) {
    moveMotor(1);
    totalSteps++;
    delay(2);
  }
  
  // Move back to closed position
  moveMotor(-totalSteps);
  currentStep = 0;
  currentPosition = 0;
  
  isCalibrated = true;
  isMoving = false;
  
  // Save calibration to EEPROM
  EEPROM.write(100, isCalibrated);
  EEPROM.write(101, totalSteps & 0xFF);
  EEPROM.write(102, (totalSteps >> 8) & 0xFF);
  EEPROM.commit();
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("WebSocket disconnected");
      break;
    
    case WStype_CONNECTED:
      Serial.println("WebSocket connected");
      break;
    
    case WStype_TEXT: {
      DynamicJsonDocument doc(256);
      deserializeJson(doc, payload);
      
      if (doc["type"] == "set_position") {
        int position = doc["position"];
        setPosition(position);
      } else if (doc["type"] == "calibrate") {
        calibrateBlind();
      }
      break;
    }
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
    Serial.println("\nConnected to hub");
    
    // Connect to WebSocket server
    webSocket.begin("192.168.1.1", 81, "/ws");
    webSocket.onEvent(webSocketEvent);
    webSocket.setReconnectInterval(5000);
  }
}
void handleRoot() {
  String html = "<!DOCTYPE html><html>";
  html += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 20px; max-width: 600px; margin: 0 auto; padding: 20px; }";
  html += ".container { background-color: #f9f9f9; border-radius: 8px; padding: 20px; margin-top: 20px; }";
  html += "h1 { color: #333; }";
  html += "input { width: 100%; padding: 8px; margin: 8px 0; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; }";
  html += "button { background-color: #4CAF50; color: white; padding: 10px 15px; border: none; border-radius: 4px; cursor: pointer; width: 100%; }";
  html += "button:hover { background-color: #45a049; }";
  html += ".status { margin-top: 20px; padding: 10px; border-radius: 4px; }";
  html += ".success { background-color: #dff0d8; color: #3c763d; }";
  html += ".error { background-color: #f2dede; color: #a94442; }";
  html += "</style></head>";
  html += "<body>";
  
  // Device Information
  html += "<h1>Smart Blind Setup</h1>";
  html += "<div class='container'>";
  html += "<h2>Device Information</h2>";
  html += "<p>Device ID: " + deviceId + "</p>";
  html += "<p>Status: " + String(isConfigured ? "Configured" : "Not Configured") + "</p>";
  html += "<p>Calibration: " + String(isCalibrated ? "Calibrated" : "Not Calibrated") + "</p>";
  html += "</div>";
  
  // WiFi Setup Form
  html += "<div class='container'>";
  html += "<h2>Hub Connection Setup</h2>";
  html += "<form action='/setup' method='get'>";
  html += "<div>Hub SSID:<br><input type='text' name='hubssid' required></div>";
  html += "<div>Hub Password:<br><input type='password' name='hubpass' required></div>";
  html += "<div><button type='submit'>Save Configuration</button></div>";
  html += "</form>";
  html += "</div>";
  
  // Calibration Section (only shown if configured)
  if (isConfigured) {
    html += "<div class='container'>";
    html += "<h2>Blind Calibration</h2>";
    if (!isCalibrated) {
      html += "<p>Your blind needs to be calibrated before use.</p>";
      html += "<form action='/calibrate' method='get'>";
      html += "<button type='submit'>Start Calibration</button>";
      html += "</form>";
    } else {
      html += "<p>Blind is calibrated with " + String(totalSteps) + " total steps.</p>";
      html += "<form action='/calibrate' method='get'>";
      html += "<button type='submit'>Recalibrate</button>";
      html += "</form>";
    }
    html += "</div>";
  }
  
  // Manual Control Section (only shown if calibrated)
  if (isCalibrated) {
    html += "<div class='container'>";
    html += "<h2>Manual Control</h2>";
    html += "<p>Current Position: " + String(currentPosition) + "%</p>";
    html += "<form action='/setposition' method='get'>";
    html += "<div>Set Position (0-100%):<br>";
    html += "<input type='number' name='position' min='0' max='100' required></div>";
    html += "<button type='submit'>Move Blind</button>";
    html += "</form>";
    html += "</div>";
  }
  
  // Connection Status
  if (isConfigured) {
    html += "<div class='container'>";
    html += "<h2>Connection Status</h2>";
    html += "<p>WiFi Status: " + String(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected") + "</p>";
    if (WiFi.status() == WL_CONNECTED) {
      html += "<p>IP Address: " + WiFi.localIP().toString() + "</p>";
      html += "<p>Signal Strength: " + String(WiFi.RSSI()) + " dBm</p>";
    }
    html += "</div>";
  }
  
  html += "</body></html>";
  server.send(200, "text/html", html);
}
void handleSetup() {
  hubSsid = server.arg("hubssid");
  hubPassword = server.arg("hubpass");
  
  if (hubSsid.length() > 0 && hubPassword.length() > 0) {
    isConfigured = true;
    saveConfiguration();
    
    String html = "<!DOCTYPE html><html>";
    html += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    html += "<style>body{font-family:Arial;margin:20px}</style></head>";
    html += "<body><h1>Configuration Saved</h1>";
    html += "<p>Device will now restart and connect to the hub.</p></body></html>";
    
    server.send(200, "text/html", html);
    delay(2000);
    ESP.restart();
  }
}

void handleCalibration() {
  calibrateBlind();
  
  String html = "<!DOCTYPE html><html>";
  html += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<style>body{font-family:Arial;margin:20px}</style></head>";
  html += "<body><h1>Calibration Complete</h1>";
  html += "<p>The blind has been calibrated.</p></body></html>";
  
  server.send(200, "text/html", html);
}

void saveConfiguration() {
  // Save SSID
  for (int i = 0; i < hubSsid.length(); i++) {
    EEPROM.write(i, hubSsid[i]);
  }
  EEPROM.write(32, 0);
  
  // Save password
  for (int i = 0; i < hubPassword.length(); i++) {
    EEPROM.write(i + 33, hubPassword[i]);
  }
  EEPROM.write(65, 0);
  
  EEPROM.write(99, isConfigured);
  EEPROM.commit();
}

void loadConfiguration() {
  isConfigured = EEPROM.read(99);
  
  if (isConfigured) {
    // Load SSID
    String ssid = "";
    for (int i = 0; i < 32; i++) {
      char c = EEPROM.read(i);
      if (c == 0) break;
      ssid += c;
    }
    hubSsid = ssid;
    
    // Load password
    String pass = "";
    for (int i = 33; i < 65; i++) {
      char c = EEPROM.read(i);
      if (c == 0) break;
      pass += c;
    }
    hubPassword = pass;
    
    // Load calibration data
    isCalibrated = EEPROM.read(100);
    if (isCalibrated) {
      totalSteps = EEPROM.read(101) | (EEPROM.read(102) << 8);
    }
  }
}

String generateUniqueId() {
  String id = "";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  
  for (int i = 0; i < 6; i++) {
    if (mac[i] < 16) id += "0";
    id += String(mac[i], HEX);
  }
  return id;
}

void setup() {
  Serial.begin(115200);
  
  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);
  
  // Set pin modes for shift register
  pinMode(SHIFT_DATA, OUTPUT);
  pinMode(SHIFT_CLK, OUTPUT);
  
  // Generate or load device ID
  deviceId = generateUniqueId();
  
  // Load configuration if available
  loadConfiguration();
  
  if (!isConfigured) {
    setupAP();
    server.on("/", HTTP_GET, handleRoot);
    server.on("/setup", HTTP_GET, handleSetup);
    server.on("/calibrate", HTTP_GET, handleCalibration);
    server.begin();
    Serial.println("HTTP server started in AP mode");
  } else {
    connectToHub();
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
void loop() {
  if (!isConfigured) {
    server.handleClient();
  } else {
    webSocket.loop();
    
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