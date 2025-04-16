/****
 * @file main.cpp
 * @version 1.1.1
 * @date April 2025
 * 
 * This is the firmware for a moon phase display. It uses the Arduino framework and runs on a 
 * Raspberry Pi Pico W. See MoonDisplay.h for details on ahow the display works display. 
 * 
 * The basic function here is overall control of the device. In particular
 * 
 *      * Managing persistent state:
 *          - Info needed to connect to WiFi for NTP access
 *          - Timezone info. (So we can talk in local time.)
 *          - The current phase that the MoonDisplay is showing. (So that we can pick up again 
 *            where we left off after a reboot. Updated each time we move the display.)
 *          - Whether we're in "test" mode. (In test mode we don't move the display.)
 *          - The ambient light level limits. (Below the min, lights are off, above the max, 
 *            lights are fuly on.)
 *          - Optionally, depending on a #define, watchdog data for the ULN2003Pico stepper 
 *            library. (I was having trouble with delays in receiving iterrupts for a while.)
 *      * Setting and maintaining the current time using WiFi, NTP and the system time() 
 *        functions. 
 *      * Blinking an LED to show we're alive.
 *      * Initializing the MoonDisplay from persistent storage and then letting it run().
 *      * Setting up and running command-line interpreter to let someone adjust and inspect the 
 *        device.
 * 
 *****
 * 
 * Copyright (C) 2024-2025 D.L. Ehnebuske
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
#include <ULN2003Pico.h>                                // ULN2003Pico stepper motor drivers (only used for watchdog)

//#define DEBUG                                           // Uncomment to enable debug printing

#define FINGERPRINT         (0x1656)                    // Our fingerprint (to see if EEProm has our stuff)
#define PAUSE_MILLIS        (500)                       // millis() to pause between various initialization retries
#define BLINK_ON_MILLIS     (50)                        // millis() LED blinks on to show we're actually running
#define BLINK_OFF_MILLIS    (10000)                     // millis() LED blinks off to show we're actually running
#define SERIAL_WAIT_MS      (20000)                     // millis() to wait for Serial to begin before charging ahead
#define WIFI_CONN_MAX_RETRY (3)                         // How many times to retry WiFi.begin() before giving up
#define NTP_MAX_RETRY       (20)                        // How many times to retry getting the system clock set by NTP
#define CONFIG_ADDR         (0)                         // Address of config structure in persistent memory
#define WD_DATA_ADDR        sizeof(nvState_t)           // Address of watchdog data structure in persistent memory
#define BANNER              "MoonDisplay V1.1.0"        // Hello World message
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
#define IL_IN1              (11)                        // Waxing COB pin
#define IL_IN2              (10)                        // Waning COB pin
#define IL_IN3              (26)                        // Phototransistor pin

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
    uint8_t loAmbient;                  // Ambient light lower limit
    uint8_t hiAmbient;                  // Ambient light upper limit
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
    .testing = true,                // Default for whether we're in testing mode or not
    .loAmbient = 4,                 // Default lower ambient light limit
    .hiAmbient = 75                 // Default upper ambient light limit
};

// GPIO pins for pivot motor, leadscrew motor, and the Illuminator's two LED COBs and its phototransistor
const byte p[4] = {PV_IN1, PV_IN2, PV_IN3, PV_IN4};
const byte l[4] = {LS_IN1, LS_IN2, LS_IN3, LS_IN4};
const byte i[3] = {IL_IN1, IL_IN2, IL_IN3};

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
 * @brief   Returns true if a comes "before" b in unsigned long modulo arithmetic. Basically, if 
 *          it's shorter to go "forward" from a to b than it is to go "backward" from a to b.
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
 * NB:  The specified time must be on or after firstNewMoon (22:57UTC on July 5, 2024).
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

#ifdef UL_WATCHDOG
String formatWdData(wdData_t wdData) {
    if (!wdData.watchdogBarked) {
        return "Watchdog did not bark.\n";
    }
    return "Watchdog barked: " +
        (wdData.watchdogAlarmType == STEP_TIME ? String("Unreasonable step time") : wdData.watchdogAlarmType == CALLBACK ? String("Late to callback") : String("Next step already missed")) + 
        ". Motor was " + String(wdData.watchdogMotor) + ", microsNextStep was " + String(wdData.watchdogMns) + 
        ", steps to go was " + String(wdData.watchdogStg) + ", curMicros was " + String(wdData.watchdogCurMicros) + 
        ", lastCurMicros was " + String(wdData.watchdogLastCurMicros) + ".\n";
}
#endif

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
            String(display.getPhase()) + "/60, actual moon phase is " + String(moonPhaseAt(now)) + "/60, next phase change is in " + 
            hourPC + ":" + (minPC.length() < 2 ? "0" : "") + minPC + ":" + (secPC.length() < 2 ? "0" : "") + secPC + ".\n";
    } else {
        answer += "Displayed moon phase is " + String(display.getPhase()) + ".\n";
    }
    #ifdef UL_WATCHDOG
    wdData_t wdData = EEPROM.get(WD_DATA_ADDR, wdData);
    answer += formatWdData(wdData);
    #endif
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
        "ambient [<lo> <hi>]    Display or set lower and upper ambient light limits\n"
        "                         0 <= lo < hi <= 100\n"
        "assume <phase>         Assume moon display is showing phase <phase>\n"
        "ls [<steps>]           Drive leadscrew by <steps>. + ==> out, - ==> in\n"
        "pv [<steps>]           Drive pivot by <steps>. + ==> CC, - ==> CW viewed from front\n"
        "save                   Save the current configuration data in persistent memory.\n"
        "                       Until a save is done or the phase of the moon changes,\n"
        "                       configuration changes are not made persistent.\n"
        "show <phase>           Make moon display show phase <phase>\n"
        "status                 Display the system's status.\n"
        "stop                   Stop all moon display motion immediately\n"
        "s                      Same as \"stop\"\n"
        "test [on|off]          Set or display whether we're in test mode\n"
        "tz [<POSIX tz>]        Set or display the POSIX-format timeszone to use.\n"
        "                       Save configuration to make persistent.\n"
        #ifdef UL_WATCHDOG
        "watchdog               Display and the reset the persistent watchdog data.\n"
        #endif
        "wifi                   Display the current ssid and pw.\n"
        "wifi pw <password>     Set the WiFi password to <password>.\n"
        "                       Save to make persistent.\n"
        "wifi ssid <ssid>       Set the WiFi SSID we should use to <ssid>\n"
        "                       Save to make persistent.\n";
}

String onAmbient(CommandHandlerHelper *h) {
    String arg1 = h->getWord(1);
    if (arg1.length() == 0) {
        return "Current ambient light level is " + String(display.getAmbient()) + ", lowerLimit; " + String(state.loAmbient) + 
            ", upper limit: " + String(state.hiAmbient) + "\n";
    }
    int16_t lowerLimit = arg1.toInt();
    int16_t upperLimit = h->getWord(2).toInt();
    if (lowerLimit < 0 || upperLimit > 100 || upperLimit <= lowerLimit) {
        return "Ambient limits must be: 0 >= lower < upper <= 100\n";
    }
    state.loAmbient = lowerLimit;
    state.hiAmbient = upperLimit;
    display.setAmbientLimits(lowerLimit, upperLimit);
    return "Ambient limits set to " + String(lowerLimit) + " " + String(upperLimit) + "\n";
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

#ifdef UL_WATCHDOG
/**
 * @brief   "watchdog" command handler: Display and the reset the persistent watchdog data.
 * 
 * @param h         The command handler helper we use to access what the user typed
 * @return String   The result to be displayed to the user
 */
String onWatchdog(CommandHandlerHelper* h) {
    wdData_t wdData = EEPROM.get(WD_DATA_ADDR, wdData);
    wdData_t empty = {
        .watchdogBarked = false,
        .watchdogCurMicros = 0,
        .watchdogAlarmType = CALLBACK,
        .watchdogMotor = 0,
        .watchdogMns = 0,
        .watchdogStg = 0,
        .watchdogLastCurMicros = 0
    };
    EEPROM.put(WD_DATA_ADDR, empty);
    EEPROM.commit();
    return formatWdData(wdData);
}
#endif

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
        ui.attachCmdHandler("ambient", onAmbient) &&
        ui.attachCmdHandler("assume", onAssume) &&
        ui.attachCmdHandler("ls", onLs) && 
        ui.attachCmdHandler("pv", onPv) &&
        ui.attachCmdHandler("save", onSave) &&
        ui.attachCmdHandler("show", onShow) &&
        ui.attachCmdHandler("status", onStatus) &&
        ui.attachCmdHandler("stop", onStop) && ui.attachCmdHandler("s", onStop) &&
        ui.attachCmdHandler("test", onTest) &&
        ui.attachCmdHandler("tz", onTz) &&
        #ifdef UL_WATCHDOG
        ui.attachCmdHandler("watchdog", onWatchdog) &&
        #endif
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
    display.setAmbientLimits(state.loAmbient, state.hiAmbient);

    // Show we're ready to go
    Serial.print(getStatus());
    Serial.print("Type 'h' or 'help' for a command summary.\n");
}

void loop() {
    static unsigned long nextBlinkMillis = millis() + PAUSE_MILLIS;
    // If we're actually running, deal with blinking the "I'm running" LED
    if (clockIsSet && isBefore(nextBlinkMillis, millis())) {
        if (!state.testing) {
            if(digitalRead(LED) == HIGH) {
                digitalWrite(LED, LOW);
                nextBlinkMillis += BLINK_OFF_MILLIS;
            } else {
                digitalWrite(LED, HIGH);
                nextBlinkMillis += BLINK_ON_MILLIS;
                #ifdef UL_WATCHDOG
                wdData_t wdData = ULN2003WatchdogDump();
                if (wdData.watchdogBarked) {
                    EEPROM.put(WD_DATA_ADDR, wdData);
                    if(!EEPROM.commit()) {
                        Serial.println("Unable to save watchdog data.");
                    }
                }
                #endif
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
