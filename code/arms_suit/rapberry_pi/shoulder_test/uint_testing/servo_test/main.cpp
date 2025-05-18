#include <pigpio.h>
#include <thread>
#include <vector>
#include <iostream>
#include <unistd.h>

#define SERVO1_PIN 18
#define SERVO2_PIN 19


void moveServo(int pin, const std::vector<int>& angles, const std::vector<float>& delays);
void fastMovement();
void slowMovement();


void moveServo(int pin, const std::vector<int>& angles, const std::vector<float>& delays) {
    for (size_t i = 0; i < angles.size(); ++i) {
        int pulseWidth = 500 + (angles[i] * 2000 / 180); // 500-2500μs
        gpioServo(pin, pulseWidth);
        usleep(delays[i] * 1000000);
    }
}


void fastMovement() {
    std::vector<int> angles = {0, 45, 90, 135, 180, 135, 90, 45, 0};
    std::vector<float> delays(angles.size(), 0.3);

    std::thread servo1(moveServo, SERVO1_PIN, angles, delays);
    
    std::vector<int> reversedAngles(angles.rbegin(), angles.rend());
    std::thread servo2(moveServo, SERVO2_PIN, reversedAngles, delays);
    
    servo1.join();
    servo2.join();
}


void slowMovement() {
    std::vector<int> angles = {0, 30, 60, 90, 120, 150, 180};
    std::vector<float> delays(angles.size(), 0.8);

    std::thread servo1(moveServo, SERVO1_PIN, angles, delays);
    std::thread servo2(moveServo, SERVO2_PIN, angles, delays);
    
    servo1.join();
    servo2.join();
}

int main() {
    if (gpioInitialise() < 0) {
        std::cerr << "faild to initialized pigpio!" << std::endl;
        return 1;
    }

    try {
        while (true) {
            std::cout << "speed move" << std::endl;
            fastMovement();
            sleep(1);
            
            std::cout << "slowe move" << std::endl;
            slowMovement();
            sleep(1);
        }
    } catch (...) {
        gpioTerminate();
        return 0;
    }

    gpioTerminate();
    return 0;
}
