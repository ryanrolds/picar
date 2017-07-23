#include "senses.hpp"

#include <wiringPi.h>

#define OBJ_PIN 22

int setupSenses() {
  pinMode(OBJ_PIN, INPUT);

  return 1;
}

bool hasObstacle() {
  int value = digitalRead(OBJ_PIN);  
  return value == 0;
}
