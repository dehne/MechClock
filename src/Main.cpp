/****
 * @file main.cpp
 * @version 1.0.0
 * @date August, 2024
 * 
 * This is the firmware for a moon phase display. The display consists of a picture of the full 
 * moon, on top of which there is a springy, flexible "terminator" that runs between two pivots 
 * placed at the northernmost and southernmost points of the moon photo. The northern one is free 
 * to turn. One end of the terminator is attached to this pivot. The terminator material passes 
 * across the photo to the southern pivot. It slides through that pivot and continues beyond it. 
 * The southern pivot can be twisted by a stepper motor (pvMotor). This twists the terminator 
 * material, set the angle it makes with the southern extremity of the photo. The northern, 
 * freely rotating, pivot assumes whatever angle the forces on it dictate. 
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
 * Copyright (C) 2024 D.L. Ehnebuske
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software 
 * and associated documentation files (the "Software"), to deal in the Software without 
 * restriction, including without limitation the rights to use, copy, modify, merge, publish, 
 * distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the 
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all copies or 
 * substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING 
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, 
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. 
 * 
 ****/
#include <Arduino.h>                                    // Basic Arduino framework stuff
#include <EEPROM.h>                                     // EEPROM emulation for the Pico
#include <WiFi.h>                                       // Pico WiFi support
#include <CommandLine.h>                                // Terminal command line support
#include <MoonDisplay.h>                                // The moon display mechanism

//#define DEBUG                                           // Uncomment to enable debug printing

#define FINGERPRINT         (0x1656)                    // Our fingerprint (to see if EEProm has our stuff)
#define PAUSE_MILLIS        (500)                       // millis() to pause between various initialization retries
#define BLINK_ON_MILLIS     (50)                        // millis() LED blinks on to show we're actually running
#define BLINK_OFF_MILLIS    (10000)                     // millis() LED blinks off to show we're actually running
#define SERIAL_WAIT_MS      (20000)                     // millis() to wait for Serial to begin before charging ahead
#define WIFI_CONN_MAX_RETRY (3)                         // How many times to retry WiFi.begin() before giving up
#define NTP_MAX_RETRY       (20)                        // How many times to retry getting the system clock set by NTP
#define CONFIG_ADDR         (0)                         // Address of config structure in persistent memory
#define BANNER              "MoonDisplay V1.0.0"
#define LUNAR_MONTH         (29.53059)                  // The (average) length of the lunar cycle in days
#define PHASE_MILLIS        (42524050)                  // The interval in ms between display phase changes (29.53059/60 days)

#define TIMEZONE            "PST8PDT,M3.2.0,M11.1.0"    // Default time zone definition in POSIX format

#define LED                 (LED_BUILTIN)               // Use the LED attached to this pin

// Stepper motor pin definitions
#define PV_IN1              (2)                         // Pivot motor
#define PV_IN2              (3)
#define PV_IN3              (4)
#define PV_IN4              (5)
#define LS_IN1              (6)                         // Leadscrew motor
#define LS_IN2              (7)
#define LS_IN3              (8)
#define LS_IN4              (9)

// Illuminator pin definitions
#define IL_IN1              (11)
#define IL_IN2              (10)

/****
 *  Type definitions
 ****/
struct nvState_t {  // Type definition for configuration data stored in "EEPROM" (actually flash memory in the rp2040)
    int16_t fingerprint;                // Value to tell whether EEPROM contents is ours
    char ssid[33];                      // The WiFi SSID
    char pw[33];                        // The WiFi password
    char timezone[49];                  // The timezone in POSIX format
    int16_t curPhase;                   // The currently displayed phase
    boolean testing;                    // True if in testing mode false if running normally
};

/****
 * Constants
 ****/
// A tm structure defining a time that's earlier than now: Jan 1, 2024
// Make it this way because C time_t values are, technically, opaque
struct tm tmDawnOfHistory {
    .tm_sec = 0,            // tm_sec is 0-based
    .tm_min = 0,            // tm_min is 0-based
    .tm_hour = 0,           // tm_hour is 0-based
    .tm_mday = 1,           // tm_mday is 1-based
    .tm_mon = 1 - 1,        // tm_mon is 0-based
    .tm_year = 2024 - 1900, // tm_year is 1900-based
    .tm_isdst = 0
};
const time_t dawnOfHistory = mktime(&tmDawnOfHistory);

// There was a new moon at 22:57UTC on JUly 5, 2024. That's what we'll use as the first new moon
struct tm tmFirstNewMoon {
    .tm_sec = 0,            // tm_sec is 0-based
    .tm_min = 57,           // tm_min is 0-based
    .tm_hour = 22,          // tm_hour is 0-based
    .tm_mday = 5,           // tm_mday is 1-based
    .tm_mon = 7 - 1,        // tm_mon is 0-based
    .tm_year = 2024 - 1900, // tm_year is 1900-based
    .tm_isdst = 0
};
const time_t firstNewMoon = mktime(&tmFirstNewMoon);

const nvState_t defaultState = {
    .fingerprint = FINGERPRINT,     // How we recognize the content as ours
    .ssid = "Set the SSID",         // Place holder for SSID
    .pw = "Set the PW",             // Place holder for Password
    .timezone = TIMEZONE,           // Default for timezone
    .curPhase = 0,                  // Default for the current phase number
    .testing = true                 // Default for whether we're in testing mode or not
};

const byte p[4] = {PV_IN1, PV_IN2, PV_IN3, PV_IN4};
const byte l[4] = {LS_IN1, LS_IN2, LS_IN3, LS_IN4};
const byte i[2] = {IL_IN1, IL_IN2};

/****
 * Global variables
 ****/
MoonDisplay display(p, l, i);                           // The moon phase display
CommandLine ui;                                         // Command line interpreter object
nvState_t state;                                        // Non-volatile (EEPROM) state
unsigned long nextPhaseChangeMillis;                    // millis() at next phase change
boolean eStop;                                          // True if emergency stop needed, false otherwise
bool haveSavedState;                                    // True if we have a saved state
bool wifiIsUp;                                          // True if we got connected to Wifi
bool clockIsSet;                                        // True if we managed to get the system clock set via WiFi, Internet and NTP

/**
 * @brief   Returns true if a comes "before" b in modulo arithmetic. Basically, if it's shorter to
 *          go "forward" from a to b than it is to go "backward" from a to b.
 * 
 * @param a
 * @param b 
 * @return  true    a comes before b
 * @return  false   a does not come before b
 */
inline bool isBefore(unsigned long a, unsigned long b) {
    return (a - b) > (b - a);
}

/**
 * @brief   Return the "moon age" in seconds at the specified time, t. I.e., the number of seconds 
 *          that have passed since the new moon previous to t
 * 
 * @param t         The time_t for which the moon age is required
 * @return int32_t  The age of the moon in seconds at t
 */
int32_t moonAgeSecsAt(time_t t) {
    uint32_t secSinceFirstNewMoon = static_cast<uint32_t>(difftime(t, firstNewMoon));
    // The number of seconds since the last new moon is mod(secSinceFirstNewMoon, <lunar month length in secs>)
    return static_cast<int32_t>(secSinceFirstNewMoon % static_cast<int32_t>(LUNAR_MONTH * 86400));
}

/**
 * @brief Get the phase (0-59) of the moon at the specifed time.
 * 
 * NB:  The specified time must be on or after firstNewMoon (22:57UTC on JUly 5, 2024).
 * 
 * @param t         time_t time for which the phase is required
 * @return int16_t  The phase (0-59) at the specified time
 */
int16_t moonPhaseAt(time_t t) {
    // The required phase is 60 * <moon age in sec> / <length of the lunar month in sec> ('cause we're using 60 phases/lunation)
    return static_cast<int16_t>(moonAgeSecsAt(t) / static_cast<int32_t>(LUNAR_MONTH * 1440.0));
}

unsigned long getNextPhaseChangeMillis() {
    time_t now = time(nullptr);
    int32_t millisSincePhaseChange = (moonAgeSecsAt(now) * 1000) - (moonPhaseAt(now) * PHASE_MILLIS);
    int32_t nextPhaseChangeMillisFromNow = PHASE_MILLIS - millisSincePhaseChange;
    return millis() + nextPhaseChangeMillisFromNow;
}

/**
 * @brief Connect to the WiFi network using state.ssid for the SSID and state.pw for the password
 * 
 * @return true     Success
 * @return false    Failure
 */
bool connectToWifi() {
    int8_t retryCount = 0;
    while (WiFi.status() != WL_CONNECTED && retryCount < WIFI_CONN_MAX_RETRY) {
        WiFi.begin(state.ssid, state.pw);
        if (WiFi.status() != WL_CONNECTED) {
            if (++retryCount >= WIFI_CONN_MAX_RETRY) {
                return false;   // Give up on connecting to WiFi
            }
        }
    }
    return true;
}

/**
 * @brief   Get the time from an NTP server and set the Pico's system clock from that
 * 
 * @return true     All went well
 * @return false    Couldn't get the time from an NTP server
 */
bool setSysTimeFromNTP() {
    time_t now;
    setenv("TZ", state.timezone, 1);               // Do POSIX ritual to make local time be our time zone
    tzset();
    NTP.begin("pool.ntp.org", "time.nist.gov");     // Set system clock with current epoch using NTP
    int8_t retryCount = 0;
    while ((now = time(nullptr)) < dawnOfHistory && ++retryCount < NTP_MAX_RETRY) {
        delay(PAUSE_MILLIS);
    }
    if (retryCount >= NTP_MAX_RETRY) {
        return false;
    }
    return true;
}

/**
 * @brief Get the status of the device as a String
 * 
 * @return String   The device status
 */
String getStatus() {
    String answer = 
        String("WiFi is ") + (wifiIsUp ? "" : "not" ) + "connected, system clock is " + (clockIsSet ? "" : "not ") + 
        "set, test is " + (state.testing ? "on.\n" : "off.\n");
    if (clockIsSet) {
        time_t now = time(nullptr);
        tm *nowTm = gmtime(&now);
        int32_t secToPC = (nextPhaseChangeMillis - millis()) / 1000;
        String hourPC = String(secToPC / 3600);
        String minPC = String((secToPC % 3600) / 60);
        String secPC = String(secToPC % 60);
        answer += 
            "At " + String(nowTm->tm_hour) + ":" + (nowTm->tm_min < 10 ? "0" : "") + 
            String(nowTm->tm_min) + ":" + (nowTm->tm_sec < 10 ? "0" : "") + String(nowTm->tm_sec) + " UTC displayed moon phase is " + 
            String(display.getPhase()) + ", actual moon phase is " + String(moonPhaseAt(now)) + ", next phase change is in " + 
            hourPC + ":" + (minPC.length() < 2 ? "0" : "") + minPC + ":" + (secPC.length() < 2 ? "0" : "") + secPC + ".\n";
    } else {
        answer += "Displayed moon phase is " + String(display.getPhase()) + ".\n";
    }
    return answer;
}

/**
 * @brief help command handler: Display command summary
 * 
 * @param h         Pointer to CommandHandlerHelper object
 * @return String   The String to dsiplay
 */
String onHelp(CommandHandlerHelper *h) {
    return
        "help                   Display this text to the user\n"
        "h                      Same as \"help\"\n"
        "assume <phase>         Assume display is showing phase <phase>\n"
        "ls [<steps>]           Drive leadscrew by <steps>. + ==> out, - ==> in\n"
        "pv [<steps>]           Drive pivot by <steps>. + ==> CC, - ==> CW viewed from front\n"
        "save                   Save the current configuration data in persistent memory.\n"
        "                       Until a save is done or the phase of the moon changes,\n"
        "                       configuration changes are not made persistent.\n"
        "show <phase>           Change display to show phase <phase>\n"
        "status                 Report on the system's status.\n"
        "stop                   Stop all motion immediately\n"
        "s                      Same as \"stop\"\n"
        "test [on|off]          Set or print whether we're in test mode\n"
        "tz [<POSIX tz>]        Set or display the POSIX-format timeszone to use.\n"
        "                       Save to make persistent.\n"
        "wifi pw <password>     Set the WiFi password to <password>.\n"
        "                       Save to make persistent.\n"
        "wifi ssid <ssid>       Set the WiFi SSID we should use to <ssid>\n"
        "                       Save to make persistent.\n";
}

/**
 * @brief   assume <phase> command handler: Assume display is showing phase <phase> and that's 
 *          where we are and want to be.
 * 
 * @param h         CommandLineHelper to talk to command processor
 * @return String   What command processor should show to the user
 */
String onAssume(CommandHandlerHelper *h) {
    int16_t phase = h->getWord(1).toInt();
    if (phase < 0 || phase >= 60) {
        return "Phase must be 0 .. 59\n";
    }
    state.curPhase = phase;
    display.assume(phase);
    EEPROM.put(CONFIG_ADDR, state);
    return String("Assumed display shows phase ") + String(phase) + String(EEPROM.commit() ? " and saved\n" : " but unable to save.\n");
}

/**
 * @brief   ls command handler: Drive leadscrew by <steps>. + ==> out, - ==> in,
 *          but don't change thes location the motor thinks it's at
 * 
 * @param h         CommandLineHelper to talk to command processor
 * @return String   What command processor should show to the user
 */
String onLs(CommandHandlerHelper *h) {
    int32_t steps = h->getWord(1).toInt();
    if (steps == 0) {
        return "Leadscrew position: " + String(display.getLs()) + " steps.\n";
    }
    display.turnLs(steps);
    return String("Driving leadscrew by ") + String(steps) + ".\n";
}

/**
 * @brief   pv command handler: Drive pivot by <steps>. + ==> CC, - ==> CW viewed from front, 
 *          but don't change the location the motor thinks it's at.
 * 
 * @param h         CommandLineHelper to talk to command processor
 * @return String   What command processor should show to the user
 */
String onPv(CommandHandlerHelper *h) {
    int32_t steps = h->getWord(1).toInt();
    if (steps == 0) {
        return "Pivot position: " + String(display.getPv()) + " steps.\n";
    }
    display.turnPv(steps);
    return String("Driving pivot by ") + String(steps) + ".\n";
}

/**
 * @brief   'save' command handler: Save the configuration data to persistent memory
 * 
 * @param h         The command handler helper we use to access what the user typed
 * @return String   The result to be displayed to the user
 */
String onSave(CommandHandlerHelper* h) {
    EEPROM.put(CONFIG_ADDR, state);
    return (EEPROM.commit() ? "Configuration saved\n" : "Configuration save failed.\n");
}

/**
 * @brief show <phase> command handler: Change display to show phase <phase>
 * 
 * @param h         CommandLineHelper to talk to command processor
 * @return String   What command processor should show to the user
 */
String onShow(CommandHandlerHelper *h) {
    int16_t phase = h->getWord(1).toInt();
    if (phase < 0 || phase >= 60) {
        return "Phase to show must be 0 .. 59.\n";
    }
    display.showPhase(phase);
    return String("Changing to show phase ") + String(phase) + ".\n";
}

/**
 * @brief status command handler: Report on the system's status
 * 
 * @param h         CommandLineHelper to talk to command processor
 * @return String   What command processor should show to the user
 */
String onStatus(CommandHandlerHelper *h) {
    return getStatus();
}

/**
 * @brief stop and s command handler: Put us in an emergency stop state
 * 
 * @param h         CommandLineHelper to talk to command processor
 * @return String   What command processor should show to the user
 */
String onStop(CommandHandlerHelper *h) {
    display.stop();
    return "Stopping.\n";
}

/**
 * @brief test on|off command handler: Turn test mode on or off
 * 
 * @param h         CommandLineHelper to talk to command processor
 * @return String   What command processor should show to the user
 */
String onTest(CommandHandlerHelper *h) {
    if(h->getWord(1).equalsIgnoreCase("on")) {
        state.testing = true;
        return "Test mode on\n";
        digitalWrite(LED, LOW); // The watchdog doesn't blink in test mode, so, in case it's on...
    } else if (h->getWord(1).equalsIgnoreCase("off")) {
        state.testing = false;
        nextPhaseChangeMillis = millis();
        return "Test mode off\n";
    } else {
        return String("Test mode is currently ") + (state.testing ? "on\n" : "off\n");
    }
}

/**
 * @brief   'tz [<POSIX tz>]' command handler: Set or display the POSIX-format timeszone to use
 * 
 * @param h         The command handler helper we use to access what the user typed
 * @return String   The result to be displayed to the user
 */
String onTz(CommandHandlerHelper* h) {
    String target = h->getWord(1);
    if (target.length() == 0) {
        return String("Timezone is: '") + String(state.timezone) + "'.\n";
    }
    if (target.length() > sizeof(state.timezone) - 1) {
        return String("timezone string must be less than ") + String(sizeof(state.timezone)) + String(" characters.\n");
    }
    strcpy(state.timezone, target.c_str());
    return String("Timezone set to '") + target + "'.\n";
}

/**
 * @brief   'wifi pw <password>' and 'wifi ssid <ssid>' commend handler: Set the WiFi password 
 *          or SSID to use to connect to the internet.
 * 
 * @param h         The command handler helper we use to access what the user typed
 * @return String   The result to be displayed to the user
 */
String onWifi(CommandHandlerHelper* h) {
    String subCmd = h->getWord(1);
    subCmd.toLowerCase();
    if (subCmd.length() == 0) {
        return String("Wifi ssid: '") + String(state.ssid) + String("'\nWifi pw:   '") + String(state.pw) + "'\n";
    }
    if (!(subCmd.equals("pw") || subCmd.equals("ssid"))) {
        return String("wifi command only knows about 'pw' and 'ssid'.\n");
    }
    String target = h->getCommandLine().substring(subCmd.length());
    target = target.substring(target.indexOf(subCmd) + subCmd.length());
    target.trim();
    if (target.length() <= 0) {
        return String("Can't set the WiFi ") + subCmd +  String(" to nothing at all.\n");
    }
    if (subCmd.equals("pw")) {
        if (sizeof(state.pw) < target.length() + 1) {
            return String("Maximum length of a password is ") + String(sizeof(state.pw) - 1) + String(" characters\n");
        }
        strcpy(state.pw, target.c_str());
    } else {
        if (sizeof(state.ssid) < target.length() + 1) {
            return String("Maximum length of an ssid is ") + String(sizeof(state.pw) - 1) + String(" characters\n");
        }
        strcpy(state.ssid, target.c_str());
    }
    return String("Changed ") + subCmd + " to '" + target + "'.\n";
}


void setup() {
    haveSavedState = true;          // Assume we'll succeed in retrieveing the state
    wifiIsUp = false;               // Assume we'll fail in getting the WiFi up
    clockIsSet = false;             // Assume we'll succeed in setting the system clock from NTP

    // Init builtin LED
    pinMode(LED, OUTPUT);
    digitalWrite(LED, HIGH);

    // Get Serial up and running
    Serial.begin(9600);
    long msStart = millis();
    while (!Serial && millis() - msStart < SERIAL_WAIT_MS) {
        delay(PAUSE_MILLIS);
        digitalWrite(LED, digitalRead(LED) == HIGH ? LOW : HIGH);
    }
    Serial.println(BANNER);
    digitalWrite(LED, LOW);

    // Try to retrieve the configuration
    EEPROM.begin(4096);
    state = EEPROM.get(CONFIG_ADDR, state);
    if (state.fingerprint != FINGERPRINT) {
        state = defaultState;
        Serial.println("There's no stored configuration data; we won't be able to connect to WiFi.");
        haveSavedState = false;
    }

    // Initialize the command interpreter
    if (!(
        ui.attachCmdHandler("help", onHelp) && ui.attachCmdHandler("h", onHelp) &&
        ui.attachCmdHandler("assume", onAssume) &&
        ui.attachCmdHandler("ls", onLs) && 
        ui.attachCmdHandler("pv", onPv) &&
        ui.attachCmdHandler("save", onSave) &&
        ui.attachCmdHandler("show", onShow) &&
        ui.attachCmdHandler("status", onStatus) &&
        ui.attachCmdHandler("stop", onStop) && ui.attachCmdHandler("s", onStop) &&
        ui.attachCmdHandler("test", onTest) &&
        ui.attachCmdHandler("tz", onTz) &&
        ui.attachCmdHandler("wifi", onWifi)
    )) {
        Serial.print("Too many command handlers.\n");
    }

    // Connect to WiFi if we have a saved config
    if (haveSavedState) {
        Serial.printf(String("Attempting to connect to WiFi with ssid '%s'.\n").c_str(), state.ssid);
        wifiIsUp = connectToWifi();
    }

    // Initialize time of day if WiFi is up
    if (wifiIsUp) {
        Serial.println("Successfully connected to WiFi. Getting time from NTP server.");
        clockIsSet = setSysTimeFromNTP();
    } else {
        Serial.println("Unable to connect to WiFi.");
    }

    if (clockIsSet) {
        Serial.println("System clock set successfully.");
    } else {
        Serial.println("Couldn't initialize the system clock from the internet, hopefully for obvious reasons.");
    }

    // Initialize the display
    Serial.println("Initializing the display.");
    display.begin(state.curPhase);
    if (clockIsSet) {
        time_t now = time(nullptr);
        if (state.testing || state.curPhase == moonPhaseAt(now)) {
            nextPhaseChangeMillis = getNextPhaseChangeMillis();
        } else {
            nextPhaseChangeMillis = millis();
        }
    }

    // Show we're ready to go
    Serial.print(getStatus());
    Serial.print("Type 'h' or 'help' for a command summary.\n");
}

void loop() {
    static unsigned long nextBlinkMillis = millis() + PAUSE_MILLIS;
    // If we're actually running, deal with blinking the watchdog LED
    if (clockIsSet && isBefore(nextBlinkMillis, millis())) {
        if (!state.testing) {
            if(digitalRead(LED) == HIGH) {
                digitalWrite(LED, LOW);
                nextBlinkMillis += BLINK_OFF_MILLIS;
            } else {
                digitalWrite(LED, HIGH);
                nextBlinkMillis += BLINK_ON_MILLIS;
            }
        } else {
            nextBlinkMillis += BLINK_OFF_MILLIS;
        }
    }
     // Let the ui and the display do their thing
    ui.run();
    int16_t newPhase = display.run();
    if (newPhase != -1) {
        state.curPhase = newPhase;
        EEPROM.put(CONFIG_ADDR, state);
        if(!EEPROM.commit()) {
            Serial.println("Moved to new phase, but unable to save!");
        }
    }

    // If the time for a new phase has arrived, deal with it
    if (clockIsSet && isBefore(nextPhaseChangeMillis, millis())) {
        // if we're not testing, actually move the display
        if (!state.testing ) {
            time_t now = time(nullptr);
            int16_t phase = moonPhaseAt(now);
            // If something's already underway, we went off the rails somehow. Stop the world, it's time to get off!
            if (!display.showPhase(phase)) {
                Serial.println("Time for phase change, but things aren't all quiet. Stopping.");
                display.stop();
            }
        }
        nextPhaseChangeMillis = getNextPhaseChangeMillis();
    }
}
