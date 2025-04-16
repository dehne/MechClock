/****
 * 
 * This file is a part of the Illuminator library. The library encapsulates an amient light sensor 
 * and two sets of LEDs that illuminate a moon phase display. One set illumintes the display while 
 * the moon is in its waxing phases, the other when it is waning. The ambient light sensor, a 
 * phototransistor, is used to adjust the brighness of the lights to "go with" the brightness of 
 * the ambient light and to turn them off completely when the ambient light falls below a minimum.
 * 
 * Because there are too many LEDs in each set to drive directly from a GPIO pin, each set is 
 * driven using a channel of a ULN2003 transistor array chip.
 * 
 * In phases 0 - 29, only the right source is turned on, thus illuminating part of the moon photo 
 * corresponding to what's lit during the waxing phases of the lunation. During the full moon 
 * transition from phase 29 to 30, during which the terminator is reset, both sources are lit. 
 * During phases 30 - 59, only the right source is lit, lighting the part corresponding to what's 
 * lit during the moon's waning phases. During the new-moon transition from phase 59 to phase 0, 
 * while the terminator is reset, neither light source is lit.
 * 
 * Illuminator uses the typical Arduino framework pattern for an encapsulated device: 
 *     1. Instantiate the object, passing it the GPIO pins it should use.
 *     2. During initialization invoke begin the object's begin() member function.
 *     3. During operation, invoke the run() member function as often as is feasible.
 *     4. As needed, invoke other member functions to adjust the object's behavior.
 * 
 *****
 * 
 * Illuminator V1.1.1, April 2025
 * Copyright (C) 2024-2025 D.L. Ehnebuske
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
#define IL_DEFAULT_MAX_DUTY     (255)           // Default duty cycle (0..255) corresponding to 100% brightness
#define IL_SENSOR_SAMPLES       (5)             // Number of ambient light sensor samples to average during a reading
#define IL_AMB_UPD_MILLIS       (1000)          // How often (millis()) to update curAmbient
#define IL_AMB_SMOOTHING        (6)             // curAmbient running average smoothing factor
#define IL_AMB_COEFF            (0.1)           // Coefficient in log scaling of ambient output
#define IL_AMB_LOWEST           (4)             // Default curAmb = this or lower ==> no lights
#define IL_AMB_HIGHEST          (75)            // Default curAmb = this or higher ==> fully bright lights

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
     * @brief Set the duty cycle corresponding to 100% brightness
     * 
     * @param newWaxingMaxDuty 
     * @param newWaningMaxDuty 
     */
    void setMaxDuty(int16_t newWaxingMaxDuty, int16_t newWaningMaxDuty);

    /**
     * @brief Set the maxDuty of waxing (waxing=true) or waning (waxing=false) COB
     * 
     * @param waxing        true => set waxing maxDuty; false => waning maxDuty
     * @param newMaxDuty    new value for maxDuty (0..ANALOG_RANGE)
     */
    void setMaxDuty(bool waxing, int16_t newMaxDuty);

    /**
     * @brief Get the current ambient light level (0 ... 100)
     * 
     * @return float    The current ambient light factor
     */
    int16_t getAmbient();

    /**
     * @brief   Set the limits between which the illuminator is lit. If lower >= upper or the 
     *          parameters are out of range, nothing happens.
     * 
     * @details Ambient light values are 0 ... 100, dark to light
     * 
     * @param lower If ambient light falls below this, the illuminator is off. Range: 0 ... 99
     * @param upper If the ambient light is above this, the illuminator is maximally lit. Range: 1 ... 100
     */
    void setAmbientLimits(int16_t lower, int16_t upper);

private:

    /**
     * @brief   Read the ambient light sensor and return its value, 0..100
     * 
     * @return int16_t The current ambient light level 0..100, 0 is dark
     */
    int16_t readAmbient();

    /**
     * @brief Map a 0..100 sensor reading into its 0.0..1.0 trimmed log-based equivalent
     * 
     * @param ambPct    The 0..100 sensor reading
     * @return float    The mapped equivalent
     */
    float ambientFactor(int16_t ambPct);

    byte waxingPin;                     // The pin controlling the set of LEDs for the waxing phases
    byte waningPin;                     // The pin controlling the set of LEDs for the waning phases
    byte sensorPin;                     // The pin to which the ambient light sensor is attached
    int16_t waxingMaxDuty;              // The duty cycle (0..255) corresponding to 100% brightness for the waxing COB
    int16_t waningMaxDuty;              // The duty cycle (0..255) corresponding to 100% brightness for the waning COB
    bool waxingIsLit;                   // True if waxing COB is lit (assuming the ambient light is bright enough)
    bool waningIsLit;                   // True if waning COB is lit (assuming the ambient light is bright enough)
    int16_t curBright;                  // The current percent (0..100) of maximum brightness to use when a COB is on
    int16_t curAmbient;                 // The current ambient brightness (0..100). 0 is dark, 100 is bright
    int16_t lowAmbient;                 // curAmbient less than this means lights off
    int16_t highAmbient;                // curAmbient more than this means lights at max brightness
    unsigned long lastAmbientMillis;    // millis() at last update of curAmbeint
};