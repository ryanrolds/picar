#pragma once

#include <linux/i2c-dev.h>
#include <wiringPi.h>

int setupControl();
void cleanupControl(int);
void setSteering(int, __u16);
void setForward(int, __u16);
void setBackward(int, __u16);
void setStop(int);

// Steering server range
const __u16 STEERING_MIN = 285;
const __u16 STEERING_MAX = 415;
const __u16 STEERING_CEN = 350;

// 285 (65) 355 (65) 415

struct Command {

};
