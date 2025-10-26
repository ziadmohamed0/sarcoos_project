#ifndef BUTTON_H_
#define BUTTON_H_

#include "common.h"

extern const char *BTN_TAG;

class Button {
    public:
        Button(gpio_num_t pin_number, gpio_pull_mode_t pull);
        bool GET();
        ~Button() = default;
    private:
        gpio_num_t pin;
        gpio_pull_mode_t pull;
};

#endif