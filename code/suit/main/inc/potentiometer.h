/**
 * @file potentiometer.h
 * @author Ziad Mohammed Fathy
 * @brief potentiometer driver.
 * @version 0.1
 * @date 2025-05-20
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#ifndef _INC_POTENTIOMETER_H_
#define _INC_POTENTIOMETER_H_

#include "driver/adc.h"

class potentiometer {
    public:
        /**
         * @brief Construct a new potentiometer object
         * 
         * @param copyChannel 
         */
        potentiometer(adc1_channel_t copyChannel);
        
        /**
         * @brief 
         * 
         * @return int 
         */
        int Raw();
        
        /**
         * @brief Get the Voltage object
         * 
         * @return float 
         */
        float getAngle();
    private:
        adc1_channel_t pot_channel;
        float prev_angle = 0.0f;
        float filter(float current);
};

#endif