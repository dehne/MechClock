# MechClock – An Electromechanical Seven-Segment Clock Display Driven by a RaspberryPi Pico-W

# Introduction

Inspired by a [project](https://www.instructables.com/Kinetic-Digital-Clock-Arduino-3D-Print/) on the Hackaday blog that implements an seven-segment digital clock featuring 3D-printed display digits with protruding segments, each segment driven in or out by a hobby servo, I decided to make a clock with a similar display implemented very differently. Instead of servos, I chose to drive each digit's seven segments using seven cams, all ganged together on single shaft driven by a small, geared-down 28BYJ-48 stepper motor. One turn of the common cam shaft drives the display to show the digits 0 - 9 in sequence. Four identical digit display modules side by side with a fixed colon unit in the middle complete the display. Add a custom PCB with a RaspberryPi Pico-W, 3 ULN2003 Darlington pair DIP chips to drive the steppers, and an LED array/light sensor for self adjusting digit illumination, a case and some custom firmware, and whaddya know: a digital clock.

Here you'll find the firmware for the project.