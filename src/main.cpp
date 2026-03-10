#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_LTR390.h>
#include <NimBLEDevice.h>

#define SDA_PIN 21
#define SCL_PIN 22

#define PIN_STEP 25
#define PIN_DIR 26
#define PIN_EN 27

// ===== Motion settings =====
// Most NEMA17 are 200 full steps/rev (1.8° per step).
// If your TMC2209 is set to microstepping (common = 1/16), steps per rev becomes 200*16 = 3200.
// Start with 200 first. If it barely moves, you are in microstepping -> change to 3200.
const int STEPS_PER_REV = 3200; // try 200 first, then 3200 if needed

// Step pulse timing (microseconds). Smaller = faster.
// 800 us HIGH + 800 us LOW => ~625 steps/sec. Safe starter speed.
const int PULSE_US = 800;

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

// ------------------------------------ HELPER FUNCTIONS
// UV and ALS reading functions
uint32_t readUVS(bool test = false);
uint32_t readALS(bool test = false);

// Communication functions
void bluetoothSetup();
uint32_t readCmd();
bool parseCSV(const String &s, float &x, float &y);

// Blind control functions
void openBlind();
void closeBlind();
void incrementBlind(uint32_t step);
void decrementBlind(uint32_t step);
uint32_t readBlindPosition();

// ------------------------------------ VARIABLES
// LTR390 sensor
Adafruit_LTR390 ltr;
// State
bool manualMode = true;

// Communication
NimBLECharacteristic *txChar = nullptr;
// Nordic UART Service (NUS) UUIDs
static const char *SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
static const char *CHAR_UUID_RX_WRITE = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";  // Pi -> ESP32 (Write)
static const char *CHAR_UUID_TX_NOTIFY = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"; // ESP32 -> Pi (Notify)

class RXCallbacks : public NimBLECharacteristicCallbacks
{
public:
  // the base class’s onWrite isn’t marked override in this library
  // version, so don’t use the specifier here – the function is still
  // virtual and will be called.
  void onWrite(NimBLECharacteristic *ch)
  {
    std::string v = ch->getValue();
    String msg(v.c_str());
    msg.trim(); // removes \r \n spaces

    float x = 0, y = 0;
    if (parseCSV(msg, x, y))
    {
      Serial.print("Got coords -> X: ");
      Serial.print(x, 3);
      Serial.print("  Y: ");
      Serial.println(y, 3);

      // Optional ACK back to Pi
      if (txChar)
      {
        char ack[64];
        snprintf(ack, sizeof(ack), "ACK %.3f,%.3f", x, y);
        txChar->setValue((uint8_t *)ack, strlen(ack));
        txChar->notify();
      }
    }
    else
    {
      Serial.print("Bad format: ");
      Serial.println(msg);

      if (txChar)
      {
        const char *err = "ERR format, use x,y";
        txChar->setValue((uint8_t *)err, strlen(err));
        txChar->notify();
      }
    }
  }
};

class ServerCallbacks : public NimBLEServerCallbacks
{
  void onConnect(NimBLEServer *)
  {
    Serial.println("Pi connected");
  }
  void onDisconnect(NimBLEServer *)
  {
    Serial.println("Pi disconnected, advertising again...");
    NimBLEDevice::startAdvertising();
  }
};

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

  bluetoothSetup();

  // motor pins
  pinMode(PIN_STEP, OUTPUT);
  pinMode(PIN_DIR, OUTPUT);
  pinMode(PIN_EN, OUTPUT);
  digitalWrite(PIN_EN, LOW);
  delay(500);
}

void loop()
{
  uint32_t cmd = readCmd();
  if (cmd == 1)
  {
    Serial.println("Toggle Mode");
    manualMode = !manualMode;
  }

  // 1 revolution forward
  Serial.println("Moving forward 1 rev");
  moveSteps(STEPS_PER_REV, true);
  delay(1000);

  // 1 revolution backward
  Serial.println("Moving backward 1 rev");
  moveSteps(STEPS_PER_REV, false);
  delay(1000);

  readUVS(true);
  readALS(true);
  if (manualMode)
  {
    // Serial.println("Manual Mode");
    switch (cmd)
    {
    case 2:
      Serial.println("Open Blind");
      break;
    case 3:
      Serial.println("Close Blind");
      break;
    case 4:
      Serial.println("Increment Blind");
      break;
    case 5:
      Serial.println("Decrement Blind");
      break;

    default:
      break;
    }
  }
  else
  {
    // Serial.println("Automatic Mode");
    readUVS();
    readALS();
  }

  delay(1000);
}

// ------------------------------------ FUNCTION DEFINITIONS
uint32_t readUVS(bool test)
{
  // ---- UV ----
  ltr.setMode(LTR390_MODE_UVS);
  delay(500);

  uint32_t uv_raw = ltr.readUVS();

  if (test)
  {
    Serial.print("UV Raw: ");
    Serial.println(uv_raw);
  }
  return uv_raw;
}
uint32_t readALS(bool test)
{
  // ---- ALS (visible light) ----
  ltr.setMode(LTR390_MODE_ALS);
  delay(200);

  uint32_t als_raw = ltr.readALS();

  if (test)
  {
    Serial.print("ALS Raw: ");
    Serial.println(als_raw);
  }
  return als_raw;
}

void bluetoothSetup()
{
  NimBLEDevice::init("ESP32_COORD");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9); // strong TX, optional

  NimBLEServer *server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  NimBLEService *svc = server->createService(SERVICE_UUID);

  txChar = svc->createCharacteristic(CHAR_UUID_TX_NOTIFY, NIMBLE_PROPERTY::NOTIFY);

  NimBLECharacteristic *rxChar = svc->createCharacteristic(
      CHAR_UUID_RX_WRITE,
      NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  rxChar->setCallbacks(new RXCallbacks());

  svc->start();

  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->start();

  Serial.println("Advertising as ESP32_COORD (BLE UART)");
}
uint32_t readCmd()
{
  // In a real application, this would read from a command source (e.g., UART, BLE)
  static uint32_t cmd = 0;

  return cmd;
}

bool parseCSV(const String &s, float &x, float &y)
{
  int comma = s.indexOf(',');
  if (comma < 0)
    return false;

  String xs = s.substring(0, comma);
  String ys = s.substring(comma + 1);

  xs.trim();
  ys.trim();
  if (xs.length() == 0 || ys.length() == 0)
    return false;

  x = xs.toFloat();
  y = ys.toFloat();
  return true;
}

/*
#include <Arduino.h>
#include <NimBLEDevice.h>

static const char *SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
static const char *CHAR_UUID_RX_WRITE = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";  // Pi -> ESP32
static const char *CHAR_UUID_TX_NOTIFY = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"; // ESP32 -> Pi

NimBLECharacteristic *txChar = nullptr;

class ServerCallbacks : public NimBLEServerCallbacks
{
  void onConnect(NimBLEServer *)
  {
    Serial.println("BLE: CONNECTED");
  }
  void onDisconnect(NimBLEServer *)
  {
    Serial.println("BLE: DISCONNECTED -> advertising again");
    NimBLEDevice::startAdvertising();
  }
};

class RXCallbacks : public NimBLECharacteristicCallbacks
{
  void onWrite(NimBLECharacteristic *ch)
  {
    std::string v = ch->getValue();

    Serial.print("BLE RX (");
    Serial.print((int)v.size());
    Serial.print(" bytes): ");

    for (size_t i = 0; i < v.size(); i++)
      Serial.write(v[i]);
    Serial.println();

    // ACK back to Pi
    if (txChar)
    {
      txChar->setValue((uint8_t *)v.data(), v.size());
      txChar->notify();
    }
  }
};

void setup()
{
  Serial.begin(115200);
  delay(300);
  Serial.println("BOOT: Serial OK");

  NimBLEDevice::init("ESP32_COORD");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  NimBLEDevice::setMTU(185);

  NimBLEServer *server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  NimBLEService *svc = server->createService(SERVICE_UUID);

  txChar = svc->createCharacteristic(CHAR_UUID_TX_NOTIFY, NIMBLE_PROPERTY::NOTIFY);

  NimBLECharacteristic *rxChar = svc->createCharacteristic(
      CHAR_UUID_RX_WRITE,
      NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  rxChar->setCallbacks(new RXCallbacks());

  svc->start();

  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->start();

  Serial.println("BOOT: Advertising started");
}

void loop()
{
  static uint32_t t0 = 0;
  if (millis() - t0 > 1000)
  {
    t0 = millis();
    Serial.println("HB: alive");
  }
}
  */