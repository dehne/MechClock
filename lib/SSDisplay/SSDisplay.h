/****
 * 
 * This file is a portion of the package SSDisplay. It drives a digital clock display consisting 
 * of four electromechanical seven-segment digit display modules to display two digits for the 
 * hour and two for the minutes. Each module has a 28BYJ-48 stepper motor to move it and a 4133 
 * Hall-effect sensor that is used to "home" each module at power on so that it moves to a known 
 * home position at startup. 
 * 
 * Driving a module's stepper by 1/5 of a turn changes the display by +/- one, depending on the 
 * direction it's turned. In a 2:1 ratio, the stepper turns seven cams ganged together. Each cam 
 * controls one segment of the module's digit display, extending or retracting it to cause the 
 * appropriate combination of segements to form the desired digit.
 * 
 *****
 * 
 * SSDisplay V1.1.0, April 2024
 * Copyright (C) 2024 D.L. Ehnebuske
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

#pragma once
#ifndef Arduino_h
    #include <Arduino.h>
#endif
#include <ULN2003Pico.h>
/**
 * 
 * Compile-time constants
 * 
 **/
#define SSD_STEPS_PER_SEC       (400)                   // How many steps per second to drive display steppers
#define SSD_STEPS_PER_REV       (2 * UL_STEPS_PER_REV)  // Number of stepper steps per rev of mechanical display cams
#define SSD_N_MODULES           (4)                     // Number of modules in the display
#define SSD_MIN_MODULE          (0)                     // The number of the unit minutes module
#define SSD_10MIN_MODULE        (1)                     // The number of the ten minutes module
#define SSD_HOUR_MODULE         (2)                     // The number of the unit hours module
#define SSD_10HOUR_MODULE       (3)                     // The number of the ten hours module
#define SSD_PINS_PER_MODULE     (5)                     // Number of GPIO pins used by a module: 4 for stepper, one for Hall sensor
#define SSD_IN1_PIN             (0)                     // Index in pins[m][] of the IN1 pin for module m
#define SSD_IN2_PIN             (1)                     // Index in pins[m][] of the IN2 pin for module m
#define SSD_IN3_PIN             (2)                     // Index in pins[m][] of the IN3 pin for module m
#define SSD_IN4_PIN             (3)                     // Index in pins[m][] of the IN4 pin for module m
#define SSD_HALL_PIN            (4)                     // Index in pins[m][] of the Hall sensor pin for module m
#define SSD_MOVING_PAUSE_MS     (250)                   // ms to delay by while delaying to wait for display motion stop

//#define SSD_DEBUG                                       // Uncomment to turn on debug printing

class SSDisplay
{
public:

// Public instance member functions

    /**
     * @brief Construct a new SSDisplay object. Pass it an array telling it what pins each of 
     *        the four modules is attached to. Each module attaches to five pins: IN1, IN2, IN3, 
     *        IN4 (the four stepper motor connections) and one pin for the Hall-effect sensor.
     *        Module 0 is the hours tens digit. Module 1 is hours units. Module 2 is the minutes 
     *        tens digit. Module 3 is the minutes units digit.
     * 
     * @param p 
     */
    SSDisplay(byte p[SSD_N_MODULES][SSD_PINS_PER_MODULE]);

    /**
     * @brief Initialize the display, making it ready to run. Usually done once in setuo()
     * 
     * @param   o   The home offset values -- number of steps from the Hall sensor trigger to the 
     *              true home position -- to assume for each module.
     * 
     */
    void begin(int16_t o[4]);

    /**
     * @brief   Display the time specified in t.
     * 
     * @param hh    The hour (0..23) to be displayed
     * @param mm    The minute (0..59) to be displayed
     */
    void showTime(int hh, int mm);

    /**
     * @brief Set the display style to 24-hour. e.g., 13:00 or 12-hour 1:00
     * 
     * @param show24 
     */
    void setStyle24(bool style);

    /**
     * @brief Get the display style
     * 
     * @return true     The display style is 24-hour
     * @return false    The display style is 12-hour
     */
    bool styleIs24();


    /**
     * @brief   Home mechanical display module m (0..3) using the Hall effect sensor and 
     *          magnet. For convenience of construction, home is "5"
     * 
     * @param m     The module to be homed.
     */
    void home(int16_t m);

    /**
     * @brief   Get the vaulue (0..9) currently displayed in module m, m = 0..9.
     * 
     * @param m         The module whose value it to be returned.
     * @return int16_t  The value currently displayed in module m
     */
    int16_t getVal(int16_t m);

    /**
     * @brief Set the mechanical display so it shows the specified digit
     * 
     * @param m     The module to set 0..3, numbered left to right. I.e., 3 is minute units digit
     * @param val   The digit value to be displayed. No action if  val < 0 or val > 9
     */
    void setVal(int16_t m, int16_t val);

    /**
     * @brief Assume that the display is showing hh hours and mm minutes
     * 
     * @param m     The module to set 0..3, numbered left to right. I.e., 3 is minute units digit
     * @param val   The digit value to be assumed. No action if  val < 0 or val > 9
     */
    void assume(int16_t m, int16_t val);

    /**
     * @brief Move the stepper for module m by n steps without changing where it thinks it is.
     * 
     * @param m     The module to jog
     * @param n     The number of steps by which to jog
     */
    void jog(int16_t m, int16_t n);

    /**
     * @brief Return the String representation of what the display shows in the form "hh:mm"
     * 
     * @return String The String representation of the display's current state
     */
    String toString();

private:

// Private instance variables

    byte pins[4][5];                                    // The pins to which the four modules are attached
    ULN2003 module[4];                                  // The stepper for each of the four modules
    int16_t offset[4];                                  // The jog offset values for each of the four modules
    bool style24;                                       // True if the clock display style is 24 hrs. e.g., 13:00

// Private static member functions

    /**
     * @brief Return the stepper position needed to display the specified digit
     * 
     * @param val       The value for which the corresponding position is required, 0..9
     * @return int16_t  The position corresponding to the specified value
     */
    static int16_t posFor(int16_t val);

    /**
     * @brief Return the value that would be displayed if the stepper were in the 
     *        specified position.
     * 
     * @param pos       The rotational value of the display, 0..STEPS_PER_REV
     * @return int16_t  The digital value displayed when in this position
     */
    static int16_t valFor(int16_t pos);

// Private instance member functions

    /**
     * @brief Convienence function to delay until all motion in the display stops.
     * 
     */
    void delayWhileMoving();
};
