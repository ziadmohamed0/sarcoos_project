#include <Arduino.h>

#define RPWM  23
#define LPWM  22
#define R_EN  21
#define L_EN  19

void setup() {
  pinMode(RPWM, OUTPUT);
  pinMode(LPWM, OUTPUT);
  pinMode(R_EN, OUTPUT);
  pinMode(L_EN, OUTPUT);

  digitalWrite(R_EN, HIGH);
  digitalWrite(L_EN, HIGH);
}

void loop() {
  digitalWrite(RPWM, HIGH);
  digitalWrite(LPWM, LOW);
  delay(2000);

  digitalWrite(RPWM, LOW);
  digitalWrite(LPWM, LOW);
  delay(1000);

  digitalWrite(RPWM, LOW);
  digitalWrite(LPWM, HIGH);
  delay(2000);

  digitalWrite(RPWM, LOW);
  digitalWrite(LPWM, LOW);
  delay(1000);
}

