// // ESP32-S3 sensor layer for agar-esp-ble. No setup()/loop() here --
// // agar-esp-ble.ino owns those and calls getResponse() once per second.
// //
// // MQ-137: AO -> R1 10k -> GPIO6 -> R2 10k -> GND
// // MQ-135: AO -> R1 10k -> GPIO7 -> R2 10k -> GND
// // VCC -> 5V, GND -> GND (both MQ sensors)
// // DHT11: VCC -> 3V3 (NOT 5V), GND -> GND, DATA -> GPIO15
// //
// // Send "c" over Serial to calibrate -- box EMPTY. Until then rel_* is not sent.
// // Send "t" for the math self-check.

// #include <DHT11.h>
// #include <ArduinoJson.h>

// const uint8_t PIN_MQ137 = 6;
// const uint8_t PIN_MQ135 = 7;
// const uint8_t PIN_DHT   = 15;

// const float VCC    = 5.0;   // sensor supply (use your multimeter value)
// const float RL_137 = 1.0;   // kOhm, load resistor on MQ-137 module ("102")
// const float RL_135 = 2.0;   // kOhm, load resistor on MQ-135 module ("202")

// const float R1 = 10.0;      // kOhm, divider top    (AO -> GPIO)
// const float R2 = 10.0;      // kOhm, divider bottom (GPIO -> GND)

// // UNFRESH when the MQ-135 resistance drops below this fraction of its clean-box
// // value. Swept over 71 runs in agar-data-collection (A01-A17, railed runs dropped):
// // FRESH median rel_135 0.76, UNFRESH 0.21; 0.45 is the best split at 88.7 % (4 FP,
// // 4 FN, every miss at smell_level 2-3). MQ-137 peaked at 81.7 %, so it is not used.
// // ponytail: single fixed threshold, retune here if the box or sensors change.
// const float UNFRESH_REL_135 = 0.45;

// const int N_AVG       = 20;
// const int CAL_SAMPLES = 300;            // ~60 s, averages out sensor wiggle
// const unsigned long WARMUP_MS = 600000; // 10 min hot before readings mean anything

// float rsAir137 = 0, rsAir135 = 0;
// bool  calibrated = false;
// bool  begun      = false;
// unsigned long bootMs = 0;

// DHT11 dht11(PIN_DHT);
// int  temperature = 0, humidity = 0;
// bool dhtOk = false;

// // mv = measured at the GPIO pin. Returns Rs in kOhm.
// float rsFromPinMv(float mv, float rl) {
//   float v = (mv / 1000.0) * (R1 + R2) / R2;        // undo divider -> sensor AO voltage
//   if (v < 0.01) v = 0.01;
//   if (v > VCC - 0.01) v = VCC - 0.01;              // keep Rs positive if VCC is set too low
//   float rl_eff = rl * (R1 + R2) / (rl + R1 + R2);  // module RL in parallel with the divider
//   return rl_eff * (VCC - v) / v;
// }

// // Returns Rs in kOhm, and hands back the averaged raw count through `raw`.
// // (No struct: Arduino hoists function prototypes above the file, so a struct
// // return type here fails to compile.)
// float readMq(uint8_t pin, float rl, int &raw) {
//   long rawSum = 0, mvSum = 0;
//   for (int i = 0; i < N_AVG; i++) {
//     rawSum += analogRead(pin);
//     mvSum  += analogReadMilliVolts(pin);   // factory-calibrated: the plain raw*3.3/4095 line
//     delay(2);                              // bends at the low end, which is where our baseline sits
//   }
//   raw = rawSum / N_AVG;
//   return rsFromPinMv(mvSum / (float)N_AVG, rl);
// }

// void enoseBegin() {
//   analogReadResolution(12);                       // 0-4095
//   analogSetPinAttenuation(PIN_MQ137, ADC_11db);   // ~0-3.1 V (use ADC_12db if your core warns)
//   analogSetPinAttenuation(PIN_MQ135, ADC_11db);
//   delay(50);
//   for (int i = 0; i < 20; i++) { analogRead(PIN_MQ137); analogRead(PIN_MQ135); }  // dummy reads
//   bootMs = millis();
//   begun  = true;
// }

// void enoseCalibrate() {
//   if (!begun) enoseBegin();
//   Serial.println("# calibrating, keep box empty.");
//   float sum137 = 0, sum135 = 0;
//   int raw;
//   for (int i = 0; i < CAL_SAMPLES; i++) {
//     sum137 += readMq(PIN_MQ137, RL_137, raw);
//     sum135 += readMq(PIN_MQ135, RL_135, raw);
//     if (i % 30 == 0) {
//       Serial.print("# calibrating ");
//       Serial.print(i * 100 / CAL_SAMPLES);
//       Serial.println(" %");
//     }
//     delay(100);
//   }
//   rsAir137 = sum137 / CAL_SAMPLES;
//   rsAir135 = sum135 / CAL_SAMPLES;
//   calibrated = rsAir137 > 0 && rsAir135 > 0;

//   Serial.print("# rs_air137: ");
//   Serial.print(rsAir137, 2);
//   Serial.print("  rs_air135: ");
//   Serial.println(rsAir135, 2);
//   if ((millis() - bootMs) < WARMUP_MS)
//     Serial.println("# WARNING: calibrated before the sensors were warm (10 min) -- redo with c");
// }

// #if DUMMY_MODE
// // Replays agar-dummy.ino instead of reading the ADC. Those rows are 10-bit counts
// // at a 5 V reference, logged on an Uno that read AO directly -- the module's own RL
// // was the only load. This board also hangs the 20k divider off AO, so for the same
// // sensor resistance AO sits lower here. Recover Rs the way the Uno would have, then
// // re-derive what this board's pin sees; everything downstream then runs unchanged.
// static float dummyPinMv(uint16_t raw, float rl) {
//   float vUno = raw / 1023.0 * VCC;                       // Uno saw AO undivided
//   if (vUno < 0.01) vUno = 0.01;
//   if (vUno > VCC - 0.01) vUno = VCC - 0.01;
//   float rs = rl * (VCC - vUno) / vUno;                   // true sensor resistance
//   float rlEff = rl * (R1 + R2) / (rl + R1 + R2);         // RL once our divider loads it
//   return VCC * rlEff / (rs + rlEff) * 1000.0 * R2 / (R1 + R2);
// }

// static void dummyRead(int &raw137, float &rs137, int &raw135, float &rs135) {
//   const uint16_t *row = DUMMY_ROWS[(millis() / DUMMY_ROW_MS) % DUMMY_N];
//   float mv137 = dummyPinMv(row[0], RL_137), mv135 = dummyPinMv(row[1], RL_135);

//   rs137 = rsFromPinMv(mv137, RL_137);
//   rs135 = rsFromPinMv(mv135, RL_135);
//   raw137 = mv137 / 3300.0 * 4095;        // what this board's 12-bit ADC would have reported
//   raw135 = mv135 / 3300.0 * 4095;
//   temperature = row[2];
//   humidity    = row[3];
//   dhtOk       = true;
//   isObjectExist = row[4];                // the table drives "chicken" too

//   rsAir137 = rsFromPinMv(dummyPinMv(DUMMY_BASE_137, RL_137), RL_137);   // stands in for "c"
//   rsAir135 = rsFromPinMv(dummyPinMv(DUMMY_BASE_135, RL_135), RL_135);
//   calibrated = true;
// }
// #endif

// static bool isUnfresh(float rel135) { return rel135 > 0 && rel135 < UNFRESH_REL_135; }

// // 358.6 mV at the pin = 0.717 V at the sensor -> 10.86 kOhm on the MQ-135.
// // Taken from a real log row, so this fails if the divider math is ever broken.
// // The two rel_135 values are real plateaus from A08 (h2 FRESH, h7 UNFRESH).
// void enoseSelfCheck() {
//   float rs = rsFromPinMv(358.6, RL_135);
//   Serial.print("# selfcheck rs_135(358.6 mV) = ");
//   Serial.print(rs, 2);
//   Serial.println(rs > 10.7 && rs < 11.0 ? " kOhm  OK" : " kOhm  FAIL");

//   bool ok = !isUnfresh(0.799) && isUnfresh(0.247) && !isUnfresh(0);  // 0 = uncalibrated
//   Serial.println(ok ? "# selfcheck unfresh  OK" : "# selfcheck unfresh  FAIL");

// #if DUMMY_MODE
//   // Pins the last row of all four phases: both empty-box ones sit at ~1.0, the
//   // FRESH plateau stays above the threshold, the UNFRESH one drops below it.
//   float air = rsFromPinMv(dummyPinMv(DUMMY_BASE_135, RL_135), RL_135);
//   float p[4];
//   for (int i = 0; i < 4; i++)
//     p[i] = rsFromPinMv(dummyPinMv(DUMMY_ROWS[(i + 1) * DUMMY_SEG_N - 1][1], RL_135), RL_135) / air;
//   Serial.print("# selfcheck dummy rel_135 base/fresh/base/unfresh = ");
//   for (int i = 0; i < 4; i++) { Serial.print(p[i], 3); Serial.print(i < 3 ? " / " : ""); }
//   bool phasesOk = p[0] > 0.95 && p[0] < 1.05 && p[2] > 0.95 && p[2] < 1.05
//                   && !isUnfresh(p[1]) && isUnfresh(p[3]);
//   Serial.println(phasesOk ? "  OK" : "  FAIL");
// #endif
// }

// void enoseSerial() {
//   if (!Serial.available()) return;
//   char c = Serial.read();
//   if (c == 'c') enoseCalibrate();
//   else if (c == 't') enoseSelfCheck();
// }

// void readTempAndHum() {
//   // keeps the last good reading on error; dht_ok says which it is
//   dhtOk = dht11.readTemperatureHumidity(temperature, humidity) == 0;
// }

// static float r2(float x) { return roundf(x * 100.0) / 100.0; }
// static float r3(float x) { return roundf(x * 1000.0) / 1000.0; }

// // 0 until calibrated -- rsAir is still 0 then, and x/0 serializes as null
// static float rel(float rs, float rsAir) { return rsAir > 0 ? r3(rs / rsAir) : 0; }

// JsonDocument getResponse() {
//   if (!begun) enoseBegin();
//   enoseSerial();

//   int raw137, raw135;
//   float rs137, rs135;
// #if DUMMY_MODE
//   dummyRead(raw137, rs137, raw135, rs135);
// #else
//   readTempAndHum();
//   rs137 = readMq(PIN_MQ137, RL_137, raw137);
//   rs135 = readMq(PIN_MQ135, RL_135, raw135);
// #endif

//   JsonDocument doc;
//   doc["mq_137"] = raw137;               // 0-4095 now, not 0-1023
//   doc["mq_135"] = raw135;
//   doc["rs_137"] = r2(rs137);            // kOhm. WAS the pin voltage under this same key
//   doc["rs_135"] = r2(rs135);
//   doc["rel_137"] = rel(rs137, rsAir137);     // 0 = not calibrated yet (send "c")
//   float rel135 = rel(rs135, rsAir135);
//   doc["rel_135"] = rel135;
//   doc["unfresh"] = isUnfresh(rel135);        // false while uncalibrated -- rel135 is 0 then
//   doc["temp"]       = temperature;
//   doc["humidity"]   = humidity;
//   return doc;                           // ~160 bytes; iOS negotiates ~185, so keep it lean
// }
