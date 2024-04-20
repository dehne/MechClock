/****
 * 
 * This file is a portion of the package ULN2003Pico, a library that provides an Arduino sketch 
 * running on a Raspberry Pi Pico or Pico-W with the ability to control an array of 28BYJ-48 
 * four-phase, eight-beat variable-reluctance stepper motors driven through ULN2003 Darlington 
 * array chips. 
 * 
 * It is derived from my UNL2003 library for the Arduino uno-like boards and has the same 
 * programming interface. 
 * 
 * The driving is pretty naive in the sense that each motor is either running at its assigned 
 * speed or it's stopped. No acceleration or deceleration is done. This works well for all the 
 * cases I've so far been interested in.
 * 
 * The the windings of the 28BYJ-48 motor are as follows:
 * 
 *                                  O -------.
 *      ULN2003     Winding                 /
 *      INx         Wire Color              \     /-------\
 *      -----------+----------              /    |         |
 *      1          | Blue   (Bu)      .----+     |  Rotor  |
 *      2          | Pink   (Pk)      |     \    |         |
 *      3          | Yellow (Y)       |     /     \-------/
 *      4          | Orange (O)       |     \    
 *                               Pk --|-----.
 *                                    |  Y --------\/\/\/-------- Bu
 *                                    |               |
 *                          R (+5v) --+---------------.
 * 
 * To drive the motor in a clockwise direction the ULN2003 Darlington pairs (INx) are turned on 
 * and off in the following cyclic sequence:
 * 
 *      ULN2003     Phase
 *      INx         0   1   2   3   4   5   6   7
 *      ----------+------------------------------
 *              1   *   *                       *
 *              2                       *   *   *
 *              3               *   *   *
 *              4       *   *   *
 * 
 * To drive the motor counterclockwise go through the sequence in ascending order. To go clockwise 
 * simply reverse the cyclic sequence.
 * 
 * The 28BYJ-48 motors are geared down a lot. The net effect is that after going through 4096 
 * phase changes, the output shaft makes one full turn. Because they are geared down so much, 
 * their output shaft doesn't turn when the power is off. (At least, they don't when I apply as 
 * much torque as I dare to apply.) We take advantage of that by turning off the power to all 
 * the windings when a motor is stopped.
 * 
 * To get the timing of the pulses as close to correct as possible, the actual motor driving is 
 * done in a "repeating timer callback" provided by the Raspberry 
 * 
 * The basic approach to using the library is:
 * 
 *      1. Instantiate one or more ULN2003 objects. (The maximum number is set by the compile-time 
 *         constant UL_MAX_OBJECTS.)
 *      2. In the sketch's setup() function, invoke begin() on each of the objects to initialize 
 *         things.
 *      3. Invoke the operational member functions to control the motors 
 *          - drive(dir, nSteps)        Move nSteps steps in direction dir
 *          - stop()                    Stop moving now
 *          - setLocation(steps)        Rebase getLocation() to steps
 *          - setSpeed(stepsPerSec)     Set the rate for taking steps
 *          - setModulus(steps)         Set the number of steps in a turn
 *      4. Invoke the informational member functions to get status information
 *          - steps = getLocation()     CW is positive, CC is negative
 *          - speed = getSpeed()        per setSpeed(); UL_SPEED is default
 *          - getStepsToGo()            Number of steps still to take
 *          - aBool = isMoving()        true if taking steps
 *****
 * 
 * ULN2003Pico V1.0.1, April 2024
 * Copyright (C) 2020-2024 D.L. Ehnebuske
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
#define UL_MAX_OBJECTS          (4)         // Maximum number of ULN2003 objects
#define UL_DEFAULT_SPEED        (600)       // Default speed in steps per second
#define UL_STEPS_PER_REV        (4096)      // The number of steps in one output shaft revolution
#define UL_MAX_JITTER           (75)        // Maximum jitter (μs) in dispatching steps
#define UL_TIMER_INTERVAL       (512)       // Interval (μs) between invocations of timer callback

//#define UL_DEBUG                            // Uncomment to enable debug code

class ULN2003 {
    /**
     * 
     * Make a new ULN2003-driven stepper motor.
     * 
     * 
     **/
    public:
        /**
         * 
         * Make a new instance of a ULN2003-controlled motor
         * 
         **/
        ULN2003();

        /**
         * 
         * Initialize the motor with motor controller pin IN1  connected to pin1, 
         * IN2 to pin2, IN3 to pin3 and IN4 to pin4
         * 
         * Put a call to begin() in your setup() function to get things set up 
         * with the motor.
         * 
         **/
        void begin(byte pin1, byte pin2, byte pin3, byte pin4);

        /**
         * 
         * Drive the motor nSteps steps, clockwise if nSteps is > 0, 
         * counterclockwise if < 0. If location has a modulus, the actual 
         * number of steps taken will be modulo that number. Nothing happens 
         * when nsteps is (effectively) 0.
         * 
         * The drive() member function does not block. During the time that 
         * steps are being taken, invoking drive() again adds to or subtracts 
         * from the number of steps still to be taken. Similarly invoking 
         * driveTo() while the motor is running changes where the motor ends 
         * up. 
         * 
         **/
        void drive(long nSteps);

        /**
         * 
         * Drive the motor to location given by loc. Basically this does a 
         * drive(mod(loc - getLocation()) in such a way that things work out 
         * even if the motor is already moving. Note that if location has a 
         * modulus, motion is in the direction with the fewest steps.
         * 
         * The driveTo() member function does not block. The motor may not 
         * reach location loc if drive() or driveTo() are invoked before it 
         * gets there.
         * 
         **/
        void driveTo(long loc);

        /**
         * 
         * Stop the current step taking, if any is underway. Stopping an 
         * already-stopped motor does nothing. After stop(), stepsToGo() is 0. 
         * 
         **/
        void stop();

        /**
         * 
         * Set the current location of the motor to the given value. 
         * Subsequent motion is measured relative to this. Positive values 
         * means the current location of the motor is clockwise from 0, 
         * negative values mean counterclockwise. When the motor is first 
         * initialized, its location is 0; If the location has a non-zero 
         * modulus, the location resulting from a setLocation() will range 
         * from 0 to the modulus - 1. If the modulus is zero, location ranges 
         * from -2,147,483,648 to 2,147,483,647 and can over-/underflow.
         * 
         * It's okay to invoke setLocation() when motion is underway, but, if 
         * you need to know where the set location is precisely, it might not 
         * be such a great idea since you won't know what it was at the 
         * instant the loction got set.
         * 
         **/
        void setLocation(long steps);

        /**
         * 
         * Returns the current location. As with setLocation() it's fine to 
         * get the location while motion is underway, but just know that the 
         * information quickly becomes obsolete as motion continues.
         * 
         **/
        long getLocation();

        /**
         * 
         * Set how many steps bring location back to 0. Setting it to 0 means 
         * no modulus -- location goes infinitely off in both directions. The 
         * default is UL_STEPS_PER_REV (4096), one rotation of the output 
         * shaft. If the mechanism that being driven has gearing, you can set 
         * the modulus to match. If the mechanism is a linear drive you can 
         * set modulus to 0.
         * 
         **/
        void setModulus(unsigned long steps);

        /**
         * 
         * Get the value of the location modulus.
         * 
         **/
        unsigned long getModulus();

        /**
         * 
         * Set the speed, in steps per second at which subsequent steps are to 
         * be taken. Doing a setSpeed() while motion is underway is fine. 
         * Default if not set is UL_DEFAULT_SPEED.
         * 
         * Tests indicate that speeds up to 1000 seem to work just fine.
         * 
         * If speed is <= 0 it is set to UL_DEFAULT_SPEED
         * 
         **/
        void setSpeed(unsigned int speed);

        /**
         * 
         * Returns the speed in steps per second at which steps are being 
         * or would be taken. Tests indicate things start to fall apart right 
         * around 1150 steps per second under no load. Values up to around 
         * 1000 seem pretty safe.
         * 
         **/
        unsigned int getSpeed();

        /**
         * 
         * Returns the number of steps still to be taken. Positive values 
         * indicate clockwise movement, negative counterclockwise. Zero means 
         * motor is not moving.
         * 
         **/
        long getStepsToGo();

        /**
         * 
         * Basically returns (getStepsToGo() != 0)
         * 
         **/
        bool isMoving();

    private:
        void moveIt(long amt, bool abs);    // The guts of drive() and driveTo()
        byte objIx;                         // The index of this object into the class globals
};
