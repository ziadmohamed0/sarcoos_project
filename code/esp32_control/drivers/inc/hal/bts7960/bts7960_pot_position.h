#ifndef BTS7960_POT_POSITION_H_
#define BTS7960_POT_POSITION_H_

#include "common.h"
#include "bts7960.h"
#include "pot.h"
#include "pid.h"

extern const char *BTS7960_POT_TAG;

class BTS7960_Pot_Position {
public:
    /**
     * @brief Construct a new BTS7960 Pot Position object
     * 
     * @param motor Pointer to BTS7960 motor driver
     * @param pot Pointer to Potentiometer feedback
     * @param kp Proportional gain
     * @param ki Integral gain
     * @param kd Derivative gain
     */
    BTS7960_Pot_Position(BTS7960* motor, 
                        Potentiometer* pot,
                        float kp = 1.0f, 
                        float ki = 0.0f, 
                        float kd = 0.0f);

    /**
     * @brief Set target angle
     * 
     * @param angle Target angle in degrees
     */
    void setAngle(float angle);

    /**
     * @brief Get current angle from potentiometer
     * 
     * @return float Current angle
     */
    float getAngle();

    /**
     * @brief Get target angle
     * 
     * @return float Target angle
     */
    float getTargetAngle();

    /**
     * @brief Update control loop. Should be called periodically.
     */
    void update();

    /**
     * @brief Set PID gains
     * 
     * @param kp Proportional gain
     * @param ki Integral gain
     * @param kd Derivative gain
     */
    void setPID(float kp, float ki, float kd);

    /**
     * @brief Check if reached target position
     * 
     * @return true if within tolerance
     */
    bool isAtTarget();

    /**
     * @brief Set angle tolerance for reaching target
     * 
     * @param tolerance Tolerance in degrees
     */
    void setAngleTolerance(float tolerance);

    ~BTS7960_Pot_Position() = default;

private:
    BTS7960* motor_driver;
    Potentiometer* feedback_pot;
    PID pid_controller;

    float current_angle;
    float target_angle;
    float angle_tolerance;

    int64_t last_time;
};

#endif // BTS7960_POT_POSITION_H_
