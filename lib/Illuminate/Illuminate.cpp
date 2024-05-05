/****
 * 
 * This file is a portion of the package Illuminate. See Illuminate.h for details.
 *  
 *****
 * 
 * Illuminate V1.0.1, May 2024
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
 * 
 ****/

#include <Illuminate.h>

// Public instance member functions (i.e., the API)

Illuminate::Illuminate(byte p, byte i) {
    pPin = p;
    iPin = i;
    dark = ILL_DEFAULT_DARK;
    bright = ILL_DEFAULT_BRIGHT;
}

void Illuminate::begin() {
    curLevel = 0;
    for (int8_t i = 0; i < ILL_N_AVG; i++) {
        samples[i] = analogRead(pPin);
    }
    sIx = 0;
    updateMillis = millis();
}

void Illuminate::run() {
    /*
     * Adjust the illumination level based on the current light level reading. Call often.
     * 
     * Light levels are divided into three categories: darker than dark, between dark and bright, 
     * and brighter than bright. WHen it's darker than dark, the illumination is off. When it's 
     * brighter than bright, the illumination is full on. The illumination when it's between dark 
     * and bright ramps from off to fully on in proportion to the light level.
     * 
     * 
     * 
     */
    if (millis() - updateMillis < ILL_MIN_CHG_MILLIS) {
        return;
    }
    float curAmbient = getLightLevel();
    int tgtLevel = 255 * (curAmbient - dark) / (bright - dark);
    tgtLevel = tgtLevel > 255 ? 255 : tgtLevel < 0 ? 0 : tgtLevel;
    if (tgtLevel == curLevel) {
        return;
    }
    #ifdef ILL_DEBUG
    Serial.printf("[Illuminate::run] Ambient light level is %f. Target illumination level is %d\n", curAmbient, tgtLevel);
    #endif
    curLevel = tgtLevel > curLevel ? curLevel + 1 : curLevel - 1;
    analogWrite(iPin, curLevel);
    updateMillis = millis();
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

// Private instance member functions

float Illuminate::getLightLevel() {
    samples[sIx++] = analogRead(pPin);
    if (sIx >= ILL_N_AVG) {
        sIx = 0;
    }
    int sum = 0;
    for (int8_t i = 0; i < ILL_N_AVG; i++) {
        sum += samples[i];
    }
    int avg = sum / ILL_N_AVG;
    return (1024 - (sum / ILL_N_AVG)) / 1024.0;
}