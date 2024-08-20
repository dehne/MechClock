/****
 * 
 * This file is a part of the Illuminator library. The library encapsulates two sets of LEDs that 
 * illuminate a moon phase display. One set illumintes the display while the moon is in its waxing 
 * phases, the other when it is waning.
 * 
 * Because there are too many LEDs in each set to drive directly from a GPIO pin, each set is 
 * driven using a channel of a ULN2003 transistor array chip.
 * 
 *****
 * 
 * Illuminator V1.0.0, June 2024
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

#define ANALOG_WRITE_FREQ   (2000)              // The frequency (Hz) to use for the PWM signal
#define ANALOG_RANGE        (1000)              // analogWite with this value is 100% duty cycle
#define DEFAULT_MAX_DUTY    (500)               // Default duty cycle corresponding to 100% brightness

//#define IL_DEBUG                    // Uncomment to enable debugging code

class Illuminator {
public:
    /**
     * @brief Construct a new Illuminator object
     * 
     * @param pin1  The GPIO pin to which the ULN2003 channel controlling the waxing LEDs is attached
     * @param pin2  The GPIO pin to which the ULN2003 channel controlling the waning LEDs is attached
     */
    Illuminator(byte pin1, byte pin2);

    /**
     * @brief Initialize the Illuminator, making it ready for operation
     * 
     */
    void begin();

    /**
     * @brief Illuminate the display appropriately for moving to the specified phase
     * 
     * @param phase 
     */
    void toPhase(int16_t phase);

    /**
     * @brief Illuminate the display appropriately while at the specified phase
     * 
     * @param phase 
     */
    void atPhase(int16_t phase);

    /**
     * @brief Get the current brightness
     * 
     * @return int16_t  The current brightness, (0..100)
     */
    int16_t getBright();

    /**
     * @brief Set the current brightness in percent (0 .. 100) of maximum to use
     * 
     * @param bright    Percent (0..100) of maximum
     */
    void setBright(int16_t bright);

    /**
     * @brief Get the current maxDuty for waxing or waning COB
     * 
     * @param waxing    true => get maxDuty for waxing COB, false for waning COB
     * @return int16_t 
     */
    int16_t getMaxDuty(bool waxing);

    /**
     * @brief Set the duty cycle corresponding to 100% brightness
     * 
     * @param newWaxingMaxDuty 
     * @param newWaningMaxDuty 
     */
    void setMaxDuty(int16_t newWaxingMaxDuty, int16_t newWaningMaxDuty);

    /**
     * @brief Set the maxDuty of waxing (wxing=true) or waning (waxing=false) COB
     * 
     * @param waxing        true => set waxing maxDuty; false => waning maxDuty
     * @param newMaxDuty    new value for maxDuty (0..ANALOG_RANGE)
     */
    void setMaxDuty(bool waxing, int16_t newMaxDuty);

private:
    byte waxingPin;             // The pin controlling the set of LEDs for the waxing phases
    byte waningPin;             // The pin controlling the set of LEDs for the waning phases
    int16_t waxingMaxDuty;      // The duty cycle corresponding to 100% brightness for the waxing COB
    int16_t waningMaxDuty;      // The duty cycle corresponding to 100% brightness for the waning COB
    int16_t curBright;          // The current percent of maximum brightness to use when a COB is on
};