/****
 * 
 * This file is a part of the Illuminator library. See Illuminator.h for details
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

#include <Illuminator.h>

/**
 * Bit masks for whether the waxing (Wx) and waning (Wn) illumination is on (1) or off (0) for 
 * when the phase is changing (to) or at (at) a given phase. So, toWx bit 20 from the right is
 * a 1, indicating that when moving from phase 19 to 20, the waxing illumination is on.
 */
//                               5         4         3         2         1         0
//                      987654321098765432109876543210987654321098765432109876543210
const uint64_t toWx = 0b000000000000000000000000000001111111111111111111111111111110;
const uint64_t toWn = 0b011111111111111111111111111111000000000000000000000000000000;
const uint64_t atWx = 0b000000000000000000000000000000111111111111111111111111111110;
const uint64_t atWn = 0b011111111111111111111111111111000000000000000000000000000000;

Illuminator::Illuminator(byte pin1, byte pin2) {
    waxingPin = pin1;
    waningPin = pin2;
}

void Illuminator::begin() {
    analogWriteFreq(ANALOG_WRITE_FREQ);
    analogWriteRange(ANALOG_RANGE);
    pinMode(waxingPin, OUTPUT);
    digitalWrite(waxingPin, LOW);
    pinMode(waningPin, OUTPUT);
    digitalWrite(waningPin, LOW);
    curBright = 100;
    waxingMaxDuty = DEFAULT_MAX_DUTY;
    waningMaxDuty = DEFAULT_MAX_DUTY;
    #ifdef IL_DEBUG
    Serial.printf("Illuminator::begin - Illuminator waxing pin: %d, waning pin: %d.\n", waxingPin, waningPin);
    #endif
}

void Illuminator::toPhase(int16_t phase) {
    phase = phase < 0 ? 0 : phase >= 60 ? 59 : phase;
    float b = curBright / 100.0;
    analogWrite(waxingPin, (toWx >> phase) & 1 ? (int16_t)(b * waxingMaxDuty) : 0);
    analogWrite(waningPin, (toWn >> phase) & 1 ? (int16_t)(b * waningMaxDuty) : 0);
    #ifdef IL_PHASE_DEBUG
    Serial.printf("Illuminator::toPhase - Illuminator to phase %d. waxing %s waning %s\n", 
        phase, (toWx >> phase) & 1 ? "on" : "off", (toWn >> phase) & 1 ? "on" : "off");
    #endif
}

void Illuminator::atPhase(int16_t phase) {
    phase = phase < 0 ? 0 : phase >= 60 ? 59 : phase;
    float b = curBright / 100.0;
    analogWrite(waxingPin, (atWx >> phase) & 1 ? (int16_t)(b * waxingMaxDuty) : 0);
    analogWrite(waningPin, (atWn >> phase) & 1 ? (int16_t)(b * waningMaxDuty) : 0);
    #ifdef IL_PHASE_DEBUG
    Serial.printf("Illuminator::atPhase - Illuminator at phase %d. waxing %s waning %s\n", 
        phase, (atWx >> phase) & 1 ? "on" : "off", (atWn >> phase) & 1 ? "on" : "off");
    #endif
}

int16_t Illuminator::getBright() {
    return curBright;
}

void Illuminator::setBright(int16_t bright) {
    if (bright >= 0 && bright <= 100) {
        curBright = bright;
    #ifdef IL_DEBUG
    float b = curBright / 100.0;
    Serial.printf("Illuminator::setBright - Setting brightness to %d%, waxingDutyCycle: %d, waningDutyCycle: %d\n",
                  curBright, (int16_t)(b * waxingMaxDuty), (int16_t)(b * waningMaxDuty));
    } else {
        Serial.printf("Illuminator::setBright - Brightness %d out of range; ignored.\n", bright);
    #endif
    }
}

int16_t Illuminator::getMaxDuty(bool waxing) {
    return waxing ? waxingMaxDuty : waningMaxDuty;
}

void Illuminator::setMaxDuty(int16_t newWaxingMaxDuty, int16_t newWaningMaxDuty) {
    if (newWaxingMaxDuty >= 0 && newWaxingMaxDuty <= ANALOG_RANGE && newWaningMaxDuty >= 0 && newWaningMaxDuty <= ANALOG_RANGE) {
        waxingMaxDuty = newWaxingMaxDuty;
        waningMaxDuty = newWaningMaxDuty;
    #ifdef IL_DEBUG
    } else {
        Serial.printf("Illuminator::setMaxDuty - Duty cycle out of range 0 .. %d: %d, %d\n", 
          ANALOG_RANGE, newWaxingMaxDuty, newWaningMaxDuty);
    #endif
    }
}

void Illuminator::setMaxDuty(bool waxing, int16_t newMaxDuty) {
    if (newMaxDuty >= 0 && newMaxDuty <= ANALOG_RANGE) {
        if (waxing) {
            waxingMaxDuty = newMaxDuty;
        } else {
            waningMaxDuty = newMaxDuty;
        }
    #ifdef IL_DEBUG
    } else {
        Serial.printf("Illuminator::setMaxDuty - maxDuty out of range 0 .. %d for %s: %d\n", 
          ANALOG_RANGE, waxing ? "waxing" : "waning", newMaxDuty);
    #endif
    }
}