/*
  ESP32-Based Washing Machine Control System
  Platform: ESP32 DevKit
  Language: C/C++ (Arduino framework)

  Manages a multi-stage wash cycle (Fill -> Wash -> Rinse -> Spin) via a
  state machine, and exposes a lightweight WiFi web server + JSON status
  endpoint so the cycle can be monitored and controlled remotely from a
  phone browser.

  Wiring (relay-driven, adjust to your build):
    Water inlet valve relay : GPIO 16
    Drain pump relay        : GPIO 17
    Wash motor relay        : GPIO 18
    Spin motor relay        : GPIO 19
    Water level sensor (ADC): GPIO 34
    Door lock relay         : GPIO 21
*/

#include <WiFi.h>
#include <WebServer.h>

// ---------- WiFi credentials ----------
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

WebServer server(80);

// ---------- Pins ----------
const uint8_t PIN_INLET_VALVE = 16;
const uint8_t PIN_DRAIN_PUMP  = 17;
const uint8_t PIN_WASH_MOTOR  = 18;
const uint8_t PIN_SPIN_MOTOR  = 19;
const uint8_t PIN_DOOR_LOCK   = 21;
const uint8_t PIN_WATER_LEVEL = 34;

// ---------- Cycle state machine ----------
enum CycleState {
  IDLE,
  FILLING,
  WASHING,
  DRAINING_WASH,
  RINSE_FILLING,
  RINSING,
  DRAINING_RINSE,
  SPINNING,
  DONE
};

CycleState state = IDLE;
unsigned long stateStartMs = 0;

// Stage durations (ms) - tune to real hardware
const unsigned long WASH_DURATION_MS   = 8UL * 60 * 1000;
const unsigned long RINSE_DURATION_MS  = 5UL * 60 * 1000;
const unsigned long SPIN_DURATION_MS   = 3UL * 60 * 1000;
const int TARGET_WATER_LEVEL           = 2400; // ADC threshold, calibrate

void setRelay(uint8_t pin, bool on) { digitalWrite(pin, on ? HIGH : LOW); }

void allOff() {
  setRelay(PIN_INLET_VALVE, false);
  setRelay(PIN_DRAIN_PUMP, false);
  setRelay(PIN_WASH_MOTOR, false);
  setRelay(PIN_SPIN_MOTOR, false);
}

void enterState(CycleState s) {
  state = s;
  stateStartMs = millis();
  Serial.printf("[cycle] -> state %d\n", (int)s);
}

void startCycle() {
  if (state == IDLE || state == DONE) {
    setRelay(PIN_DOOR_LOCK, true);
    enterState(FILLING);
  }
}

void stopCycle() {
  allOff();
  setRelay(PIN_DOOR_LOCK, false);
  enterState(IDLE);
}

bool waterLevelReached() {
  return analogRead(PIN_WATER_LEVEL) >= TARGET_WATER_LEVEL;
}

void runStateMachine() {
  unsigned long elapsed = millis() - stateStartMs;

  switch (state) {
    case IDLE:
      break;

    case FILLING:
      setRelay(PIN_INLET_VALVE, true);
      if (waterLevelReached()) {
        setRelay(PIN_INLET_VALVE, false);
        enterState(WASHING);
      }
      break;

    case WASHING:
      setRelay(PIN_WASH_MOTOR, true);
      if (elapsed >= WASH_DURATION_MS) {
        setRelay(PIN_WASH_MOTOR, false);
        enterState(DRAINING_WASH);
      }
      break;

    case DRAINING_WASH:
      setRelay(PIN_DRAIN_PUMP, true);
      if (!waterLevelReached()) { // level dropped below threshold = drained
        setRelay(PIN_DRAIN_PUMP, false);
        enterState(RINSE_FILLING);
      }
      break;

    case RINSE_FILLING:
      setRelay(PIN_INLET_VALVE, true);
      if (waterLevelReached()) {
        setRelay(PIN_INLET_VALVE, false);
        enterState(RINSING);
      }
      break;

    case RINSING:
      setRelay(PIN_WASH_MOTOR, true);
      if (elapsed >= RINSE_DURATION_MS) {
        setRelay(PIN_WASH_MOTOR, false);
        enterState(DRAINING_RINSE);
      }
      break;

    case DRAINING_RINSE:
      setRelay(PIN_DRAIN_PUMP, true);
      if (!waterLevelReached()) {
        setRelay(PIN_DRAIN_PUMP, false);
        enterState(SPINNING);
      }
      break;

    case SPINNING:
      setRelay(PIN_SPIN_MOTOR, true);
      if (elapsed >= SPIN_DURATION_MS) {
        setRelay(PIN_SPIN_MOTOR, false);
        enterState(DONE);
      }
      break;

    case DONE:
      setRelay(PIN_DOOR_LOCK, false);
      break;
  }
}

const char* stateName(CycleState s) {
  switch (s) {
    case IDLE: return "IDLE";
    case FILLING: return "FILLING";
    case WASHING: return "WASHING";
    case DRAINING_WASH: return "DRAINING_WASH";
    case RINSE_FILLING: return "RINSE_FILLING";
    case RINSING: return "RINSING";
    case DRAINING_RINSE: return "DRAINING_RINSE";
    case SPINNING: return "SPINNING";
    case DONE: return "DONE";
  }
  return "UNKNOWN";
}

// ---------- Web server handlers ----------
void handleRoot() {
  String html =
    "<html><body style='font-family:sans-serif'>"
    "<h2>Washing Machine Control</h2>"
    "<p>State: <b>" + String(stateName(state)) + "</b></p>"
    "<button onclick=\"fetch('/start')\">Start</button> "
    "<button onclick=\"fetch('/stop')\">Stop</button>"
    "<script>setInterval(()=>location.reload(),5000);</script>"
    "</body></html>";
  server.send(200, "text/html", html);
}

void handleStatus() {
  String json = "{";
  json += "\"state\":\"" + String(stateName(state)) + "\",";
  json += "\"elapsed_ms\":" + String(millis() - stateStartMs) + ",";
  json += "\"water_level\":" + String(analogRead(PIN_WATER_LEVEL));
  json += "}";
  server.send(200, "application/json", json);
}

void handleStart() { startCycle(); server.send(200, "text/plain", "started"); }
void handleStop()  { stopCycle();  server.send(200, "text/plain", "stopped"); }

void setup() {
  Serial.begin(115200);

  pinMode(PIN_INLET_VALVE, OUTPUT);
  pinMode(PIN_DRAIN_PUMP, OUTPUT);
  pinMode(PIN_WASH_MOTOR, OUTPUT);
  pinMode(PIN_SPIN_MOTOR, OUTPUT);
  pinMode(PIN_DOOR_LOCK, OUTPUT);
  allOff();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/start", handleStart);
  server.on("/stop", handleStop);
  server.begin();

  enterState(IDLE);
}

void loop() {
  server.handleClient();
  runStateMachine();
}
