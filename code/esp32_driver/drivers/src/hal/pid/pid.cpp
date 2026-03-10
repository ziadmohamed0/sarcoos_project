#include "pid.h"

PID::PID(float p, float i, float d, float min_output, float max_output) :
            kp(p),
            ki(i),
            kd(d),
            error(0),
            integral(0),
            derivative(0),
            prevError(0),
            min_out(min_output),
            max_out(max_output) {}

float PID::updatePID(float setPoint, float measurment, float dt) {
    this->error = setPoint - measurment;

    // Integral calculation
    this->integral += error * dt;

    // Integral anti-windup (clamp integral term)
    if (this->integral > this->max_out) this->integral = this->max_out;
    else if (this->integral < this->min_out) this->integral = this->min_out;

    this->derivative  = (error - this->prevError) / dt;

    float output = ((this->kp * error) +
                    (this->ki * this->integral) +
                    (this->kd * derivative));

    // Output clamping
    if (output > this->max_out) output = this->max_out;
    else if (output < this->min_out) output = this->min_out;

    this->prevError = error;
    
    return output;
}

void PID::reset() {
    this->error = 0;
    this->integral = 0;
    this->derivative = 0;
    this->prevError = 0;
}

void PID::setGains(float p, float i, float d) {
    this->kp = p;
    this->ki = i;
    this->kd = d;
}

void PID::setOutputLimits(float min, float max) {
    this->min_out = min;
    this->max_out = max;
}