// ESP32-S3 sensor layer for agar-esp-ble. No setup()/loop() here --
// agar-esp-ble.ino owns those and calls getResponse() once per second.
//
// MQ-137: AO -> R1 10k -> GPIO6 -> R2 10k -> GND
// MQ-135: AO -> R1 10k -> GPIO7 -> R2 10k -> GND
// VCC -> 5V, GND -> GND (both MQ sensors)
// DHT11: VCC -> 3V3 (NOT 5V), GND -> GND, DATA -> GPIO15
//
// Send "c" over Serial to calibrate -- box EMPTY. Until then rel_* is not sent.
// Send "t" for the math self-check.

#include <DHT11.h>
#include <ArduinoJson.h>

const uint8_t PIN_MQ137 = 6;
const uint8_t PIN_MQ135 = 7;
const uint8_t PIN_DHT   = 15;

const float VCC    = 5.0;   // sensor supply (use your multimeter value)
const float RL_137 = 1.0;   // kOhm, load resistor on MQ-137 module ("102")
const float RL_135 = 2.0;   // kOhm, load resistor on MQ-135 module ("202")

const float R1 = 10.0;      // kOhm, divider top    (AO -> GPIO)
const float R2 = 10.0;      // kOhm, divider bottom (GPIO -> GND)

const int N_AVG       = 20;
const int CAL_SAMPLES = 300;            // ~60 s, averages out sensor wiggle
const unsigned long WARMUP_MS = 600000; // 10 min hot before readings mean anything

float rsAir137 = 0, rsAir135 = 0;
bool  calibrated = false;
bool  begun      = false;
unsigned long bootMs = 0;

DHT11 dht11(PIN_DHT);
int  temperature = 0, humidity = 0;
bool dhtOk = false;

// mv = measured at the GPIO pin. Returns Rs in kOhm.
float rsFromPinMv(float mv, float rl) {
  float v = (mv / 1000.0) * (R1 + R2) / R2;        // undo divider -> sensor AO voltage
  if (v < 0.01) v = 0.01;
  if (v > VCC - 0.01) v = VCC - 0.01;              // keep Rs positive if VCC is set too low
  float rl_eff = rl * (R1 + R2) / (rl + R1 + R2);  // module RL in parallel with the divider
  return rl_eff * (VCC - v) / v;
}

// Returns Rs in kOhm, and hands back the averaged raw count through `raw`.
// (No struct: Arduino hoists function prototypes above the file, so a struct
// return type here fails to compile.)
float readMq(uint8_t pin, float rl, int &raw) {
  long rawSum = 0, mvSum = 0;
  for (int i = 0; i < N_AVG; i++) {
    rawSum += analogRead(pin);
    mvSum  += analogReadMilliVolts(pin);   // factory-calibrated: the plain raw*3.3/4095 line
    delay(2);                              // bends at the low end, which is where our baseline sits
  }
  raw = rawSum / N_AVG;
  return rsFromPinMv(mvSum / (float)N_AVG, rl);
}

void enoseBegin() {
  analogReadResolution(12);                       // 0-4095
  analogSetPinAttenuation(PIN_MQ137, ADC_11db);   // ~0-3.1 V (use ADC_12db if your core warns)
  analogSetPinAttenuation(PIN_MQ135, ADC_11db);
  delay(50);
  for (int i = 0; i < 20; i++) { analogRead(PIN_MQ137); analogRead(PIN_MQ135); }  // dummy reads
  bootMs = millis();
  begun  = true;
}

void enoseCalibrate() {
  if (!begun) enoseBegin();
  Serial.println("# calibrating, keep box empty.");
  float sum137 = 0, sum135 = 0;
  int raw;
  for (int i = 0; i < CAL_SAMPLES; i++) {
    sum137 += readMq(PIN_MQ137, RL_137, raw);
    sum135 += readMq(PIN_MQ135, RL_135, raw);
    if (i % 30 == 0) {
      Serial.print("# calibrating ");
      Serial.print(i * 100 / CAL_SAMPLES);
      Serial.println(" %");
    }
    delay(100);
  }
  rsAir137 = sum137 / CAL_SAMPLES;
  rsAir135 = sum135 / CAL_SAMPLES;
  calibrated = rsAir137 > 0 && rsAir135 > 0;

  Serial.print("# rs_air137: ");
  Serial.print(rsAir137, 2);
  Serial.print("  rs_air135: ");
  Serial.println(rsAir135, 2);
  if ((millis() - bootMs) < WARMUP_MS)
    Serial.println("# WARNING: calibrated before the sensors were warm (10 min) -- redo with c");
}

// 358.6 mV at the pin = 0.717 V at the sensor -> 10.86 kOhm on the MQ-135.
// Taken from a real log row, so this fails if the divider math is ever broken.
void enoseSelfCheck() {
  float rs = rsFromPinMv(358.6, RL_135);
  Serial.print("# selfcheck rs_135(358.6 mV) = ");
  Serial.print(rs, 2);
  Serial.println(rs > 10.7 && rs < 11.0 ? " kOhm  OK" : " kOhm  FAIL");
}

void enoseSerial() {
  if (!Serial.available()) return;
  char c = Serial.read();
  if (c == 'c') enoseCalibrate();
  else if (c == 't') enoseSelfCheck();
}

void readTempAndHum() {
  // keeps the last good reading on error; dht_ok says which it is
  dhtOk = dht11.readTemperatureHumidity(temperature, humidity) == 0;
}

static float r2(float x) { return roundf(x * 100.0) / 100.0; }
static float r3(float x) { return roundf(x * 1000.0) / 1000.0; }

// 0 until calibrated -- rsAir is still 0 then, and x/0 serializes as null
static float rel(float rs, float rsAir) { return rsAir > 0 ? r3(rs / rsAir) : 0; }

JsonDocument getResponse() {
  if (!begun) enoseBegin();
  enoseSerial();
  readTempAndHum();

  int raw137, raw135;
  float rs137 = readMq(PIN_MQ137, RL_137, raw137);
  float rs135 = readMq(PIN_MQ135, RL_135, raw135);

  JsonDocument doc;
  doc["mq_137"] = raw137;               // 0-4095 now, not 0-1023
  doc["mq_135"] = raw135;
  doc["rs_137"] = r2(rs137);            // kOhm. WAS the pin voltage under this same key
  doc["rs_135"] = r2(rs135);
  doc["rel_137"] = rel(rs137, rsAir137);     // 0 = not calibrated yet (send "c")
  doc["rel_135"] = rel(rs135, rsAir135);
  doc["temp"]       = temperature;
  doc["humidity"]   = humidity;
  return doc;                           // ~160 bytes; iOS negotiates ~185, so keep it lean
}
