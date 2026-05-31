#ifndef MAIN_H_
#define MAIN_H_

#include <string>
#include "dc_motor.h"
#include "bts7960.h"
#include "bts7960_position.h"

extern const char *TAG;

// ─── Kconfig overrides (if using menuconfig) ─────────────────────────────────
// If Kconfig.projbuild values exist, use them; otherwise keep hardcoded defaults.
#ifdef CONFIG_SARCUS_WIFI_SSID
    #define WIFI_SSID               CONFIG_SARCUS_WIFI_SSID
#else
    #define WIFI_SSID               "WE_BF1790"
#endif

#ifdef CONFIG_SARCUS_WIFI_PASSWORD
    #define WIFI_PASSWORD           CONFIG_SARCUS_WIFI_PASSWORD
#else
    #define WIFI_PASSWORD           "n7j05024"
#endif

#ifdef CONFIG_SARCUS_MQTT_BROKER_URI
    #define DEFAULT_MQTT_URI        CONFIG_SARCUS_MQTT_BROKER_URI
#else
    #define DEFAULT_MQTT_URI        "mqtt://192.168.1.5"
#endif

#ifdef CONFIG_SARCUS_MQTT_PORT
    #define mqtt_port               CONFIG_SARCUS_MQTT_PORT
#else
    #define mqtt_port               1883
#endif

#ifdef CONFIG_SARCUS_SERVO_SHOULDER_LEFT
    #define SERVO_PIN_SHOULDER_LEFT   ((gpio_num_t)CONFIG_SARCUS_SERVO_SHOULDER_LEFT)
#else
    #define SERVO_PIN_SHOULDER_LEFT   GPIO_NUM_13
#endif

#ifdef CONFIG_SARCUS_SERVO_SHOULDER_RIGHT
    #define SERVO_PIN_SHOULDER_RIGHT  ((gpio_num_t)CONFIG_SARCUS_SERVO_SHOULDER_RIGHT)
#else
    #define SERVO_PIN_SHOULDER_RIGHT  GPIO_NUM_14
#endif

#ifdef CONFIG_SARCUS_SERVO_ELBOW_LEFT
    #define SERVO_PIN_ELBOW_LEFT      ((gpio_num_t)CONFIG_SARCUS_SERVO_ELBOW_LEFT)
#else
    #define SERVO_PIN_ELBOW_LEFT      GPIO_NUM_27
#endif

#ifdef CONFIG_SARCUS_SERVO_ELBOW_RIGHT
    #define SERVO_PIN_ELBOW_RIGHT     ((gpio_num_t)CONFIG_SARCUS_SERVO_ELBOW_RIGHT)
#else
    #define SERVO_PIN_ELBOW_RIGHT     GPIO_NUM_23
#endif

// Stepper pin defaults
#ifdef CONFIG_SARCUS_STEPPER_LEFT_STEP
    #define STEPPER_PIN_LEFT_STEP   ((gpio_num_t)CONFIG_SARCUS_STEPPER_LEFT_STEP)
#else
    #define STEPPER_PIN_LEFT_STEP   GPIO_NUM_4
#endif
#ifdef CONFIG_SARCUS_STEPPER_LEFT_DIR
    #define STEPPER_PIN_LEFT_DIR    ((gpio_num_t)CONFIG_SARCUS_STEPPER_LEFT_DIR)
#else
    #define STEPPER_PIN_LEFT_DIR    GPIO_NUM_21
#endif
#ifdef CONFIG_SARCUS_STEPPER_LEFT_EN
    #define STEPPER_PIN_LEFT_EN     ((gpio_num_t)CONFIG_SARCUS_STEPPER_LEFT_EN)
#else
    #define STEPPER_PIN_LEFT_EN     GPIO_NUM_22
#endif
#ifdef CONFIG_SARCUS_STEPPER_RIGHT_STEP
    #define STEPPER_PIN_RIGHT_STEP  ((gpio_num_t)CONFIG_SARCUS_STEPPER_RIGHT_STEP)
#else
    #define STEPPER_PIN_RIGHT_STEP  GPIO_NUM_15
#endif
#ifdef CONFIG_SARCUS_STEPPER_RIGHT_DIR
    #define STEPPER_PIN_RIGHT_DIR   ((gpio_num_t)CONFIG_SARCUS_STEPPER_RIGHT_DIR)
#else
    #define STEPPER_PIN_RIGHT_DIR   GPIO_NUM_2
#endif
#ifdef CONFIG_SARCUS_STEPPER_RIGHT_EN
    #define STEPPER_PIN_RIGHT_EN    ((gpio_num_t)CONFIG_SARCUS_STEPPER_RIGHT_EN)
#else
    #define STEPPER_PIN_RIGHT_EN    GPIO_NUM_5
#endif

// Motion smoothing
#ifdef CONFIG_SARCUS_CTRL_PERIOD_MS
    #define MOTION_CTRL_PERIOD_MS     CONFIG_SARCUS_CTRL_PERIOD_MS
#else
    #define MOTION_CTRL_PERIOD_MS     50
#endif

#ifdef CONFIG_SARCUS_MOTOR_SLEW_STEP
    #define MOTOR_SPEED_SLEW_STEP     CONFIG_SARCUS_MOTOR_SLEW_STEP
#else
    #define MOTOR_SPEED_SLEW_STEP     6.0f
#endif

#ifdef CONFIG_SARCUS_SERVO_SLEW_STEP
    #define SERVO_ANGLE_SLEW_STEP     CONFIG_SARCUS_SERVO_SLEW_STEP
#else
    #define SERVO_ANGLE_SLEW_STEP     4.0f
#endif

#ifdef CONFIG_SARCUS_SERVO_FILTER_ALPHA
    #define SERVO_ANGLE_FILTER_ALPHA  CONFIG_SARCUS_SERVO_FILTER_ALPHA
#else
    #define SERVO_ANGLE_FILTER_ALPHA  0.7f
#endif

#ifdef CONFIG_SARCUS_SERVO_DEFAULT_ANGLE
    #define SERVO_DEFAULT_ANGLE       CONFIG_SARCUS_SERVO_DEFAULT_ANGLE
#else
    #define SERVO_DEFAULT_ANGLE       135.0f
#endif

// ─── MQTT topics (always the same) ──────────────────────────────────────────
#define topic_robot_movement    "sarcus/robot/movement"
#define topic_robot_estop       "sarcus/robot/estop"
#define topic_joint_shoulder    "sarcus/robot/joints/shoulder"
#define topic_joint_elbow       "sarcus/robot/joints/elbow"
#define topic_joint_wrist       "sarcus/robot/joints/wrist"

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

#endif