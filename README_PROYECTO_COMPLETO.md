# 📊 VISCOSÍMETRO COUETTE ESP8266 - DOCUMENTACIÓN COMPLETA

## 🎯 RESUMEN EJECUTIVO

Este proyecto implementa un **viscosímetro Couette digital** usando un ESP8266 con interfaz web asíncrona, basado en la teoría y especificaciones técnicas del documento "VISCOSIMETRO_COUETTE_ESPECIFICACIONES_TECNICAS.txt". El sistema permite medir viscosidad dinámica de fluidos mediante conteo de pulsos de un sensor infrarrojo FC-03 y cálculos automáticos basados en principios físicos fundamentales.

---

## 📋 TABLA DE CONTENIDOS

1. [Arquitectura del Sistema](#arquitectura-del-sistema)
2. [Ramas del Repositorio](#ramas-del-repositorio)
3. [Hardware Requerido](#hardware-requerido)
4. [Configuración de Software](#configuración-de-software)
5. [Estructura del Código](#estructura-del-código)
6. [Interfaz Web](#interfaz-web)
7. [API REST](#api-rest)
8. [Cálculos de Viscosidad](#cálculos-de-viscosidad)
9. [Optimizaciones de Estabilidad](#optimizaciones-de-estabilidad)
10. [Instrucciones de Desarrollo](#instrucciones-de-desarrollo)
11. [Troubleshooting](#troubleshooting)
12. [Roadmap Futuro](#roadmap-futuro)

---

## 🏗️ ARQUITECTURA DEL SISTEMA

### Componentes Principales:
- **ESP8266 (ESP-12E)**: Microcontrolador principal con WiFi integrado
- **Sensor FC-03**: Sensor infrarrojo para conteo de pulsos (GPIO D2)
- **Access Point**: Red WiFi propia para acceso independiente
- **Servidor Web Asíncrono**: Interface HTTP para control y monitoreo
- **Sistema de Archivos LittleFS**: Almacenamiento de archivos web
- **Server-Sent Events (SSE)**: Comunicación en tiempo real con navegador

### Flujo de Datos:
```
Sensor FC-03 → Interrupción GPIO → Conteo Pulsos → Cálculo Viscosidad → JSON → Web Interface
```

---

## 🌿 RAMAS DEL REPOSITORIO

| Rama | Descripción | Estado | Uso Recomendado |
|------|-------------|--------|-----------------|
| `main` | Código original con sensor BME280 | ✅ Estable | Referencia inicial |
| `sensor_simulado` | Sensores simulados con valores aleatorios | ✅ Estable | Testing sin hardware |
| `sensor_infrarrojo_fc03` | Implementación sensor IR FC-03 | ✅ Estable | Desarrollo básico |
| `access_point_mode` | **PRINCIPAL** - AP + Viscosímetro + Controles | ✅ **ACTIVA** | **Producción** |

### 🎯 **Rama Principal: `access_point_mode`**
Esta es la rama más completa y estable, con todas las funcionalidades implementadas.

---

## ⚙️ HARDWARE REQUERIDO

### Componentes Esenciales:
- **ESP8266 ESP-12E** (o compatible)
- **Sensor Infrarrojo FC-03** 
- **Resistencias pull-up** (10kΩ recomendado)
- **Fuente 3.3V/5V** (según especificaciones)
- **Breadboard y cables jumper**

### Conexiones:
```
ESP8266 ESP-12E:
├── D2 (GPIO4) ← FC-03 Signal Pin
├── 3.3V → FC-03 VCC
├── GND → FC-03 GND
└── UART (TX/RX) → Programación
```

### Especificaciones Sensor FC-03:
- **Voltaje**: 3.3V - 5V
- **Corriente**: 20mA (típico)
- **Distancia detección**: 2-30cm
- **Salida**: Digital (HIGH/LOW)
- **Frecuencia máxima**: 1kHz

---

## 💻 CONFIGURACIÓN DE SOFTWARE

### Dependencias PlatformIO:
```ini
[env:esp12e]
platform = espressif8266
board = esp12e
framework = arduino
monitor_speed = 9600
lib_deps = 
    ESP Async WebServer
    ESPAsyncTCP
    arduino-libraries/Arduino_JSON @ 0.1.0
    adafruit/Adafruit BME280 Library @ ^2.1.0
    adafruit/Adafruit Unified Sensor @ ^1.1.4
board_build.filesystem = littlefs
```

### Configuración Access Point:
```cpp
const char* ap_ssid = "ESP8266_Sensors";
const char* ap_password = "12345678";
// IP por defecto: 192.168.4.1
```

---

## 📁 ESTRUCTURA DEL CÓDIGO

### Archivo Principal: `src/main.cpp`

#### 🔧 Variables Globales Clave:
```cpp
// Configuración Access Point
const char* ap_ssid = "ESP8266_Sensors";
const char* ap_password = "12345678";

// Sensor infrarrojo FC-03
const int IR_SENSOR_PIN = D2;
volatile unsigned long pulseCount = 0;
volatile bool pulseDetected = false;

// Control de experimento
bool experimentRunning = false;
unsigned long experimentStartTime = 0;
unsigned long pulseCountAtStart = 0;

// Constantes viscosímetro Couette (del documento técnico)
const float C_GEOM = 2.856;        // Constante geométrica (m³)
const float PULSE_TO_TORQUE = 0.001; // Factor conversión pulsos a N·m
const float RPM_TO_RAD_S = 0.10472;  // Conversión RPM a rad/s

// Estabilidad del sistema
bool systemStable = true;
unsigned long timerDelay = 90000;  // Intervalo actualizaciones (90s)
```

#### 🎯 Funciones Principales:

1. **`handleIrSensor()`** - ISR para detección de pulsos
2. **`calculateViscosity()`** - Cálculo según teoría Couette
3. **`getSensorReadings()`** - Generación JSON optimizada
4. **`initWiFi()`** - Configuración Access Point
5. **`initFS()`** - Inicialización sistema de archivos

### Sistema de Archivos: `data/`

#### 📄 **index.html** - Interfaz principal:
```html
<!-- Botones de control -->
<button id="start-btn" onclick="startExperiment()">INICIAR EXPERIMENTO</button>
<button id="stop-btn" onclick="stopExperiment()">DETENER EXPERIMENTO</button>
<button id="reset-btn" onclick="resetExperiment()">RESET CONTADORES</button>

<!-- Tabla de mediciones -->
<table>
  <tr><td>Pulsos IR</td><td><span id="ir_pulses"></span></td></tr>
  <tr><td>Velocidad</td><td><span id="rpm"></span> RPM</td></tr>
  <tr><td>Viscosidad Dinámica</td><td><span id="viscosity"></span> mPa·s</td></tr>
</table>
```

#### 🎨 **style.css** - Estilos optimizados:
- Botones con colores intuitivos (verde, rojo, amarillo)
- Diseño responsive
- Tabla con hover effects
- Estados deshabilitados

#### ⚡ **script.js** - Lógica frontend:
```javascript
// Control de experimento
function startExperiment() {
  fetch('/control?action=start', {method: 'POST'})
    .then(response => response.text())
    .then(data => {
      experimentRunning = true;
      updateButtonStates();
    });
}

// Server-Sent Events para tiempo real
if (!!window.EventSource) {
  var source = new EventSource('/events');
  source.addEventListener('new_readings', function(e) {
    var obj = JSON.parse(e.data);
    // Actualizar DOM automáticamente
  });
}
```

---

## 🌐 INTERFAZ WEB

### Acceso:
1. **Conectar a WiFi**: "ESP8266_Sensors" (password: 12345678)
2. **Abrir navegador**: http://192.168.4.1
3. **Interfaz automática**: Sin configuración adicional

### Funcionalidades:
- ✅ **Visualización en tiempo real** (cada 90 segundos)
- ✅ **Control de experimento** (Iniciar/Detener/Reset)
- ✅ **Estado del sistema** (Running/Stopped)
- ✅ **Datos calculados** (Pulsos, RPM, Viscosidad)
- ✅ **Timestamp** de última actualización

### Estados de Botones:
```javascript
INICIAR EXPERIMENTO:
├── Habilitado cuando: experiment_status = "stopped"
├── Color: Verde (#28a745)
└── Acción: POST /control?action=start

DETENER EXPERIMENTO:
├── Habilitado cuando: experiment_status = "running"  
├── Color: Rojo (#dc3545)
└── Acción: POST /control?action=stop

RESET CONTADORES:
├── Siempre habilitado
├── Color: Amarillo (#ffc107)
└── Acción: POST /control?action=reset
```

---

## 🔌 API REST

### Endpoints Disponibles:

#### `GET /` - Página principal
```http
GET /
Response: text/html (index.html)
```

#### `GET /readings` - Datos de sensores
```http
GET /readings
Response: application/json
```

**Ejemplo de respuesta:**
```json
{
  "ir_pulses": 1234,
  "rpm": 45.6,
  "viscosity": 23.456,
  "units": "mPa·s",
  "status": "OK",
  "experiment_status": "running"
}
```

#### `POST /control` - Control de experimento
```http
POST /control?action={start|stop|reset}
Response: text/plain
```

**Acciones disponibles:**
- `action=start` → Inicia conteo de pulsos
- `action=stop` → Detiene conteo (mantiene valores)
- `action=reset` → Reinicia contadores a cero

#### `GET /events` - Server-Sent Events
```http
GET /events
Response: text/event-stream

Eventos:
├── "ping" - Keepalive cada 90s
└── "new_readings" - Datos de sensores
```

---

## 🧮 CÁLCULOS DE VISCOSIDAD

### Fundamento Teórico:
Basado en el viscosímetro Couette según especificaciones técnicas del documento incluido.

#### Ecuación Fundamental:
```
η = M / (C_geom × ω)

Donde:
├── η = Viscosidad dinámica (Pa·s)
├── M = Momento/Torque aplicado (N·m)  
├── C_geom = Constante geométrica (2.856 m³)
└── ω = Velocidad angular (rad/s)
```

#### Conversiones:
```cpp
// Pulsos a Torque
float torque = pulses * PULSE_TO_TORQUE; // 0.001 N·m por pulso

// RPM a rad/s  
float omega = rpm * RPM_TO_RAD_S; // 2π/60 = 0.10472

// Pa·s a mPa·s
float viscosity_mPas = viscosity_Pas * 1000.0;
```

#### Implementación:
```cpp
float calculateViscosity(unsigned long pulses, float rpm) {
  float torque = pulses * PULSE_TO_TORQUE;
  float omega = rpm * RPM_TO_RAD_S;
  
  if (omega == 0) return 0.0;
  
  float viscosity = torque / (C_GEOM * omega);
  return viscosity * 1000.0; // Convertir a mPa·s
}
```

### Rangos Esperados:
- **Agua (20°C)**: ~1 mPa·s
- **Aceites ligeros**: 10-100 mPa·s  
- **Aceites pesados**: 100-1000 mPa·s
- **Glicerina**: ~900 mPa·s

---

## 🛡️ OPTIMIZACIONES DE ESTABILIDAD

### Problemas Resueltos:
1. **Reinicios durante actualizaciones** ✅
2. **Memory leaks** ✅  
3. **Watchdog timeouts** ✅
4. **Interrupciones inestables** ✅

### Técnicas Implementadas:

#### 1. **Manejo Seguro de Interrupciones:**
```cpp
// ISR minimalista - solo flag
void IRAM_ATTR handleIrSensor() {
  if (experimentRunning) {
    pulseDetected = true;
  }
}

// Procesamiento en loop principal
void loop() {
  if (pulseDetected) {
    pulseDetected = false;
    pulseCount++;
  }
}
```

#### 2. **Gestión Optimizada de Memoria:**
```cpp
// JSON directo (más eficiente que JSONVar)
String getSensorReadings() {
  String json = "{";
  json += "\"ir_pulses\":" + String(pulses) + ",";
  json += "\"viscosity\":" + String(visc, 3);
  json += "}";
  return json;
}

// Liberación explícita
sensorData = String(); // Liberar memoria
```

#### 3. **Watchdog y Yield:**
```cpp
void loop() {
  // Alimentar watchdog
  ESP.wdtFeed();
  
  // Permitir tareas internas
  yield();
  
  // Delay estable
  delay(50);
}
```

#### 4. **Monitoreo de Sistema:**
```cpp
// Cada 10 segundos
if (ESP.getFreeHeap() < 2000) {
  systemStable = false;
  Serial.println("Warning: Low memory");
}
```

### Parámetros de Estabilidad:
- **Intervalo actualizaciones**: 90 segundos
- **Umbral memoria**: 2000 bytes libres
- **Delay loop**: 50ms
- **Timeout watchdog**: Auto-feed
- **Validación datos**: < 500 bytes JSON

---

## 🚀 INSTRUCCIONES DE DESARROLLO

### Setup Inicial:
```bash
# 1. Clonar repositorio
git clone https://github.com/rodmicito/es8266Asinctable.git
cd es8266Asinctable

# 2. Cambiar a rama principal
git checkout access_point_mode

# 3. Abrir con PlatformIO (VS Code)
code .

# 4. Compilar y subir
pio run --target upload --upload-port COM3
pio run --target uploadfs --upload-port COM3
```

### Workflow de Desarrollo:

#### Para nuevas funcionalidades:
```bash
# 1. Crear rama desde access_point_mode
git checkout access_point_mode
git pull origin access_point_mode
git checkout -b nueva_funcionalidad

# 2. Desarrollar y probar
# 3. Commit y push
git add .
git commit -m "Descripción de cambios"
git push -u origin nueva_funcionalidad

# 4. Merge request a access_point_mode
```

#### Para modificaciones menores:
```bash
# Trabajar directamente en access_point_mode
git checkout access_point_mode
# ... desarrollar ...
git add .
git commit -m "Fix: descripción"
git push origin access_point_mode
```

### Testing:
1. **Hardware**: Verificar conexiones del FC-03
2. **Compilación**: `pio run` sin errores
3. **Upload**: Firmware y filesystem exitosos
4. **Conectividad**: Red "ESP8266_Sensors" visible
5. **Web**: http://192.168.4.1 accesible
6. **Funcionalidad**: Botones y datos funcionando
7. **Estabilidad**: Sin reinicios por 15+ minutos

---

## 🔧 TROUBLESHOOTING

### Problemas Comunes:

#### 1. **ESP8266 no se conecta:**
```bash
# Verificar puerto COM
ls /dev/tty* # Linux/Mac
# o revisar Device Manager en Windows

# Boot mode
pio run --target upload --upload-port COM3 --upload-resetmethod nodemcu
```

#### 2. **Error 404 en web:**
```bash
# Subir sistema de archivos
pio run --target uploadfs --upload-port COM3

# Verificar archivos data/
ls data/  # Debe contener: index.html, script.js, style.css, favicon.png
```

#### 3. **Reinicios constantes:**
```cpp
// Verificar en código:
unsigned long timerDelay = 90000;  // Debe ser >= 90s
ESP.wdtFeed();  // Debe estar en loop()
yield();        // Debe estar en loop()
```

#### 4. **Sensor no detecta pulsos:**
```cpp
// Verificar conexión FC-03:
// D2 (GPIO4) ← Signal
// 3.3V ← VCC  
// GND ← GND

// Test en setup():
Serial.println(digitalRead(IR_SENSOR_PIN)); // Debe cambiar 0/1
```

#### 5. **Memoria insuficiente:**
```cpp
// Monitor en tiempo real:
Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());

// Optimizar si < 2000 bytes:
// - Reducir tamaño strings
// - Liberar variables no usadas
// - Simplificar JSON
```

### Logs de Debugging:
```cpp
// Habilitar debug completo:
#define DEBUG_ESP_WIFI
#define DEBUG_ESP_HTTP_SERVER

// Monitor serie a 9600 baudios
// Verificar mensajes de inicialización
```

---

## 🗺️ ROADMAP FUTURO

### Mejoras Inmediatas (Prioridad Alta):
- [ ] **Calibración**: Interfaz para ajustar factores de conversión
- [ ] **Logging**: Almacenamiento histórico de mediciones  
- [ ] **Gráficos**: Visualización temporal de viscosidad
- [ ] **Export**: Descarga de datos en CSV/JSON
- [ ] **WiFi Manager**: Configuración de red sin hardcode

### Funcionalidades Avanzadas (Prioridad Media):
- [ ] **OTA Updates**: Actualización remota de firmware
- [ ] **MQTT**: Integración con IoT platforms
- [ ] **Múltiples sensores**: Soporte para varios FC-03
- [ ] **Temperatura**: Compensación térmica automática
- [ ] **Base de datos**: Almacenamiento en SQLite/InfluxDB

### Optimizaciones Técnicas (Prioridad Baja):
- [ ] **FreeRTOS**: Tasks concurrentes para mejor performance
- [ ] **Deep Sleep**: Modo ahorro energía para operación a batería
- [ ] **Security**: HTTPS y autenticación
- [ ] **Multi-idioma**: Interfaz en inglés/español
- [ ] **Mobile App**: Aplicación nativa iOS/Android

### Hardware Avanzado:
- [ ] **ESP32**: Migración para mayor potencia y memoria
- [ ] **Display**: Pantalla OLED local para mediciones
- [ ] **SD Card**: Almacenamiento local expandido
- [ ] **RTC**: Timestamp preciso independiente
- [ ] **Encoder rotativo**: Control físico de parámetros

---

## 📚 DOCUMENTACIÓN TÉCNICA ADICIONAL

### Referencias Científicas:
- **Documento base**: `VISCOSIMETRO_COUETTE_ESPECIFICACIONES_TECNICAS.txt`
- **Normas ASTM**: D2983, D445 (viscosimetría)
- **ISO Standards**: 3104, 3219 (rheología)

### Datasheets:
- **ESP8266**: [Espressif Official](https://www.espressif.com/sites/default/files/documentation/0a-esp8266ex_datasheet_en.pdf)
- **FC-03**: Sensor infrarrojo especificaciones técnicas
- **PlatformIO**: [Framework Arduino ESP8266](https://docs.platformio.org/en/latest/platforms/espressif8266.html)

### Librerías Utilizadas:
```cpp
#include <ESP8266WiFi.h>          // Core WiFi ESP8266
#include <ESPAsyncTCP.h>          // TCP asíncrono
#include <ESPAsyncWebServer.h>    // Servidor web asíncrono  
#include <LittleFS.h>             // Sistema de archivos
#include <Arduino_JSON.h>         // Manejo JSON (limitado)
```

---

## 👨‍💻 INFORMACIÓN PARA DESARROLLADORES IA

### Contexto de Desarrollo:
Este proyecto fue desarrollado iterativamente con las siguientes fases:
1. **Base original**: Sensor BME280 con WiFi cliente
2. **Simulación**: Valores aleatorios para testing
3. **Sensor real**: Implementación FC-03 infrarrojo
4. **Access Point**: Red independiente
5. **Viscosímetro**: Cálculos según teoría científica
6. **Controles**: Botones de inicio/parada/reset
7. **Estabilización**: Optimizaciones críticas de memoria y interrupciones

### Decisiones de Arquitectura:
- **ESP8266 vs ESP32**: Elegido ESP8266 por simplicidad y costo
- **AsyncWebServer vs WebServer**: Asíncrono para mejor performance
- **LittleFS vs SPIFFS**: LittleFS por ser más moderno y estable  
- **Server-Sent Events vs WebSockets**: SSE por simplicidad unidireccional
- **JSON directo vs JSONVar**: JSON string directo por eficiencia de memoria

### Patrones de Código:
- **ISR minimalista**: Flags en interrupción, procesamiento en loop
- **Memory management**: Liberación explícita de strings
- **Error handling**: Validaciones antes de operaciones críticas
- **Watchdog feeding**: Prevención de resets automáticos
- **Modular functions**: Separación clara de responsabilidades

### Consideraciones de Continuidad:
1. **Mantener estabilidad**: Las optimizaciones actuales son críticas
2. **Testing exhaustivo**: Cada cambio debe probarse por 15+ minutos sin reinicios
3. **Documentar cambios**: Actualizar este README con nuevas funcionalidades
4. **Backward compatibility**: Mantener compatibilidad de API REST
5. **Memory monitoring**: Siempre verificar uso de memoria en cambios

---

## 📞 CONTACTO Y SOPORTE

### Repositorio:
- **GitHub**: https://github.com/rodmicito/es8266Asinctable
- **Rama principal**: `access_point_mode`
- **Issues**: Utilizar GitHub Issues para reportar problemas

### Información de Versión:
- **Versión actual**: 2.0 (access_point_mode)
- **Fecha**: Agosto 2025
- **Compatibilidad**: ESP8266 ESP-12E, PlatformIO, Arduino Framework

---

**🎯 NOTA IMPORTANTE PARA IA CONTINUADORA:**

Este proyecto está **completamente funcional y estable**. La rama `access_point_mode` contiene la implementación más robusta y debe ser el punto de partida para cualquier desarrollo futuro. Todas las optimizaciones de estabilidad son críticas y no deben modificarse sin testing exhaustivo.

**Las próximas mejoras recomendadas están en el Roadmap, priorizadas por impacto y complejidad.**

---

*Documentación completa - Proyecto Viscosímetro Couette ESP8266*  
*Generado automáticamente para continuidad de desarrollo*
