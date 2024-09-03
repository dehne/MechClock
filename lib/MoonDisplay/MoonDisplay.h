/****
 * 
 * This file is a part of the MoonDisplay library. The library encapsulates a moon phase display 
 * consisting of a picture of the full moon, on top of which there lies a springy, flexible 
 * "terminator" running between two pivots placed at the northernmost and southernmost points of 
 * the moon photo. The northern one is free to turn. One end of the terminator is attached to this 
 * pivot. The terminator material passes across the photo to the southern pivot. It slides through 
 * that pivot and continues beyond it. The southern pivot can be twisted by a stepper motor 
 * (pvMotor). This twists the terminator material, settting the angle it makes with the southern 
 * extremity of the photo. The northern, freely rotating, pivot assumes whatever angle the forces 
 * on it dictate. 
 * 
 * As noted, the material forming the terminator is longer than the distance from the northern 
 * pivot to the southern one. After it passes through the southern (driven) pivot, its end is 
 * attached to the travelling part of a leadscrew mechanism the whole of which rotates with the 
 * pivot. The leadscrew mechanism, driven by another stepper motor (lsMotor) pushes or pulls the 
 * terminator material, sliding it through the pivot. Thus the position of the leadscrew 
 * determins the length of the terminator material the runs across the photo between the two 
 * pivots. 
 * 
 * By coordinating the operation of the two steppers we can form the terminator into various 
 * curves crossing the face of the photo. By doing so correctly we can approximate the the curves 
 * real terminator takes as it crosses the face of the real moon. Well, close enough to make a 
 * nice mechanical moon phase display, anyway.
 * 
 * Based on calibration runs of the as-built mechanism, if the position of the leadscrew (ls) as 
 * a function of pivot position (pv) is
 * 
 *      ls = 497671 + 30.5pv - 0.201pv^2
 * 
 * where -1600 <= pv <= 1600, the result is a (semi-credible) terminator path across the moon 
 * photo. (Here, ls is a measure of how much terminator material is stored in the leadscrew 
 * mechanism, so the length across the photo in inversely proportional to it. Positions of both 
 * motors is measured in steps.) 
 * 
 * For the purposes of the display, I've divided a lunation into 60 phases. Phase 0 is a new moon, 
 * phase 16 is the first quarter, phase 30 is the full moon and phase 45 is the third quarer moon. 
 * The transition from phase 59 to 0 brings us back to the new moon of the next lunation.
 * 
 * At the start of a lunation (new moon -- phase 0), pv is at its minimum (-1600) and the 
 * terminator is at its longest and is bent strongly to the right (the display is for earth's 
 * northern hemisphere). As time progresses, the phases increase step by step, showing the waxing 
 * crescent progression. At each phase, the steppers are operated to form the appropriate 
 * terminator. During the transition from phase 15 to 16, the terminator begins to bend to the 
 * left marking the progress of the gibbous moon until, at phase 29, it is at its maximum, bent 
 * strongly to the left at full moon. The transition from phase 29 to 30 involves moving the 
 * terminator from full left extension to full right so that it is in position to show the start 
 * of the waning gibbous moon. As the phases continue, the terminator makes its way across the 
 * moon photo, marking the lunation's waning phases, until, at the transition from phase 59 to 0, 
 * we're back at the new moon. Like the transition from phase 29 to 30, the transition from phase 
 * 59 to 0 requires the terminator to be reset from its extreme left position to its extreme 
 * right one.
 * 
 * In addition to the terminator and moon photo, the display has two low angle light sources that 
 * shine aross the moon photo, one from right to left, and the other from left to right. The 
 * terminator material stands tall enough above the photo to keep the light from shining on the 
 * part of the photo on the side away from the light source. Thus when one light source is on and 
 * the other is off, the part of the moon photo on the same side is illuminated, but the part of 
 * the photo on the other side is in (relative) darkness. 
 * 
 * In phases 0 - 29, only the right source is turned on, thus illuminating part of the moon phot 
 * corresponding to what's lit during the waxing phases of the lunation. During the full moon 
 * transition from phase 29 to 30, during which the terminator is reset, both sources are lit. 
 * During phases 30 - 59, only the right source is lit, lighting the part corresponding to what's 
 * lit during the moon's waning phases. During the new-moon transition from phase 59 to phase 0, 
 * while the terminator is reset, neither light source is lit.
 * 
 *****
 * 
 * MoonDisplay V1.1.0, June 2024
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
#include <Illuminator.h>

/**
 * 
 * Compile-time constants
 * 
 **/
//#define MD_DEBUG                            // Uncomment for debug printing
#define TOP_SPEED               (600)       // Top speed for steppers

class MoonDisplay {
public:
    /**
     * @brief Construct a new Moon Display object attached to the specified GPIO pins.
     * 
     * @param p The GPIO pins for the pv stepper: {pvIn1, pvIn2,pvIn3, pvIn4}
     * @param l The GPIO pins for the ls stepper: {lsIn1, lsIn2, lsIn3, lsIn4}    
     * @param i The GPIO pins for the illuminator: {ilIn1, ilIn2, ilIn3}
     */
    MoonDisplay (const byte p[4], const byte l[4], const byte i[3]);

    /**
     * @brief   Initialize the MoonDisplay assuming it is currently displaying the specified 
     *          phase. Typically called once in startuo().
     * 
     * @param   phase   The phase the display should assume it as currently showing.
     */
    void begin(int32_t phase);

    /**
     * @brief Let the MoonDisplay do its thing. Call frequently.
     * 
     * @return int16_t  0 - 59 if just finished moving the that phase. -1 otherwise
     */
    int16_t run();

    /**
     * @brief   Move cyclcally through the phases to the specified phase
     * 
     * @param phase     The phase to move to
     * @return boolean  true if success. false if failed because phase was out of bounds or 
     *                  display was moving
     */
    boolean showPhase(int16_t phase);

    /**
     * @brief   Return the current phase showing in the MoonDisplay
     * 
     * @return int16_t 
     */
    int16_t getPhase();

    // Low-level and debugging member functions. Use with caution!

    /**
     * @brief   For debugging: Assume that the display is now showing the specified phase, 
     *          setting the reported LS and PV positions to match the phase
     * 
     * @param phase 
     */
    void assume(int16_t phase);

    /**
     * @brief For debugging: Get the position of the leadscrew stepper in steps
     * 
     * @return int32_t  The current position of the leadscrew stepper in steps
     */
    int32_t getLs();

    /**
     * @brief   For debugging: Turn the leadscrew stepper by the specified number of steps without 
     *          altering its reported position
     * 
     * @param steps 
     */
    void turnLs(int32_t steps);

    /**
     * @brief Get the position of the pivot stepper in steps
     * 
     * @return int32_t  The positio of the pivot stepper in steps
     */
    int32_t getPv();

    /**
     * @brief   For debugging: Turn the pivot stepper by the specified number of steps without 
     *          altering its reported position
     * 
     * @param steps 
     */
    void turnPv(int32_t steps);

    /**
     * @brief   For debugging: Stop all motion now, and be at peace with whererver that is
     * 
     */
    void stop();

private:
    ULN2003 *pvMotor;                       // Pointer to the pv stepper motor
    ULN2003 *lsMotor;                       // Pointer to the ls stepper motor
    Illuminator *illum;                     // Pointer to the illuminator device

    byte pvPins[4];                         // GPIO pins pvMotor is attached to
    byte lsPins[4];                         // GPIO pins lsMotor is attached to
    unsigned long nextPhaseChangeMillis;    // millis() at next phase change
    int16_t curPhase;                       // The phase we're at currently (or were if we're now moving)
    int16_t tgtPhase;                       // The phase we're working to get to
    boolean resetting = false;              // true when driving the display backwards to go from ph 29 to 30 or 59 to 0
    boolean underway;                       // true when we're moving to the next phase
    int32_t resetTgt;                       // The stash for tgtPhase during reset operations

/**
 * @brief   Return the position (in steps) the leadscrew should have given the positon (in steps) 
 *          of the pivot to form a good-looking moon terminator. 
 * 
 * @note    The assumption that -1600 <= pv <= 1600 is not checked.
 * 
 * @details This functon is based on curve fitting calibration data. When pv is 0, the terminator 
 *          is a straight vertical line with 497,671 steps (each step is 1/131072") worth of 
 *          material to be pushed out. The curve formed when pv = 1600 is all the way to the left 
 *          rim of the moon photo (approximately), so not on the displayed face. The curve is 
 *          mirror symmetrical around pv = 0.
 * 
 * @param pv    The position (in steps) of the pivot
 * @return int32_t 
 */
static int32_t pvToLs(int32_t pv) {
    return pv >= 0 ? (int32_t)(497671.0 + 30.5 * pv - 0.201 * pv * pv) : (int32_t)(497671.0 - 30.5 * pv - 0.201 * pv * pv);
}

/**
 * @brief   Return the position (in steps) of the pivot given its angle in degrees.
 * 
 * @note    The assumtion that -78.125 <= angle <= 78.125 is not checked.
 * 
 * @details The pivot is driven by a 4096 step/turn stepper using a toothed belt drive with a 
 *          20-tooth pullet on the stepper and a 32-tooth pulley on the pivot.
 * 
 * @param angle 
 * @return int32_t 
 */
static int32_t degToPv(float angle) {
    return (int32_t)(20.48 * angle);
}


};