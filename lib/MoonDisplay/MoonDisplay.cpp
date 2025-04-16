/****
 * 
 * This file is a part of the MoonDisplay library. See MoonDisplay.h for details
 *  
 *****
 * 
 * MoonDisplay V1.1.1, April 2025
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
#include <MoonDisplay.h>

static const float phasePva[60] = {     // Pivot angle (degrees) for each lunation display phase
    -78, -78, -75.5, -73.5, -71, -67, -63,  -58,  -53, -47,
    -40, -33, -25,   -18,   -10,  10,  18,   25,   33,  40,
     47,  53,  58,    63,    67,  71,  73.5, 75.5, 78,  78,
    -78, -78, -75.5, -73.5, -71, -67, -63,  -58,  -53, -47,
    -40, -33, -25,   -18,   -10,  10,  18,   25,   33,  40,
     47,  53,  58,    63,    67,  71,  73.5, 75.5, 78,  78
};

// Public instance member functions

MoonDisplay::MoonDisplay(const byte p[4], const byte l[4], const byte i[3]) {
    pvMotor = new ULN2003();
    pvPins[0] = p[0]; pvPins[1] = p[1]; pvPins[2] = p[2]; pvPins[3] = p[3]; 
    lsMotor = new ULN2003();
    lsPins[0] = l[0]; lsPins[1] = l[1]; lsPins[2] = l[2]; lsPins[3] = l[3]; 
    illum = new Illuminator {i[0], i[1], i[2]};
}

void MoonDisplay::begin(int32_t phase) {
    int32_t initPv = degToPv(phasePva[phase]);
    int32_t initLs = pvToLs(initPv);
    lsMotor->begin(lsPins[0], lsPins[1], lsPins[2], lsPins[3]);
    lsMotor->setModulus(0);
    lsMotor->setSpeed(TOP_SPEED);
    lsMotor->setLocation(initLs);
    pvMotor->begin(pvPins[0], pvPins[1], pvPins[2], pvPins[3]);
    pvMotor->setModulus(0);
    pvMotor->setSpeed(TOP_SPEED / 2);
    pvMotor->setLocation(initPv);
    tgtPhase = curPhase = phase;
    underway = resetting = false;
    illum->begin();
    illum->atPhase(curPhase);
    #ifdef MD_DEBUG
    Serial.printf("MoonDisplay::begin - Starting at phase %d.\n", phase); 
    #endif
}

int16_t MoonDisplay::run() {
    illum->run();                       // Let the Illiminator do its thing

    // If no motors are running we might need to do something
    if (!lsMotor->isMoving() && !pvMotor->isMoving()) {
        // If the current and target phases don't match, we need to move the display
        if (curPhase != tgtPhase) {
            #ifdef MD_DEBUG
            Serial.printf("MoonDisplay::run - At phase %d %s phase %d\n", curPhase, (resetting ? "resetting to" : "aiming at"), tgtPhase);
            #endif
            if (!resetting) {
                illum->toPhase(tgtPhase);
            }
            // Choose the next step on our way
            curPhase += resetting ? -1 : 1;
            // If it's a transition from phase 59 to phase 0, we need to begin by resetting to phase 30 
            if (!resetting && curPhase == 60) {
                #ifdef MD_DEBUG
                Serial.println("MoonDisplay::run - Transition 59 -> 0: Need to reset to phase 30");
                #endif
                resetTgt = tgtPhase;    // Remember the target while reset is underway
                tgtPhase = 30;          // The reset target is 30
                curPhase = 58;          // The next step from 59 toward 30 is 58
                resetting = true;
            }
            //if it's a transition from phase 29 to phase 30, need to begin by resetting to phase 0
            if (!resetting && curPhase == 30) {
                #ifdef MD_DEBUG
                Serial.println("MoonDisplay::run - Transition 29 -> 30: Need to reset to phase 0");
                #endif
                resetTgt = tgtPhase;    // Remember the target while reset is underway
                tgtPhase = 0;           // The reset target is 0
                curPhase = 28;          // The next step from 29 toward 0 is 28
                resetting = true;
            }
            int32_t pv = degToPv(phasePva[curPhase]);
            int32_t ls = pvToLs(pv);
            lsMotor->driveTo(ls);
            pvMotor->driveTo(pv);
            underway = true;
        // Otherwise we're stationary at the target phase
        } else {
            // If we're in the middle of a reset we might be done with it
            if (resetting) {
                // if we're at phase 0, that means the reset needed to get to phase 30 is complete
                if (curPhase == 0) {
                    curPhase = 30;
                    tgtPhase = resetTgt;
                    resetting = false;
                    #ifdef MD_DEBUG
                    Serial.printf("MoonDisplay::run - Reset to 0 complete. Continuing with move to %d.\n", tgtPhase);
                    #endif
                // Otherwise, if we're at phase 30, that means the reset needed to get to phase 0 is complete
                } else if (curPhase == 30) {
                    curPhase = 0;
                    tgtPhase = resetTgt;
                    resetting = false;
                    #ifdef MD_DEBUG
                    Serial.printf("MoonDisplay::run - Reset to 30 complete. Continuing with move to %d.\n", tgtPhase);
                    #endif
                }
            }
            // If we've been underway, we've just come to a stop
            if (underway) {
                underway = false;
                #ifdef MD_DEBUG
                Serial.printf("MoodDisplay::run - Now at phase %d.\n", + curPhase);
                #endif
                illum->atPhase(curPhase);
                return curPhase;                // Let the outside world know we completed a move to curPhase
            }
        }
    }
    return -1;                                  // Let the outside world know nothing exciting happened
}

boolean MoonDisplay::showPhase(int16_t phase) {
    if (resetting || pvMotor->isMoving() || lsMotor->isMoving()) {
        #ifdef MD_DEBUG
        Serial.println("MoonDisplay::showPhase - Tried to move to next phase while display is moving.");
        #endif
        return false;
    }
    if (phase > sizeof(phasePva)/sizeof(phasePva[0]) || phase < 0) {
        #ifdef MD_DEBUG
        Serial.printf("MoonDisplay::showPhase: Phase (%d) out of bounds; ignored.\n", phase);
        #endif
        return false;
    }
    tgtPhase = phase;
    return true;
}

int16_t MoonDisplay::getPhase() {
    return curPhase;
}

void MoonDisplay::assume(int16_t phase) {
    int32_t pvLoc = degToPv(phasePva[phase]);
    int32_t lsLoc = pvToLs(pvLoc);
    pvMotor->setLocation(pvLoc);
    lsMotor->setLocation(lsLoc);
    curPhase = phase;
    tgtPhase = phase;
    illum->atPhase(curPhase);
    #ifdef MD_DEBUG
    Serial.printf("MoonDisplay::assume - Assuming display shows phase %d; pv: %d, ls: %d.\n", phase, pvLoc, lsLoc);
    #endif
}

int32_t MoonDisplay::getLs() {
    return lsMotor->getLocation();
}

void MoonDisplay::turnLs(int32_t steps) {
    lsMotor->setLocation(lsMotor->getLocation() - steps);
    lsMotor->drive(steps);
}

int32_t MoonDisplay::getPv() {
    return pvMotor->getLocation();
}

void MoonDisplay::turnPv(int32_t steps) {
    pvMotor->setLocation(pvMotor->getLocation() - steps);
    pvMotor->drive(steps);
}

void MoonDisplay::stop() {
    lsMotor->stop();
    pvMotor->stop();
    tgtPhase = curPhase;
    resetting = false;
}

int16_t MoonDisplay::getAmbient() {
    return illum->getAmbient();
}

void MoonDisplay::setAmbientLimits(int16_t lower, int16_t upper) {
    illum->setAmbientLimits(lower, upper);
}