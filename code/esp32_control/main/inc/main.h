#ifndef MAIN_H_
#define MAIN_H_

#include <string>
//#include "led.h"
//#include "button.h"
#include "dc_motor.h"
//#include "TCRT5000.h"
//#include "MPU6050.h"
//#include "ultrasonic.h"
#include "bts7960.h"
#include "bts7960_position.h"
//#include "pot.h"
//#include "pid.h"
//#include "MQTT.h"
//#include "NVS.h"
//#include "WiFi.h"

extern const char *TAG;

// WiFi configurations
#define WIFI_SSID               "WE_BF1790"
#define WIFI_PASSWORD           "n7j05024"

// MQTT configurations
#define mqtt_broker             "mqtt://192.168.1.5"
#define mqtt_port               1883

#define topic_robot_movement    "sarcus/robot/movement"
#define topic_robot_estop       "sarcus/robot/estop"
#define topic_joint_shoulder    "sarcus/robot/joints/shoulder"
#define topic_joint_elbow       "sarcus/robot/joints/elbow"

// Servo signal pins (adjust to your wiring)
#define SERVO_PIN_SHOULDER_LEFT   GPIO_NUM_13
#define SERVO_PIN_SHOULDER_RIGHT  GPIO_NUM_14
#define SERVO_PIN_ELBOW_LEFT      GPIO_NUM_27
#define SERVO_PIN_ELBOW_RIGHT     GPIO_NUM_23

// Motion smoothing (50 ms control loop)
#define MOTION_CTRL_PERIOD_MS     50
#define MOTOR_SPEED_SLEW_STEP     6.0f    // max % change per tick
#define SERVO_ANGLE_SLEW_STEP     4.0f    // max degrees per tick
#define SERVO_ANGLE_FILTER_ALPHA  0.7f   // low-pass: 0=slow, 1=instant
#define SERVO_DEFAULT_ANGLE       135.0f

// topics of right hand
#define topic_right_shoulder1   "arm_right/shoulder1"
#define topic_right_shoulder2   "arm_right/shoulder2"
// #define topic_right_elbow       "arm_right/elbow"
// #define topic_right_wrist       "arm_right/wrist"

// topics of left hand
#define topic_left_shoulder1   "arm_left/shoulder1"
#define topic_left_shoulder2   "arm_left/shoulder2"
#define topic_left_elbow       "arm_left/elbow"
#define topic_left_wrist       "arm_left/wrist"

// topic of rover dc motors
#define topic_forward           "move/forward"
#define topic_backward          "move/backward"
#define topic_right             "move/right"
#define topic_left              "move/left"

// gripper topics
#define topic_gripper_close     "gripper/close"
#define topic_gripper_open      "gripper/open"

// prototyping functions
//void setup_wifi();
//void setup_mqtt();
//void setup_HW();

#endif