/**
  * @author: Ziad Mohammed Fathy
  * @file : servo.h
  * @brief: header file to control servo motor
  * @version : _v2
  */

#ifndef _INC_SERVO_H_
#define _INC_SERVO_H_

/* include libraries */
#include <iostream> 	// standared library for c++
#include <pigpio.h>	// gpio library for rasberry pi 4
#include <unistd.h>	// for sleep (delay) function

/**
  * @brief: class for drive servo motors in project
  */
class servo {
	public:
		/**
		  * @brief: constaractor of servo class initialize gpio.
		  * @param: copyPinNumber -> number of signal pin.
		  */
		servo(uint8_t copyPinNumber);
		/**
		  * @brief: function to move servo.
		  * @param: copyAngle -> angle of servo to convert it to PWM signal.
		  */
		void moveServo(float copyAngle);
		~servo();
	private:
		uint8_t pin; // number of pin.
};

#endif
