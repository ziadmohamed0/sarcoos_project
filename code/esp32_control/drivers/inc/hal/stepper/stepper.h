#ifndef STEPPER_H_
#define STEPPER_H_

#include "common.h"

extern const char* STEPPER_TAG;

struct StepperCommand {
    enum Type : uint8_t { NONE, MOVE, STOP };
    Type type;
    int   steps;       // signed: positive = CW, negative = CCW
    float speed_rpm;   // RPM
};

class Stepper {
public:
    Stepper(gpio_num_t step_pin, gpio_num_t dir_pin,
            gpio_num_t en_pin = GPIO_NUM_NC);
    ~Stepper();

    void init();
    void enable(bool on);
    void move(int steps, float speed_rpm);
    void stop();
    bool is_running() const;
    int  getRemainingSteps() const { return m_remaining; }
    int  getTotalSteps() const { return m_total_steps; }

private:
    gpio_num_t m_step_pin;
    gpio_num_t m_dir_pin;
    gpio_num_t m_en_pin;
    bool       m_enabled;

    volatile bool  m_running = false;
    volatile int   m_remaining = 0;
    volatile int   m_total_steps = 0;

    QueueHandle_t m_cmd_queue;
    TaskHandle_t  m_task;

    static void task_func(void* arg);
};

#endif
