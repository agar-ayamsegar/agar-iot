#include <HCSR04.h>


// ultrasonic
const byte trigPin = 2; // pin 2
const byte echoPin = 1; // pin 1
UltraSonicDistanceSensor sensor(trigPin, echoPin);

// fan
const int fanInA = 42; // pin 42
const int fanInB = 41; // pin 41
const int maxFanSpeed = 100; // speed interval is 0...255, 100 is sweet spot, lower if you want

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

void setupFan() {
  pinMode(fanInA, OUTPUT);
  pinMode(fanInB, OUTPUT);

  Serial.println("Setting up agar fan");
}

bool isDistance() {
  float dist = medianDistance(); // in cm
  return dist > 0 && dist < 10; // if less than 10 
}