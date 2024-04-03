/****
 * 
 * This file is a portion of the package Illuminate. It encapsulates a illumination control system 
 * consisting of a phototransistor to determine the brightess of the surroundings and a 
 * PWM-controlled array of LEDs used to illuminate a display. The idea is to supply enough 
 * illumination to the display to make it easily visible in daylight but to reduce the amount of 
 * illumination to nothing in darkness so as to not bother folks when they've turned the lights 
 * off.
 * 
 *****
 * 
 * Illuminate V1.0.0, April 2024
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

/**
 * 
 * Compile-time constants
 * 
 **/
#define ILL_DEFAULT_DARK    (0.020)                 // If the light level falls below this, it's dark; turn off illumination
#define ILL_DEFAULT_BRIGHT  (0.125)                 // If the light level rises above thus, its bright; max out illumination

//#define ILL_DEBUG                                   // Uncomment to enable debug printing

class Illuminate
{
public:
    /**
     * @brief Construct a new Illuminate object
     * 
     * @param p      The analog-capable GPIO pin to which the phototransistor is attached
     * @param i      The PWM-capable GPIO pin to which the LED light strip is attached
     */
    Illuminate(byte p, byte i);

    /**
     * @brief   Let Illuminate do its thing. Invoke as often as desired to control the level of 
     *          illumination being produced.
     */
    void run();

    /**
     * @brief   Set the boundaries between ambient light categories. darkToLamps is the point on a 
     *          scale of 0.0 .. 1.0 at which "dark" transitions to "lamps". Below this boundary, 
     *          the illumination is turned off completely. lampsToDay is the point on a scale of 
     *          0.0 .. 1.0 at which "lamps" gives way to "day". Above this boundary, the 
     *          illumination is fully on. 
     * 
     * @param darkToLamps   Boundary position, where 0.0 <= darkToLamps <= lampsToDay
     * @param lampsToDay    Boundary position, where darkToLamps <= lampsToDay <= 1.0
     * @return true         New boundaries set successfully
     * @return false        Invalid new boundaries specified; boundries unchanged
     */
    bool setAmbientBounds(float darkToLamps, float lampsToDay);

    /**
     * @brief Get the value of the darkToLamps boundary
     * 
     * @return float    The current darkToLamps boundary value
     */
    float getDarkToLamps();

    /**
     * @brief Get the value of the lampsToDay boundary
     * 
     * @return float    The current lampsToDay boundary value
     */
    float getLampsToDay();

private:
    /**
     * @brief Instance variables
     * 
     */
    byte pPin;                  // The GPIO pin the phototransistor is attached to
    byte iPin;                  // The GPIO pin the LED strip is attached to
    float dark;                 // The light level below which it's considered dark
    float bright;               // The light level above which it's considered bright
};
