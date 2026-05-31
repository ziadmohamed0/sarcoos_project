#include "bts7960_pot_position.h"

const char *BTS7960_POT_TAG = "BTS7960_POT_POS";

BTS7960_Pot_Position::BTS7960_Pot_Position(BTS7960* motor, 
                                         Potentiometer* pot,
                                         float kp, float ki, float kd)
    : motor_driver(motor),
      feedback_pot(pot),
      pid_controller(kp, ki, kd, -100.0f, 100.0f),
      current_angle(0),
      target_angle(0),
      angle_tolerance(1.0f),
      last_time(esp_timer_get_time()) {}

void BTS7960_Pot_Position::setAngle(float angle) {
    this->target_angle = angle;
    ESP_LOGI(BTS7960_POT_TAG, "Target angle set to %.2f", angle);
}

float BTS7960_Pot_Position::getAngle() {
    this->current_angle = (float)this->feedback_pot->readAngle();
    return this->current_angle;
}

float BTS7960_Pot_Position::getTargetAngle() {
    return this->target_angle;
}

void BTS7960_Pot_Position::update() {
    int64_t now = esp_timer_get_time();
    float dt = (float)(now - this->last_time) / 1000000.0f; // Seconds
    this->last_time = now;

    if (dt <= 0) dt = 0.001f; // Avoid division by zero

    this->current_angle = (float)this->feedback_pot->readAngle();
    float error = this->target_angle - this->current_angle;

    if (std::abs(error) < this->angle_tolerance) {
        this->motor_driver->stop();
        return;
    }

    float speed = this->pid_controller.updatePID(this->target_angle, this->current_angle, dt);

    if (speed > 0) {
        this->motor_driver->forward(std::abs(speed));
    } else {
        this->motor_driver->backward(std::abs(speed));
    }
}

void BTS7960_Pot_Position::setPID(float kp, float ki, float kd) {
    this->pid_controller.setGains(kp, ki, kd);
}

bool BTS7960_Pot_Position::isAtTarget() {
    return std::abs(this->target_angle - this->getAngle()) < this->angle_tolerance;
}

void BTS7960_Pot_Position::setAngleTolerance(float tolerance) {
    this->angle_tolerance = tolerance;
}
