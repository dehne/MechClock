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
 * Illuminator V1.1.0, June 2024
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

#define IL_ANALOG_WRITE_FREQ    (2000)          // The frequency (Hz) to use for the PWM signal
#define IL_ANALOG_RANGE         (1000)          // analogWite with this value is 100% duty cycle
#define IL_ANALOG_READ_RES      (12)            // The analog read resolution (bits)
#define IL_ANALOG_FULLSCALE     ((1 << IL_ANALOG_READ_RES) - 1) // The max possible reading on the analog sensor
#define IL_DEFAULT_MAX_DUTY     (500)           // Default duty cycle corresponding to 100% brightness
#define IL_SENSOR_SAMPLES       (5)             // Number of ambient light sensor samples to average during a reading
#define IL_AMB_UPD_MILLIS       (1000)          // How often (millis()) to update curAmbient
#define IL_AMB_SMOOTHING        (6)             // curAmbient running average smoothing factor
#define IL_AMB_COEFF            (0.1)           // Coeefficient in log scaling of ambient output
#define IL_AMB_LOWEST           (4)             // curAmb = this or lower ==> no lights
#define IL_AMB_HIGHEST          (75)            // curAmb = this or higher ==> fully bright lights

//#define IL_DEBUG                                // Uncomment to enable debugging code

class Illuminator {
public:
    /**
     * @brief Construct a new Illuminator object
     * 
     * @param pin1  The GPIO pin to which the ULN2003 channel controlling the waxing LEDs is attached
     * @param pin2  The GPIO pin to which the ULN2003 channel controlling the waning LEDs is attached
     * @param pin3  The GPIO pin to which the phototransistor ambient light sensor is attached
     */
    Illuminator(byte pin1, byte pin2, byte pin3);

    /**
     * @brief Initialize the Illuminator, making it ready for operation
     * 
     */
    void begin();

    /**
     * @brief Let the Illuminator do its thing
     * 
     */
    void run();

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
     * @brief Get the current ambient light factor (1.0 .. 0.0)
     * 
     * @return float    The current amboent light factor
     */
    float getAmbient();

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

    /**
     * @brief   Read the ambient light sensor and return its value, 0..100
     * 
     * @return int16_t The current ambient light level 0..100, 0 is dark
     */
    int16_t readAmbient();

    byte waxingPin;                     // The pin controlling the set of LEDs for the waxing phases
    byte waningPin;                     // The pin controlling the set of LEDs for the waning phases
    byte sensorPin;                     // The pin to which the ambient light sensor is attached
    int16_t waxingMaxDuty;              // The duty cycle corresponding to 100% brightness for the waxing COB
    int16_t waningMaxDuty;              // The duty cycle corresponding to 100% brightness for the waning COB
    bool waxingIsLit;                   // True if waxing COB is lit (when the ambient light is bright enough)
    bool waningIsLit;                   // True if waning COB is lit (when the ambient light is bright enough)
    int16_t curBright;                  // The current percent of maximum brightness to use when a COB is on
    int16_t curAmbient;                 // The current ambient brightness 0..100. 0 is dark, 100 is bright
    unsigned long lastAmbientMillis;    // millis() at last update of curAmbeint
};