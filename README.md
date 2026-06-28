# Smart_irrigation_using_IOT_analytics_final
# AquaFlow: Smart Irrigation System

AquaFlow is an ESP32-based automated smart irrigation system designed for environmental monitoring and automated water delivery. It leverages sensor data (soil moisture, temperature, humidity, and rain detection) to control irrigation pumps while exposing a REST API interface for real-time monitoring and web-based manual overrides.

---

## Features

* **Dual Mode Operation**:
* **Autonomous Mode**: Manages water delivery automatically based on preset threshold configurations.
* **Manual Override**: Allows instantaneous web-interface pump control; system automatically returns to autonomous monitoring once target moisture levels stabilize.


* **Sensor Integration**: Interfaces with Analog Soil Moisture sensors, Analog Rain sensors, and a DHT11 Temperature/Humidity sensor.
* **Signal Filtering**: Implements a 10-sample rolling average on raw analog sensor lines to filter out transient noise and prevent erratic pump triggers.
* **Fault Recovery**: Built-in network auto-reconnect logic and software-based watchdog routines to handle endpoint connection failures.
* **REST API Communications**: Handles asynchronous state data transmission via structured JSON payloads.

---

## Hardware Configurations and Pin Mapping

| Component | ESP32 GPIO Pin | Mode / Type | Configuration Notes |
| --- | --- | --- | --- |
| **Soil Moisture Sensor** | `GPIO 34` | Analog Input | Calibrated across 12-bit ADC |
| **Rain Sensor** | `GPIO 35` | Analog Input | Threshold defined below 2000 |
| **DHT11 Sensor** | `GPIO 18` | Digital Input | Requires pull-up configuration |
| **Relay Module (Pump)** | `GPIO 25` | Digital Output | Active-LOW configuration |
| **Status LED (Green)** | `GPIO 26` | Digital Output | High state indicates Pump ON |
| **Status LED (Red)** | `GPIO 27` | Digital Output | High state indicates Pump OFF |

---

## Operational Thresholds

* **Moisture ADC Limits**: Mapped from 3200 (completely dry) to 1200 (completely saturated) equating to 0% - 100%.
* **Activation Target**: Autonomous irrigation initiates when calculated soil moisture drops below 30% and no rain is detected.
* **Deactivation Target**: Autonomous irrigation terminates when soil moisture reaches 70% or if precipitation is detected.

---

## Dependencies

The firmware requires the compilation of the following libraries in the Arduino IDE environment:

* `DHT sensor library` by Adafruit
* `Adafruit Unified Sensor`
* `ArduinoJson` (Version 6 or newer)

---

## Deployment and Setup

1. **Clone the Repository**:
```bash
git clone https://github.com/yourusername/aquaflow-smart-irrigation.git

```


2. **Network Credentials Configuration**:
Open `final2.ino` and update the local Wi-Fi parameters:
```cpp
#define WIFI_NAME     "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

```


3. **Backend Network Mapping**:
Redirect the API endpoints to reflect the IPv4 address of your deployed host machine running the central flask server (`app.py`):
```cpp
#define URL_DATA      "http://<HOST_SERVER_IP>:5000/api/data"
#define URL_CMD_POLL  "http://<HOST_SERVER_IP>:5000/api/pump_command"
#define URL_CMD_ACK   "http://<HOST_SERVER_IP>:5000/api/pump_command/ack"

```


4. **Compilation and Flash**:
Connect the target microcontroller board via USB, designate the target board device (`ESP32 Dev Module`) and selected communication port, and execute the upload routine.

---

## API Documentation

### Telemetry Ingestion (`POST /api/data`)

The device uploads real-time state vectors using the following schema:

```json
{
  "soil_moisture": 45,
  "temperature": 26.5,
  "humidity": 62.0,
  "rain_pct": 0,
  "pump_status": "OFF"
}

```

### Manual Command Request (`GET /api/pump_command`)

Incoming web requests parse the target operational instructions using the payload structure below:

```json
{
  "command": "ON", 
  "id": 7
}

```

### Command Handshake Confirmation (`POST /api/pump_command/ack`)

Acknowledges confirmation of manual overrides to finalize execution threads:

```json
{
  "id": 7,
  "pump_status": "ON"
}

```
