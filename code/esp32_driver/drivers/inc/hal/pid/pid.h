#ifndef _PID_H_
#define _PID_H_

#include "common.h"

class PID {
    public:
        PID(float p, float i, float d, float min_output = -100.0f, float max_output = 100.0f);
        float updatePID(float setPoint, float measurment, float dt);
        void reset();
        void setGains(float p, float i, float d);
        void setOutputLimits(float min, float max);
        ~PID() = default;
    private:
        float kp;
        float ki;
        float kd;
        float error;
        float integral;
        float derivative;
        float prevError;
        float min_out;
        float max_out;
};

#endif