#include "stepper.h"

const char* STEPPER_TAG = "Stepper";

Stepper::Stepper(gpio_num_t step_pin, gpio_num_t dir_pin, gpio_num_t en_pin)
    : m_step_pin(step_pin), m_dir_pin(dir_pin), m_en_pin(en_pin),
      m_enabled(false), m_cmd_queue(nullptr), m_task(nullptr) {}

Stepper::~Stepper() {
    stop();
    if (m_task) {
        vTaskDelete(m_task);
        m_task = nullptr;
    }
    if (m_cmd_queue) {
        vQueueDelete(m_cmd_queue);
        m_cmd_queue = nullptr;
    }
}

void Stepper::init() {
    gpio_reset_pin(m_step_pin);
    gpio_reset_pin(m_dir_pin);
    gpio_set_direction(m_step_pin, GPIO_MODE_OUTPUT);
    gpio_set_direction(m_dir_pin,  GPIO_MODE_OUTPUT);
    gpio_set_level(m_step_pin, 0);
    gpio_set_level(m_dir_pin,  0);

    if (m_en_pin != GPIO_NUM_NC) {
        gpio_reset_pin(m_en_pin);
        gpio_set_direction(m_en_pin, GPIO_MODE_OUTPUT);
        gpio_set_level(m_en_pin, 1);  // active-low, 1 = disabled
    }

    m_cmd_queue = xQueueCreate(1, sizeof(StepperCommand));
    if (!m_cmd_queue) {
        ESP_LOGE(STEPPER_TAG, "Failed to create command queue");
        return;
    }

    BaseType_t ret = xTaskCreate(task_func, "stepper", 2048, this, 6, &m_task);
    if (ret != pdPASS) {
        ESP_LOGE(STEPPER_TAG, "Failed to create stepper task");
        m_task = nullptr;
    }

    m_enabled = true;
    enable(true);
}

void Stepper::enable(bool on) {
    if (m_en_pin != GPIO_NUM_NC) {
        gpio_set_level(m_en_pin, on ? 0 : 1);  // active-low
    }
    m_enabled = on;
}

void Stepper::move(int steps, float speed_rpm) {
    if (!m_cmd_queue) return;
    if (steps == 0 || speed_rpm <= 0.0f) return;

    StepperCommand cmd;
    cmd.type      = StepperCommand::MOVE;
    cmd.steps     = steps;
    cmd.speed_rpm = speed_rpm;

    xQueueOverwrite(m_cmd_queue, &cmd);
}

void Stepper::stop() {
    if (!m_cmd_queue) return;
    StepperCommand cmd;
    cmd.type  = StepperCommand::STOP;
    cmd.steps = 0;
    cmd.speed_rpm = 0;
    xQueueOverwrite(m_cmd_queue, &cmd);
}

bool Stepper::is_running() const {
    return m_running;
}

#ifdef CONFIG_SARCUS_STEPPER_STEPS_PER_REV
    #define STEPS_PER_REV CONFIG_SARCUS_STEPPER_STEPS_PER_REV
#else
    #define STEPS_PER_REV 200
#endif

void Stepper::task_func(void* arg) {
    auto* self = static_cast<Stepper*>(arg);
    StepperCommand cmd;

    while (true) {
        if (xQueueReceive(self->m_cmd_queue, &cmd, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        if (cmd.type != StepperCommand::MOVE || cmd.steps == 0) {
            self->m_running = false;
            continue;
        }

        gpio_set_level(self->m_dir_pin, (cmd.steps > 0) ? 1 : 0);
        esp_rom_delay_us(10);

        int remaining = (cmd.steps > 0) ? cmd.steps : -cmd.steps;
        int rpm       = static_cast<int>(cmd.speed_rpm + 0.5f);
        if (rpm < 1) rpm = 1;

        self->m_running = true;
        self->m_remaining = remaining;
        self->m_total_steps = remaining;

        int half_period_us = 30000000 / (rpm * STEPS_PER_REV);
        if (half_period_us < 3)  half_period_us = 3;

        while (remaining > 0) {
            if (uxQueueMessagesWaiting(self->m_cmd_queue) > 0) {
                break;
            }

            gpio_set_level(self->m_step_pin, 1);
            if (half_period_us > 2000) {
                vTaskDelay(pdMS_TO_TICKS(half_period_us / 1000));
            } else {
                esp_rom_delay_us(half_period_us);
            }
            gpio_set_level(self->m_step_pin, 0);
            remaining--;
            self->m_remaining = remaining;

            if (remaining > 0) {
                if (half_period_us > 2000) {
                    vTaskDelay(pdMS_TO_TICKS(half_period_us / 1000));
                } else {
                    esp_rom_delay_us(half_period_us);
                }
            }
        }

        gpio_set_level(self->m_step_pin, 0);
        self->m_running = false;
        self->m_remaining = 0;
        ESP_LOGD(STEPPER_TAG, "Move complete");
    }
}
