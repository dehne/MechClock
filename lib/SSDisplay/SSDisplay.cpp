/****
 * 
 * This file is a portion of the package SSDisplay. See SSDisplay.h for details.
 *  
 *****
 * 
 * SSDisplay V1.0.0, April 2024
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

#include <SSDisplay.h>

// Public instance member functions

SSDisplay::SSDisplay(byte p[SSD_N_MODULES][SSD_PINS_PER_MODULE]) {
    for (int16_t m = 0; m < SSD_N_MODULES; m++) {
        for (int16_t n = 0; n < SSD_PINS_PER_MODULE; n++) {
            pins[m][n] = p [m][n]; // a safe and clear but clumsy way of saying "pins = p" in C++
        }
    }
}

void SSDisplay::begin(int16_t o[4]) {
    for (int16_t m = 0; m < SSD_N_MODULES; m++) {
        module[m].begin(pins[m][SSD_IN1_PIN], pins[m][SSD_IN2_PIN], pins[m][SSD_IN3_PIN], pins[m][SSD_IN4_PIN]);
        module[m].setModulus(SSD_STEPS_PER_REV);
        module[m].setSpeed(SSD_STEPS_PER_SEC);
        pinMode(pins[m][SSD_HALL_PIN], INPUT_PULLUP);
        offset[m] = o[m];
        home(m);
    }
    style24 = false;
}

void SSDisplay::showTime(int hh, int mm) {
        setVal(SSD_MIN_MODULE, mm % 10);
        setVal(SSD_10MIN_MODULE, mm / 10);
        if (!style24) {
            if (hh == 0) {
                hh = 12;
            } else if (hh > 12) {
                hh -= 12;
            }
        }
        setVal(SSD_HOUR_MODULE, hh % 10);
        setVal(SSD_10HOUR_MODULE, hh / 10);
}

void SSDisplay::setStyle24(bool style) {
    style24 = style;
}

bool SSDisplay::styleIs24() {
    return style24;
}

void SSDisplay::home(int16_t m) {
    module[m].setLocation(0);
    // If we're atop the Hall sensor magnet, rotate the module stepper clockwise until we're off it
    if (digitalRead(pins[m][SSD_HALL_PIN]) == LOW) {
        module[m].drive(SSD_STEPS_PER_REV - 1);
        while (digitalRead(pins[m][SSD_HALL_PIN]) == LOW && module[m].isMoving()) {
            // Spin
        }
        module[m].stop();
        if (digitalRead(pins[m][SSD_HALL_PIN]) == LOW) {
            Serial.printf("[SSDisplay.home] Unable to move module %d away from the position sensor.\n", m);
            return;
        }
    }

    // Rotate module stepper clockwise until we sense the Hall sensor magnet
    module[m].drive(SSD_STEPS_PER_REV - 1);
    while (digitalRead(pins[m][SSD_HALL_PIN]) == HIGH && module[m].isMoving()) {
        // Spin
    }
    module[m].stop();
    if (digitalRead(pins[m][SSD_HALL_PIN]) == HIGH) {
        Serial.printf("[SSDisplay.home] Unable to move module %d to the position sensor.\n", m);
        return;
    }
    // We're home!
    jog(m, offset[m]);
    module[m].setLocation(posFor(5));
}

int16_t SSDisplay::getVal(int16_t m) {
    return valFor(module[m].getLocation());
}

void SSDisplay::setVal(int16_t m, int16_t val) {
    if (!(val >= 0 && val <= 9)) {
        Serial.print("Failed assertion in displayVal\n");
        while(true) {
            // Bye bye
        }
    }
    module[m].driveTo(posFor(val));
    delayWhileMoving();
}

void SSDisplay::assume(int16_t m, int16_t val) {
    module[m].setLocation(posFor(val));
}

void SSDisplay::jog(int16_t m, int16_t n) {
    int16_t curLoc = module[m].getLocation();
    module[m].drive(n);
    while (module[m].isMoving()) {
        // Spin
    }
    module[m].setLocation(curLoc);
    #ifdef SSD_DEBUG
    Serial.printf("[SSDisplay::jog] Jogged module %d by %d steps. Starting location is %d. Current location is %d.\n", 
        m, n, curLoc, module[m].getLocation());
    #endif
}

String SSDisplay::toString() {
        String answer = "";
        for (int16_t i = 3; i >= 0; i--) {
        answer += String(valFor(module[i].getLocation())) + (i == 2 ? ":" : "");
    }
    return answer;
}

// Private static member functions

int16_t SSDisplay::posFor(int16_t val) {
    return (((SSD_STEPS_PER_REV * val) / 10)) % SSD_STEPS_PER_REV;
}

int16_t SSDisplay::valFor(int16_t pos) {
    return (((10 * (pos + SSD_STEPS_PER_REV / 20)) / SSD_STEPS_PER_REV)) % 10;
}

// Private instance member functions

void SSDisplay::delayWhileMoving() {
    while (module[SSD_MIN_MODULE].isMoving() || 
           module[SSD_10MIN_MODULE].isMoving() || 
           module[SSD_HOUR_MODULE].isMoving() || 
           module[SSD_10HOUR_MODULE].isMoving()) {
        delay(SSD_MOVING_PAUSE_MS);
    }
}
