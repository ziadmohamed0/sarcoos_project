/**
  * @author: Ziad Mohammed Fathy
  * @file : servo.cpp
  * @brief: source file to control servo motor
  * @version : _v2
  */

#include "servo.h"

servo::servo(uint8_t copyPinNumber) : pin(copyPinNumber) {
	gpioInitialise();
}

void servo::moveServo(float copyAngle) {
	if(copyAngle < 0.0f) {
		copyAngle = 0.0f;
	}
	else if(copyAngle > 180.0f) {
		copyAngle = 180.0f;
	}

	int pulsWidth = static_cast<int>(500.0f + (copyAngle * 2000.0f / 180.0f));
	gpioServo(pin, pulsWidth);
}

servo::~servo() {
	gpioServo(pin, 0);
	gpioTerminate();
}
