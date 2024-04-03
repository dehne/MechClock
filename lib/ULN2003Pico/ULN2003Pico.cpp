/****
 * 
 * This file is a portion of the package ULN2003Pico, a library that provides an Arduino sketch 
 * running on a Raspberry Pi Pico or Pico-W with the ability to control an array of 28BYJ-48 
 * four-phase, eight-beat variable-reluctance stepper motors driven through ULN2003 Darlington 
 * array chips. 
 * 
 * See ULN2003Pico.h for details.
 *
 *****
 * 
 * ULN2003Pico V1.0.0, April 2024
 * Copyright (C) 2020-2024 D.L. Ehnebuske
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. 
 * 
 ****/

#include "ULN2003Pico.h"
#include <pico/time.h>

/**
 * 
 * Class variables
 * 
 **/
// Outputs by step phase:      1     1+4       4     4+3       3     3+2       2     2+1
static byte phState[8] = {0b0001, 0b1001, 0b1000, 0b1100, 0b0100, 0b0110, 0b0010, 0b0011};
static byte maxObjNo = -1;                          // The index in of the last instantiated ULN2003 obj
static unsigned int curSpeed[UL_MAX_OBJECTS];       // The current speed in steps per second
static unsigned int usPerStep[UL_MAX_OBJECTS];      // μs per step (1,000,000 / curSpeed)
static volatile long stepsToGo[UL_MAX_OBJECTS];     // The current number of steps still to be taken. Positive is clockwise
static volatile long location[UL_MAX_OBJECTS];      // The current location in steps. Positive is clockwise
static long modulus[UL_MAX_OBJECTS];                // The modulus for location. 0 indicates location is linear
static volatile unsigned long microsNextStep[UL_MAX_OBJECTS]; // micros() when next step is to be dispatched.
static byte motorPin[UL_MAX_OBJECTS][4];            // The pins to which the motor is attached
static byte phase[UL_MAX_OBJECTS];                  // Which stepping phase the motor is in
static repeating_timer_t rt;                        // Where Pico SDK puts the repeating timer info

/**
 * 
 * The timer ISR that actually steps the motors.
 * 
 **/
bool repeatingTimerCallback(repeating_timer_t *rt) {
    #ifdef UL_DEBUG
    digitalWrite(LED_BUILTIN, HIGH);
    #endif
    unsigned long curMicros = micros();
    unsigned long nextDispMicros = curMicros + UL_TIMER_INTERVAL;
    for (byte i = 0; i <= maxObjNo; i++) {
        // Deal with a ULN2003 object instance i
        if (stepsToGo[i] != 0 && microsNextStep[i] - UL_MAX_JITTER <= curMicros) {
            // Dispatch a step
            phase[i] = (phase[i] + (stepsToGo[i] < 0 ? 1 : 7)) & 0x7;
            for (byte j = 0; j < 4; j++) {
                digitalWrite(motorPin[i][j], bitRead(phState[phase[i]], j) == 1 ? HIGH : LOW);
                if (abs(stepsToGo[i]) == 1) {           // If just took last step
                    digitalWrite(motorPin[i][j], LOW);  //   Put it in rest mode
                }
            }
            location[i] += stepsToGo[i] > 0 ? 1 : -1;
            if (modulus[i] != 0) {
                if (location[i] < 0) {
                    location[i] += modulus[i];
                } else if (location[i] >= modulus[i]) {
                    location[i] -= modulus[i];
                }
            }
            stepsToGo[i] -= stepsToGo[i] > 0 ? 1 : -1;
            if (stepsToGo[i] != 0) {
                microsNextStep[i] += usPerStep[i];
                if (microsNextStep[i] < nextDispMicros) {
                    nextDispMicros = microsNextStep[i];
                }
            }
        }
    }
    #ifdef UL_DEBUG
    digitalWrite(LED_BUILTIN, LOW);
    #endif
    return true;
}

ULN2003::ULN2003() {
    if(++maxObjNo >= UL_MAX_OBJECTS) {
        Serial.println(F("Too many ULN2003 objects; increase UL_MAX_OBJECTS."));
        while (true) {
            // Spin
        }
    }
    objIx = maxObjNo;
}

void ULN2003::begin(byte pin1, byte pin2, byte pin3, byte pin4) {
    #ifdef UL_DEBUG
    Serial.println(F("ULN2003 begin()"));
    digitalWrite(LED_BUILTIN, HIGH);
    #endif
    motorPin[objIx][0] = pin1;
    motorPin[objIx][1] = pin2;
    motorPin[objIx][2] = pin3;
    motorPin[objIx][3] = pin4;
    curSpeed[objIx] = UL_DEFAULT_SPEED;
    usPerStep[objIx] = 1000000L / UL_DEFAULT_SPEED;
    location[objIx] = 0;
    modulus[objIx] = UL_STEPS_PER_REV;
    stepsToGo[objIx] = 0;
    phase[objIx] = 0;

    #ifdef UL_DEBUG
    Serial.printf("[ULN2003::begin] Stepper %d pins: %d, %d, %d, %d.\n", 
        objIx, motorPin[objIx][0], motorPin[objIx][1], motorPin[objIx][2], motorPin[objIx][3]);
    #endif

    for (byte i = 0; i < 4; i++) {
        pinMode(motorPin[objIx][i], OUTPUT);
        digitalWrite(motorPin[objIx][i], LOW);
    }

    
    if (objIx == 0) {
        #ifdef UL_DEBUG
        Serial.println(F("Initializing repeating timer."));
        #endif
        // Set up our repeating timer
        alarm_pool_init_default();
        add_repeating_timer_us(UL_TIMER_INTERVAL, repeatingTimerCallback, nullptr, &rt);
    }
    #ifdef UL_DEBUG
    digitalWrite(LED_BUILTIN, LOW);
    #endif
}

void ULN2003::moveIt(long amt, bool abs) {
    uint32_t intStatus = save_and_disable_interrupts();         // Make the following atomic by disabling interrupts
    long newStepsToGo;
    if (abs) {                                                  // If the motion is to an absolute position
        newStepsToGo = amt - location[objIx];                   //   Steps needed is steps from origin to destination less steps from origin to where we are
        if (modulus[objIx] !=0) {                               //   If we're doing modulus motion, reduce number of steps to less that 1/2 turn
            newStepsToGo %= modulus[objIx];
            if (newStepsToGo > modulus[objIx] / 2) {
                newStepsToGo -= modulus[objIx];
            } else if (newStepsToGo < modulus[objIx] / -2) {
                newStepsToGo += modulus[objIx];
            }
        }
    } else {                                                    // Otherwise it's a relative motion
        newStepsToGo = stepsToGo[objIx] + amt;                  //   The new number of steps is the old number plus the new steps
    }
    if (stepsToGo[objIx] == 0 && newStepsToGo != 0) {           // Transition to moving
        microsNextStep[objIx] = micros();
    } else if (stepsToGo[objIx] != 0 && newStepsToGo == 0) {    // Transition to not moving
        for (byte j = 0; j < 4; j++) {
            digitalWrite(motorPin[objIx][j], LOW);              //   Put motor in resting mode
        }
    }
    stepsToGo[objIx] = newStepsToGo;                            // Set the number of steps neede to get to destination
    restore_interrupts(intStatus);                              // End the atomic operation
}

void ULN2003::drive(long nSteps) {
    #ifdef UL_DEBUG
    Serial.printf("drive(%d)\n", nSteps);
    #endif
    if (nSteps == 0) {
        return;
    }
    moveIt(nSteps, false);
}

void ULN2003::driveTo(long loc) {
    moveIt(loc, true);
}

void ULN2003::stop() {
    uint32_t intStatus = save_and_disable_interrupts();
    stepsToGo[objIx] = stepsToGo[objIx] > 0 ? 1 : stepsToGo[objIx] < 0 ? -1 : 0;
    restore_interrupts(intStatus);
}

void ULN2003::setLocation(long steps){
    uint32_t intStatus = save_and_disable_interrupts();
    if (modulus[objIx] != 0) {
        steps %= modulus[objIx];
        if (steps < 0) {
            steps += modulus[objIx];
        }
    }
    location[objIx] = steps;
    restore_interrupts(intStatus);
}

long ULN2003::getLocation() {
    long answer;
    uint32_t intStatus = save_and_disable_interrupts();
    answer = location[objIx];
    restore_interrupts(intStatus);
    return answer;
}

void ULN2003::setModulus(unsigned long steps) {
    uint32_t intStatus = save_and_disable_interrupts();
    modulus[objIx] = steps;
    location[objIx] %= steps;
    if (location[objIx] < 0) {
        location[objIx] += steps;
    }
    restore_interrupts(intStatus);
}

unsigned long ULN2003::getModulus() {
    unsigned long answer;
    uint32_t intStatus = save_and_disable_interrupts();
    answer = modulus[objIx];
    restore_interrupts(intStatus);
    return answer;
}

void ULN2003::setSpeed(unsigned int speed){
    uint32_t intStatus = save_and_disable_interrupts();
    if (speed <= 0) {
        speed = UL_DEFAULT_SPEED;
    }
    curSpeed[objIx] = speed;
    usPerStep[objIx] = 1000000L / speed;
    restore_interrupts(intStatus);
}

unsigned int ULN2003::getSpeed() {
    unsigned int answer;
    uint32_t intStatus = save_and_disable_interrupts();
    answer = curSpeed[objIx];
    restore_interrupts(intStatus);
    return answer;
}

long ULN2003::getStepsToGo() {
    long answer;
    uint32_t intStatus = save_and_disable_interrupts();
    answer = stepsToGo[objIx];
    restore_interrupts(intStatus);
    return answer;
}

bool ULN2003::isMoving() {
    bool answer;
    uint32_t intStatus = save_and_disable_interrupts();
    answer = stepsToGo[objIx] != 0;
    restore_interrupts(intStatus);
    return answer;
}