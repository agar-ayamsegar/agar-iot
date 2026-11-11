// ESP32-C3 electrode layer for agar-esp-ble. No setup()/loop() here.
// To use it from agar-esp-ble.ino:
//   setup(): electrodeBegin();
//   loop():  JsonDocument doc = electrodeResponse();
//            -> {"ohm":1180.2,"v_total":3.131,"v_chk":1.687}   same 3 numbers as the logger's data rows
//               ohm -1 = probe not in meat
//
// Wiring (same breadboard as electrode_logger, see design/breadboard_holes.png):
//   ADS1115 VDD->3.3V  GND->GND  SDA->GPIO8  SCL->GPIO9  ADDR->GND  (A1 not used)
//   row 10: GPIO4, A0, resistor leg      row 14: resistor leg, A2, probe 1
//   row 18: GPIO5, A3, probe 2
// Library: Adafruit ADS1X15

#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <ArduinoJson.h>

const int EL_PIN_SDA = 8;
const int EL_PIN_SCL = 9;
const int EL_PIN_A   = 4;
const int EL_PIN_B   = 5;

const float EL_RREF  = 1000.0;   // breadboard resistor (ohm)
const int   EL_PAIRS = 20;       // 20 forward + 20 reverse pulses per reading (~0.25 s)

Adafruit_ADS1115 elAds;
bool  elOk = false;
float elVt = 0, elVc = 0;   // last raw voltages: A0-A3 (resistor + chicken), A2-A3 (chicken only)

// One pulse, current flipped every other call so the pins don't charge up.
static float elPulse(bool forward) {
  digitalWrite(EL_PIN_A, forward ? HIGH : LOW);
  digitalWrite(EL_PIN_B, forward ? LOW  : HIGH);
  delay(3);
  float vt = fabs(elAds.computeVolts(elAds.readADC_Differential_0_3()));  // resistor + chicken
  float vc = fabs(elAds.computeVolts(elAds.readADC_Differential_2_3()));  // chicken only
  digitalWrite(EL_PIN_A, LOW);
  digitalWrite(EL_PIN_B, LOW);
  elVt = vt; elVc = vc;
  float vr = vt - vc;
  if (vr < 0.005) return -1;   // no current: probe not in anything
  return vc / vr * EL_RREF;
}

void electrodeBegin() {
  Wire.begin(EL_PIN_SDA, EL_PIN_SCL);
  elOk = elAds.begin();
  if (!elOk) {
    Serial.println("# ERROR ADS1115 not found - check VDD(3.3V), GND, SDA(8), SCL(9), ADDR->GND");
  } else {
    elAds.setGain(GAIN_ONE);
    elAds.setDataRate(RATE_ADS1115_860SPS);
  }
  pinMode(EL_PIN_A, OUTPUT); pinMode(EL_PIN_B, OUTPUT);
  digitalWrite(EL_PIN_A, LOW); digitalWrite(EL_PIN_B, LOW);
}

// Chicken resistance in ohm, -1 = probe not in meat (or ADS1115 missing).
float electrodeOhm() {
  if (!elOk) return -1;
  float sum = 0;
  for (int i = 0; i < EL_PAIRS; i++) {
    float fwd = elPulse(true);    // named fwd/rev: agar-enose.ino already has r2()
    float rev = elPulse(false);
    if (fwd < 0 || rev < 0) return -1;
    sum += fwd + rev;
  }
  return sum / (2 * EL_PAIRS);
}

JsonDocument electrodeResponse() {
  float ohm = electrodeOhm();
  JsonDocument doc;
  doc["ohm"]     = ohm < 0 ? -1 : roundf(ohm * 10.0) / 10.0;   // R_ohm
  doc["v_total"] = roundf(elVt * 1000.0) / 1000.0;              // volts, A0-A3
  doc["v_chk"]   = roundf(elVc * 1000.0) / 1000.0;              // volts, A2-A3
  return doc;
}
