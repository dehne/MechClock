/****
 * 
 * This file is a portion of the package Illuminate. See Illuminate.h for details.
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

#include <Illuminate.h>

/**
 * @brief   Get the light level 0.0 (completely dark) to 1.0 (blindingly bright)
 * 
 * @param pin       The analog GPIO pin to which the light sensor is attached. 
 *                  Reads max (1024) with no light and min(0) in light.
 * @return float 
 */
float getLightLevel(byte pin) {
    return (1024 - analogRead(pin)) / 1024.0;
}

// Public instance member functions (i.e., the API)

Illuminate::Illuminate(byte p, byte i) {
    pPin = p;
    iPin = i;
    dark = ILL_DEFAULT_DARK;
    bright = ILL_DEFAULT_BRIGHT;
}

void Illuminate::run() {
    /*
     * Adjust the illumination level based on the current light level reading. 
     * 
     * Light levels are divided into three categories: darker than dark, between dark and bright, 
     * and brighter than bright. WHen it's darker than dark, the illumination is off. When it's 
     * brighter than bright, the illumination is full on. The illumination when it's between dark 
     * and bright ramps from off to fully on in proportion to the light level.
     * 
     * It may be that I need to add a bit of histerisis if the illuminaton flickers back and forth 
     * between off and barely on.
     * 
     */
    float lightLevel = getLightLevel(pPin);
    int level = 255 * (lightLevel - dark) / (bright - dark);
    level = level > 255 ? 255 : level < 0 ? 0 : level;
    analogWrite(iPin, level);
    #ifdef ILL_DEBUG
    Serial.printf("[Illuminate::run] The light level is %f. Illumination level is %d\n", lightLevel, level);
    #endif
}

bool Illuminate::setAmbientBounds(float darkToLamps, float lampsToDay) {
    if (darkToLamps < 0.0 || lampsToDay < darkToLamps || lampsToDay > 1.0) {
        return false;
    }
    dark = darkToLamps;
    bright = lampsToDay;
    run();
    return true;
}

float Illuminate::getDarkToLamps() {
    return dark;
}

float Illuminate::getLampsToDay() {
    return bright;
}