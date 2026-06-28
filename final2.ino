#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <ArduinoJson.h>   // Install: Arduino IDE → Library Manager → ArduinoJson by Benoit Blanchon

// ================= WIFI =================
#define WIFI_NAME     "RISHITH 6644"
#define WIFI_PASSWORD "Rishith@11"

// ─── Server IP — must match your PC running app.py ───────────────────────────
// Run `ipconfig` (Windows) / `ip a` (Linux) to find your LAN IP.
// Must be the same network as the ESP32.


#define URL_DATA      "http://192.168.137.1:5000/api/data"
#define URL_CMD_POLL  "http://192.168.137.1:5000/api/pump_command"
#define URL_CMD_ACK   "http://192.168.137.1:5000/api/pump_command/ack"

// ================= PINS =================
#define SOIL_PIN    34
#define RAIN_PIN    35
#define DHTPIN      18
#define DHTTYPE     DHT11

#define RELAY_PIN   25
#define GREEN_LED   26
#define RED_LED     27

DHT dht(DHTPIN, DHTTYPE);

// ================= CALIBRATION =================
const int drySoil = 3200;
const int wetSoil = 1200;

// ================= STATE =================
bool pumpOn       = false;   // actual hardware state
bool manualMode   = false;   // true when website has overridden auto logic

// ================= WIFI CONNECT =================
void connectWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_NAME, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (++attempts >= 20) {
      Serial.println("\nWiFi failed! Restarting...");
      delay(1000);
      ESP.restart();
    }
  }
  Serial.println("\nWiFi Connected — IP: " + WiFi.localIP().toString());
}

// ================= APPLY PUMP STATE =================
void applyPump(bool on)
{
  pumpOn = on;
  if (on) {
    digitalWrite(RELAY_PIN, LOW);    // Active-LOW relay → Pump ON
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(RED_LED,   LOW);
  } else {
    digitalWrite(RELAY_PIN, HIGH);   // Relay OFF → Pump OFF
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED,   HIGH);
  }
}

// ================= SEND SENSOR DATA =================
void sendDataToServer(int moisturePercent, float temperature, float humidity,
                      String rainStatus, bool pumpState)
{
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost — reconnecting...");
    connectWiFi();
    return;
  }

  HTTPClient http;
  http.setTimeout(5000);
  http.begin(URL_DATA);
  http.addHeader("Content-Type", "application/json");

  // Build JSON payload — key names match app.py arduino_ingest() exactly
  String payload = "{";
  payload += "\"soil_moisture\":"   + String(moisturePercent) + ",";
  payload += "\"temperature\":"     + String(temperature, 1)  + ",";
  payload += "\"humidity\":"        + String(humidity, 1)     + ",";
  payload += "\"rain_pct\":"        + String(rainStatus == "Rain Detected" ? 100 : 0) + ",";
  payload += "\"pump_status\":\""   + String(pumpState ? "ON" : "OFF") + "\"";
  payload += "}";

  int code = http.POST(payload);
  Serial.print("Sensor POST → HTTP "); Serial.println(code);
  http.end();
}

// ================= POLL WEBSITE FOR PUMP COMMAND =================
// Called every loop().  Returns true if a command was received and acted on.
bool checkWebCommand()
{
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  http.setTimeout(4000);
  http.begin(URL_CMD_POLL);

  int code = http.GET();
  if (code != 200) {
    Serial.print("CMD poll → HTTP "); Serial.println(code);
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();

  // Parse JSON  { "command": "ON"|"OFF"|"NONE", "id": 7 }
  StaticJsonDocument<128> doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok) {
    Serial.println("CMD poll: JSON parse error");
    return false;
  }

  const char* cmd = doc["command"] | "NONE";

  if (strcmp(cmd, "NONE") == 0) return false;   // nothing pending

  int cmdId = doc["id"] | -1;
  Serial.print("CMD from web: ");
  Serial.print(cmd);
  Serial.print("  id=");
  Serial.println(cmdId);

  // ── Act on the command ──
  if (strcmp(cmd, "ON") == 0) {
    manualMode = true;
    applyPump(true);
    Serial.println(">>> Pump turned ON by website <<<");
  } else if (strcmp(cmd, "OFF") == 0) {
    manualMode = true;
    applyPump(false);
    Serial.println(">>> Pump turned OFF by website <<<");
  }

  // ── Acknowledge — tell server we acted on it ──
  if (cmdId > 0) {
    HTTPClient ackHttp;
    ackHttp.setTimeout(4000);
    ackHttp.begin(URL_CMD_ACK);
    ackHttp.addHeader("Content-Type", "application/json");
    String ackBody = "{\"id\":" + String(cmdId) +
                     ",\"pump_status\":\"" + String(pumpOn ? "ON" : "OFF") + "\"}";
    int ackCode = ackHttp.POST(ackBody);
    Serial.print("CMD ack → HTTP "); Serial.println(ackCode);
    ackHttp.end();
  }

  return true;
}

// ================= SETUP =================
void setup()
{
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED,   OUTPUT);

  applyPump(false);   // Start with pump OFF
  dht.begin();
  connectWiFi();

  Serial.println("AquaFlow Smart Irrigation System — Started");
  Serial.println("Web command polling: ENABLED  (every loop ~2 s)");
}

// ================= LOOP =================
void loop()
{
  // ── 1. Check for pump commands from the website FIRST ──────────────────
  checkWebCommand();

  // ── 2. Read sensors ────────────────────────────────────────────────────
  long total = 0;
  for (int i = 0; i < 10; i++) { total += analogRead(SOIL_PIN); delay(10); }
  int soilValue = total / 10;
  int rainValue = analogRead(RAIN_PIN);

  float temperature = dht.readTemperature();
  float humidity    = dht.readHumidity();

  int moisturePercent = map(soilValue, drySoil, wetSoil, 0, 100);
  moisturePercent     = constrain(moisturePercent, 0, 100);

  bool   isRaining  = (rainValue < 2000);
  String rainStatus = isRaining ? "Rain Detected" : "No Rain";

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("DHT11 Read Error — skipping cycle.");
    delay(2000);
    return;
  }

  // ── 3. Auto pump logic — only when NOT in manual override mode ─────────
  //    Manual mode is cleared if moisture comes back to mid-range (60–70 %)
  //    so the system can self-recover after a manual session.
  if (!manualMode) {
    if (!pumpOn && moisturePercent < 30 && !isRaining) applyPump(true);
    if ( pumpOn && (moisturePercent >= 70 || isRaining)) applyPump(false);
  }

  // ── 4. Send sensor data to server ──────────────────────────────────────
  sendDataToServer(moisturePercent, temperature, humidity, rainStatus, pumpOn);

  // ── 5. Serial Monitor ──────────────────────────────────────────────────
  Serial.println("================================");
  Serial.print("Mode: ");           Serial.println(manualMode ? "MANUAL" : "AUTO");
  Serial.print("Soil Moisture: ");  Serial.print(moisturePercent); Serial.println("%");
  Serial.print("Rain: ");           Serial.println(rainStatus);
  Serial.print("Temperature: ");    Serial.print(temperature, 1);  Serial.println(" °C");
  Serial.print("Humidity: ");       Serial.print(humidity, 1);     Serial.println(" %");
  Serial.print("Pump: ");           Serial.println(pumpOn ? "ON" : "OFF");
  Serial.println("================================");

  delay(2000);
}
