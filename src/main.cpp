/*********
  Rui Santos
  Complete instructions at https://RandomNerdTutorials.com/build-web-servers-ebook/
  
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*********/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include <Arduino_JSON.h>


// Access Point credentials
const char* ap_ssid = "ESP8266_Sensors";
const char* ap_password = "12345678";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Create an Event Source on /events
AsyncEventSource events("/events");

// Json Variable to Hold Sensor Readings
JSONVar readings;


// Timer variables
unsigned long lastTime = 0;
unsigned long timerDelay = 90000;  // Aumentado a 90 segundos para mayor estabilidad
unsigned long lastPulseCount = 0;  // Para detectar cambios estables

// FC-03 IR sensor on D2 (GPIO4)
const int IR_SENSOR_PIN = D2;
volatile unsigned long pulseCount = 0;
volatile bool pulseDetected = false;  // Flag para manejar interrupciones de forma segura

// JSN-SR04T Ultrasonic sensor pins
const int TRIGGER_PIN = D5;  // GPIO14
const int ECHO_PIN = D6;     // GPIO12

// Distance measurement variables
volatile unsigned long echoStartTime = 0;
volatile unsigned long echoEndTime = 0;
volatile bool echoReceived = false;
float distance_cm = 0.0;
unsigned long lastDistanceRead = 0;
const unsigned long DISTANCE_INTERVAL = 1000;  // Read distance every 1 second

// Viscometer Couette constants (from technical specifications)
const float C_GEOM = 2.856;        // Geometric constant (m³)
const float PULSE_TO_TORQUE = 0.001; // Conversion factor: pulses to N·m
const float RPM_TO_RAD_S = 0.10472;  // Conversion: RPM to rad/s (2π/60)

// Stability control
bool systemStable = true;
unsigned long lastResetCheck = 0;

// Experiment control
bool experimentRunning = false;
unsigned long experimentStartTime = 0;
unsigned long pulseCountAtStart = 0;

void IRAM_ATTR handleIrSensor() {
  // Interrupción simplificada para mayor estabilidad
  if (experimentRunning) {
    pulseDetected = true;
  }
}

// JSN-SR04T Echo interrupt handler
void IRAM_ATTR handleEcho() {
  if (digitalRead(ECHO_PIN) == HIGH) {
    // Echo pulse started
    echoStartTime = micros();
  } else {
    // Echo pulse ended
    echoEndTime = micros();
    echoReceived = true;
  }
}

// Function to trigger ultrasonic measurement
void triggerUltrasonic() {
  digitalWrite(TRIGGER_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIGGER_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIGGER_PIN, LOW);
}

// Function to read distance from JSN-SR04T
float readDistance() {
  echoReceived = false;
  
  // Trigger measurement
  triggerUltrasonic();
  
  // Wait for echo with timeout (30ms max for ~5m range)
  unsigned long timeout = millis() + 30;
  while (!echoReceived && millis() < timeout) {
    yield(); // Allow system tasks
  }
  
  if (!echoReceived) {
    return -1.0; // Timeout - no object detected or out of range
  }
  
  // Calculate distance in cm
  // Speed of sound = 343 m/s = 0.0343 cm/µs
  // Distance = (time * speed) / 2 (round trip)
  unsigned long duration = echoEndTime - echoStartTime;
  float distance = (duration * 0.0343) / 2.0;
  
  // Validate range (JSN-SR04T: 2cm - 450cm)
  if (distance < 2.0 || distance > 450.0) {
    return -1.0; // Out of valid range
  }
  
  return distance;
}





// Calculate viscosity using Couette viscometer theory
float calculateViscosity(unsigned long pulses, float rpm) {
  // Convert pulses to torque (N·m)
  float torque = pulses * PULSE_TO_TORQUE;
  
  // Convert RPM to angular velocity (rad/s)
  float omega = rpm * RPM_TO_RAD_S;
  
  // Avoid division by zero
  if (omega == 0) return 0.0;
  
  // Calculate dynamic viscosity: η = M/(C_geom × ω) 
  float viscosity = torque / (C_GEOM * omega);
  
  // Convert from Pa·s to mPa·s (millipascal-seconds)
  return viscosity * 1000.0;
}

// Get IR pulse count and calculated viscosity (optimized for memory)
String getSensorReadings() {
  // Check system stability first
  if (!systemStable) {
    return "{\"error\":\"System unstable\"}";
  }
  
  // Read distance if enough time has passed
  if (millis() - lastDistanceRead >= DISTANCE_INTERVAL) {
    distance_cm = readDistance();
    lastDistanceRead = millis();
  }
  
  // Calculate values
  unsigned long experimentPulses = experimentRunning ? 
    (pulseCount - pulseCountAtStart) : pulseCount;
  float simulatedRPM = (experimentPulses % 100) + 10.0;
  float viscosity_mPas = calculateViscosity(experimentPulses, simulatedRPM);
  
  // Build JSON string directly (more memory efficient)
  String json = "{";
  json += "\"ir_pulses\":" + String(experimentPulses) + ",";
  json += "\"rpm\":" + String(simulatedRPM, 1) + ",";
  json += "\"viscosity\":" + String(viscosity_mPas, 3) + ",";
  json += "\"distance\":" + String(distance_cm, 1) + ",";
  json += "\"distance_units\":\"cm\",";
  json += "\"units\":\"mPa·s\",";
  json += "\"status\":\"OK\",";
  json += "\"experiment_status\":\"" + String(experimentRunning ? "running" : "stopped") + "\"";
  json += "}";
  
  return json;
}

// Initialize LittleFS
void initFS() {
  if (!LittleFS.begin()) {
    Serial.println("An error has occurred while mounting LittleFS");
  }
  else {
    Serial.println("LittleFS mounted successfully");
  }
}

// Initialize WiFi as Access Point
void initWiFi() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password);
  
  // Wait for AP to start
  delay(2000);
  
  Serial.println("Access Point started");
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
  Serial.print("Max connections: ");
  Serial.println(WiFi.softAPgetStationNum());
}

void setup() {
  // Serial port for debugging purposes

  Serial.begin(9600);
  
  // Initialize IR sensor (FC-03)
  pinMode(IR_SENSOR_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(IR_SENSOR_PIN), handleIrSensor, FALLING);
  
  // Initialize JSN-SR04T ultrasonic sensor
  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIGGER_PIN, LOW);
  attachInterrupt(digitalPinToInterrupt(ECHO_PIN), handleEcho, CHANGE);
  
  Serial.println("Sensors initialized:");
  Serial.println("- FC-03 IR sensor on pin D2 (GPIO4)");
  Serial.println("- JSN-SR04T ultrasonic on pins D5(trig)/D6(echo)");
  
  initWiFi();
  initFS();

  // Web Server Root URL
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/index.html", "text/html");
  });

  server.serveStatic("/", LittleFS, "/");
  
  // Request for the latest sensor readings
  server.on("/readings", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = getSensorReadings();
    request->send(200, "application/json", json);
    json = String(); // Free memory
  });

  // Experiment control endpoint
  server.on("/control", HTTP_POST, [](AsyncWebServerRequest *request){
    String action = "";
    if (request->hasParam("action")) {
      action = request->getParam("action")->value();
    }
    
    if (action == "start") {
      experimentRunning = true;
      experimentStartTime = millis();
      pulseCountAtStart = pulseCount;
      Serial.println("Experiment STARTED");
      request->send(200, "text/plain", "Experiment started");
    }
    else if (action == "stop") {
      experimentRunning = false;
      Serial.println("Experiment STOPPED");
      request->send(200, "text/plain", "Experiment stopped");
    }
    else if (action == "reset") {
      pulseCount = 0;
      pulseCountAtStart = 0;
      experimentStartTime = millis();
      Serial.println("Counters RESET");
      request->send(200, "text/plain", "Counters reset");
    }
    else {
      request->send(400, "text/plain", "Invalid action");
    }
  });

  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID: %u\n", client->lastId());
    }
    // Send initial connection message
    client->send("hello!", NULL, millis(), 10000);
  });
  
  server.addHandler(&events);

  // Start server
  server.begin();
}

void loop() {
  // Manejar pulsos fuera de la interrupción para mayor estabilidad
  if (pulseDetected) {
    pulseDetected = false;
    pulseCount++;
  }
  
  // Check system stability every 10 seconds (menos frecuente)
  if (millis() - lastResetCheck > 10000) {
    // Monitor memory and connections
    size_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < 2000) {  // Umbral más conservador
      Serial.printf("Warning: Low memory - %u bytes\n", freeHeap);
      systemStable = false;
    } else {
      systemStable = true;
    }
    lastResetCheck = millis();
    
    // Watchdog reset manual para evitar cuelgues
    ESP.wdtFeed();
  }

  // Send sensor data con intervalo aumentado
  if ((millis() - lastTime) > timerDelay && systemStable) {
    // Solo enviar si hay cambios significativos o es forzado
    unsigned long timeSinceLastUpdate = millis() - lastTime;
    
    if (timeSinceLastUpdate > timerDelay || abs((long)pulseCount - (long)lastPulseCount) > 2) {
      lastPulseCount = pulseCount;
      
      // Send events with error handling
      String sensorData = getSensorReadings();
      if (sensorData.length() > 0 && sensorData.length() < 500) {  // Validar tamaño
        events.send("ping", NULL, millis());
        events.send(sensorData.c_str(), "new_readings", millis());
        Serial.printf("Data sent. Free heap: %u bytes\n", ESP.getFreeHeap());
        
        // Liberar memoria explícitamente
        sensorData = String();
      } else {
        Serial.println("Data validation failed or too large");
      }
      lastTime = millis();
    }
  }
  
  // Small delay to prevent watchdog reset
  delay(50);  // Aumentado para mayor estabilidad
  yield();    // Permitir que el ESP8266 maneje tareas internas
}