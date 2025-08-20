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
unsigned long timerDelay = 30000;

// FC-03 IR sensor on D2 (GPIO4)
const int IR_SENSOR_PIN = D2;
volatile unsigned long pulseCount = 0;

// Viscometer Couette constants (from technical specifications)
const float C_GEOM = 2.856;        // Geometric constant (m³)
const float PULSE_TO_TORQUE = 0.001; // Conversion factor: pulses to N·m
const float RPM_TO_RAD_S = 0.10472;  // Conversion: RPM to rad/s (2π/60)

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
  // Simulate RPM based on pulse count (for demonstration)
  float simulatedRPM = (pulseCount % 100) + 10; // 10-109 RPM range
  
  // Calculate viscosity using Couette theory
  float viscosity_mPas = calculateViscosity(pulseCount, simulatedRPM);
  
  readings["ir_pulses"] = String(pulseCount);
  readings["rpm"] = String(simulatedRPM, 1);
  readings["viscosity"] = String(viscosity_mPas, 3);
  readings["units"] = "mPa·s";
  
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
  Serial.println("Access Point started");
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
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
    json = String();
  });

  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    client->send("hello!", NULL, millis(), 10000);
  });
  server.addHandler(&events);

  // Start server
  server.begin();
}

void loop() {
  if ((millis() - lastTime) > timerDelay) {
    // Send Events to the client with the Sensor Readings Every 30 seconds
    events.send("ping",NULL,millis());
    events.send(getSensorReadings().c_str(),"new_readings" ,millis());
    lastTime = millis();
  }
}