#include <HCSR04.h>


// ultrasonic
const byte trigPin = 2; // pin 2
const byte echoPin = 1; // pin 1
UltraSonicDistanceSensor sensor(trigPin, echoPin);

// fan
const int fanInA = 42; // pin 42
const int fanInB = 41; // pin 41
const int maxFanSpeed = 140; // speed interval is 0...255, 140 is sweet spot, lower if you want

void fan(int speed) {
  speed = constrain(speed, -maxFanSpeed, maxFanSpeed);
  analogWrite(fanInA, speed > 0 ? speed : 0);
  analogWrite(fanInB, speed < 0 ? -speed : 0);
}

float medianDistance() {
  float a = sensor.measureDistanceCm(); delay(60);
  float b = sensor.measureDistanceCm(); delay(60);
  float c = sensor.measureDistanceCm();
  float d = max(min(a,b), min(max(a,b),c));   // median of 3

  return d;
}

void setup() {
  Serial.begin(115200);
  pinMode(fanInA, OUTPUT);
  pinMode(fanInB, OUTPUT);
}

void loop() {
  float dist = medianDistance(); // in cm
  Serial.println(dist);
  
  if (dist > 0 && dist <= 15) {
    // if distance less than 15
    fan(-maxFanSpeed);
  } else {
    fan(0);
  }

  delay(100);
}
