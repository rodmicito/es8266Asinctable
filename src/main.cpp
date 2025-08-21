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
unsigned long timerDelay = 60000;  // Aumentado a 60 segundos para más estabilidad
unsigned long lastPulseCount = 0;  // Para detectar cambios estables

// FC-03 IR sensor on D2 (GPIO4)
const int IR_SENSOR_PIN = D2;
volatile unsigned long pulseCount = 0;

// Viscometer Couette constants (from technical specifications)
const float C_GEOM = 2.856;        // Geometric constant (m³)
const float PULSE_TO_TORQUE = 0.001; // Conversion factor: pulses to N·m
const float RPM_TO_RAD_S = 0.10472;  // Conversion: RPM to rad/s (2π/60)

// Stability control
bool systemStable = true;
unsigned long lastResetCheck = 0;

void IRAM_ATTR handleIrSensor() {
  pulseCount++;
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

// Get IR pulse count and calculated viscosity
String getSensorReadings() {
  // Check system stability
  if (!systemStable) {
    readings["error"] = "System unstable";
    return JSON.stringify(readings);
  }
  
  // Simulate RPM based on pulse count (for demonstration)
  float simulatedRPM = (pulseCount % 100) + 10; // 10-109 RPM range
  
  // Calculate viscosity using Couette theory
  float viscosity_mPas = calculateViscosity(pulseCount, simulatedRPM);
  
  // Clear previous data to avoid memory issues
  readings = JSONVar();
  
  readings["ir_pulses"] = String(pulseCount);
  readings["rpm"] = String(simulatedRPM, 1);
  readings["viscosity"] = String(viscosity_mPas, 3);
  readings["units"] = "mPa·s";
  readings["status"] = "OK";
  
  String jsonString = JSON.stringify(readings);
  return jsonString;
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
  pinMode(IR_SENSOR_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(IR_SENSOR_PIN), handleIrSensor, FALLING);
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

  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID: %u\n", client->lastId());
    }
    // Send initial connection message
    client->send("hello!", NULL, millis(), 10000);
  });
  
  // Add error handling for events
  events.onError([](AsyncEventSourceClient *client, int error){
    Serial.printf("Event source error: %d\n", error);
  });
  server.addHandler(&events);

  // Start server
  server.begin();
}

void loop() {
  // Check system stability every 5 seconds
  if (millis() - lastResetCheck > 5000) {
    // Monitor memory and connections
    if (ESP.getFreeHeap() < 1000) {
      Serial.println("Warning: Low memory");
      systemStable = false;
    } else {
      systemStable = true;
    }
    lastResetCheck = millis();
  }

  // Send sensor data every 30 seconds
  if ((millis() - lastTime) > timerDelay && systemStable) {
    // Check if pulse count is changing (system active)
    if (pulseCount != lastPulseCount) {
      lastPulseCount = pulseCount;
      
      // Send events with error handling
      String sensorData = getSensorReadings();
      if (sensorData.length() > 0) {
        events.send("ping", NULL, millis());
        events.send(sensorData.c_str(), "new_readings", millis());
        Serial.printf("Data sent. Free heap: %d bytes\n", ESP.getFreeHeap());
      }
    }
    lastTime = millis();
  }
  
  // Small delay to prevent watchdog reset
  delay(10);
}