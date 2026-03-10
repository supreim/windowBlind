#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <Adafruit_LTR390.h>

#define SDA_PIN 21
#define SCL_PIN 22

#define PIN_STEP 25
#define PIN_DIR 26
#define PIN_EN 27

// ================= WIFI =================
const char *ssid = "CWC-9053175";
const char *password = "Kv3wkcnf6Phb";

// ================= WEBSOCKET =================
WebSocketsServer webSocket = WebSocketsServer(81);

// ================= SENSOR =================
Adafruit_LTR390 ltr;

// ================= MOTOR =================
// If using 1/16 microstepping with 200-step motor:
const int STEPS_PER_REV = 3200;
const int PULSE_US = 800;

// Blind travel range in motor steps
// Change this after testing your full open-to-close travel.
const long MAX_BLIND_STEPS = 20000;

// ================= STATE =================
struct DeviceState
{
  bool automode = false;
  uint32_t als = 0;
  uint32_t uvs = 0;

  long currentSteps = 0; // actual current motor step position
  int motorPosition = 0; // 0 to 100 percent
};

DeviceState state;

// ================= TIMERS =================
unsigned long lastSensorReadMs = 0;
unsigned long lastBroadcastMs = 0;

// ------------------------------------ MOTOR FUNCTIONS
void stepOnce()
{
  digitalWrite(PIN_STEP, HIGH);
  delayMicroseconds(PULSE_US);
  digitalWrite(PIN_STEP, LOW);
  delayMicroseconds(PULSE_US);
}

void moveSteps(long steps, bool dir)
{
  digitalWrite(PIN_DIR, dir ? HIGH : LOW);

  for (long i = 0; i < steps; i++)
  {
    stepOnce();
  }
}

void updateMotorPercentFromSteps()
{
  long clamped = constrain(state.currentSteps, 0L, MAX_BLIND_STEPS);
  state.currentSteps = clamped;
  state.motorPosition = (int)((100.0 * clamped) / MAX_BLIND_STEPS);
}

void moveToPercent(int targetPercent)
{
  targetPercent = constrain(targetPercent, 0, 100);

  long targetSteps = (long)((targetPercent / 100.0) * MAX_BLIND_STEPS);
  targetSteps = constrain(targetSteps, 0L, MAX_BLIND_STEPS);

  long delta = targetSteps - state.currentSteps;

  if (delta == 0)
    return;

  bool dir = (delta > 0);
  long stepsToMove = labs(delta);

  moveSteps(stepsToMove, dir);

  state.currentSteps = targetSteps;
  updateMotorPercentFromSteps();
}

void incrementBlind(uint32_t stepAmount)
{
  long target = state.currentSteps + stepAmount;
  target = constrain(target, 0L, MAX_BLIND_STEPS);

  long delta = target - state.currentSteps;
  if (delta > 0)
  {
    moveSteps(delta, true);
    state.currentSteps = target;
    updateMotorPercentFromSteps();
  }
}

void decrementBlind(uint32_t stepAmount)
{
  long target = state.currentSteps - stepAmount;
  target = constrain(target, 0L, MAX_BLIND_STEPS);

  long delta = state.currentSteps - target;
  if (delta > 0)
  {
    moveSteps(delta, false);
    state.currentSteps = target;
    updateMotorPercentFromSteps();
  }
}

void openBlind()
{
  moveToPercent(100);
}

void closeBlind()
{
  moveToPercent(0);
}

// ------------------------------------ SENSOR FUNCTIONS
uint32_t readUVS(bool test = false)
{
  ltr.setMode(LTR390_MODE_UVS);
  delay(50);
  uint32_t uv_raw = ltr.readUVS();

  if (test)
  {
    Serial.print("UV Raw: ");
    Serial.println(uv_raw);
  }
  delay(200);

  return uv_raw;
}

uint32_t readALS(bool test = false)
{
  ltr.setMode(LTR390_MODE_ALS);
  delay(50);
  uint32_t als_raw = ltr.readALS();

  if (test)
  {
    Serial.print("ALS Raw: ");
    Serial.println(als_raw);
  }
  delay(200);

  return als_raw;
}

// ------------------------------------ JSON / WS FUNCTIONS
String makeStateJson()
{
  JsonDocument doc;
  doc["type"] = "state";
  doc["motorPosition"] = state.motorPosition;
  doc["currentSteps"] = state.currentSteps;
  doc["als"] = state.als;
  doc["uvs"] = state.uvs;
  doc["automode"] = state.automode;

  String out;
  serializeJson(doc, out);
  return out;
}

void broadcastState()
{
  String payload = makeStateJson();
  webSocket.broadcastTXT(payload);
  Serial.print("Broadcast: ");
  Serial.println(payload);
}

void sendStateToClient(uint8_t clientNum)
{
  String payload = makeStateJson();
  webSocket.sendTXT(clientNum, payload);
}

void handleCommand(const String &msg, uint8_t clientNum)
{
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, msg);

  if (err)
  {
    Serial.print("Bad JSON: ");
    Serial.println(msg);

    JsonDocument errDoc;
    errDoc["type"] = "error";
    errDoc["message"] = "Invalid JSON";

    String out;
    serializeJson(errDoc, out);
    webSocket.sendTXT(clientNum, out);
    return;
  }

  const char *type = doc["type"];
  if (!type)
    return;

  String cmd = String(type);

  if (cmd == "requestState")
  {
    sendStateToClient(clientNum);
    return;
  }

  if (cmd == "setAutomode")
  {
    state.automode = doc["value"] | false;
    broadcastState();
    return;
  }

  if (cmd == "setMotorPosition")
  {
    int value = doc["value"] | state.motorPosition;
    moveToPercent(value);
    broadcastState();
    return;
  }

  if (cmd == "openBlind")
  {
    openBlind();
    broadcastState();
    return;
  }

  if (cmd == "closeBlind")
  {
    closeBlind();
    broadcastState();
    return;
  }

  if (cmd == "incrementBlind")
  {
    uint32_t value = doc["value"] | 500;
    incrementBlind(value);
    broadcastState();
    return;
  }

  if (cmd == "decrementBlind")
  {
    uint32_t value = doc["value"] | 500;
    decrementBlind(value);
    broadcastState();
    return;
  }
}

// ------------------------------------ WIFI / WS SETUP
void connectWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("ESP IP address: ");
  Serial.println(WiFi.localIP());
  WiFi.setHostname("smartblind");
}

void onWebSocketEvent(uint8_t clientNum, WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
  case WStype_DISCONNECTED:
    Serial.printf("Client %u disconnected\n", clientNum);
    break;

  case WStype_CONNECTED:
  {
    IPAddress ip = webSocket.remoteIP(clientNum);
    Serial.printf("Client %u connected from %d.%d.%d.%d\n",
                  clientNum, ip[0], ip[1], ip[2], ip[3]);
    sendStateToClient(clientNum);
    break;
  }

  case WStype_TEXT:
  {
    String msg = String((char *)payload).substring(0, length);
    Serial.print("WS RX: ");
    Serial.println(msg);
    handleCommand(msg, clientNum);
    break;
  }

  default:
    break;
  }
}

// ------------------------------------ SETUP / LOOP
void setup()
{
  Serial.begin(115200);
  delay(200);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);

  if (!ltr.begin())
  {
    Serial.println("LTR390 not found!");
    while (1)
      delay(10);
  }

  ltr.setGain(LTR390_GAIN_18);
  ltr.setResolution(LTR390_RESOLUTION_20BIT);

  pinMode(PIN_STEP, OUTPUT);
  pinMode(PIN_DIR, OUTPUT);
  pinMode(PIN_EN, OUTPUT);

  digitalWrite(PIN_EN, LOW); // enable driver
  delay(500);

  connectWiFi();

  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);

  state.currentSteps = 0;
  updateMotorPercentFromSteps();

  Serial.println("WebSocket server started on port 81");
}

bool readUVNext = true;
unsigned long lastSensorMs = 0;
void loop()
{
  webSocket.loop();

  // incrementBlind(1000);
  // if (state.motorPosition >= 100)
  //  decrementBlind(500);

  // read sensors every 500 ms
  if (millis() - lastSensorMs >= 300)
  {
    lastSensorMs = millis();

    if (readUVNext)
    {
      ltr.setMode(LTR390_MODE_UVS);
      delay(150);
      state.uvs = ltr.readUVS();
    }
    else
    {
      ltr.setMode(LTR390_MODE_ALS);
      delay(150);
      state.als = ltr.readALS();
    }

    readUVNext = !readUVNext;
    broadcastState();
  }

  // example simple automode
  if (state.automode)
  {
    // adjust this logic to match your real project idea
    if (state.als > 50000 && state.motorPosition < 100)
    {
      incrementBlind(200);
    }
    else if (state.als < 10000 && state.motorPosition > 0)
    {
      decrementBlind(200);
    }
  }

  // send state every 1 second so website stays updated
  if (millis() - lastBroadcastMs >= 1000)
  {
    lastBroadcastMs = millis();
    broadcastState();
  }
}