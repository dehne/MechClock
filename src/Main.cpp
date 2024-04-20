/****
 * @file main.cpp
 * @version 1.1.0
 * @date April 20, 2024
 * 
 * This is the firmware for a mechanical seven-segment digital clock. The clock consists of four 
 * seven-segment digits forming a conventional digital clock display. Each seven-segment digit is 
 * driven by a 28BYJ-48 geared-down stepper motor. Rotating the stepper's shaft moves seven ganged 
 * cams, one cam for each segment. The cams cause the segments to protrude from the face of the 
 * display, or retract into it, as necessary to form the digit to be displayed.
 * 
 * To make the display easier to read, the clock is lit from an angle above with an array of SMD 
 * LEDs so that the segments cast shadows when they protrude. The brightness of the LEDs is 
 * adjusted based on the ambient illumination, bright when it's daytime, dimmer when the room 
 * lights are on and off completely when the room light are switched off.
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
#include <SSDisplay.h>                                  // The 7-segment display device
#include <Illuminate.h>                                 // The illumination control mechanism

#define SIGNATURE           (0x437d)                    // "Signature" to id EEPROM contents as ours
#define PAUSE_MS            (500)                       // millis() to pause between various initialization retries
#define SERIAL_WAIT_MS      (20000)                     // millis() to wait for Serial to begin before charging ahead
#define COUNT_PAUSE_MS      (2000)                      // millis() to pause between digits during 'count' command
#define WIFI_CONN_MAX_RETRY (3)                         // How many times to retry WiFi.begin() before giving up
#define NTP_MAX_RETRY       (20)                        // How many times to retry getting the system clock set by NTP
#define CONFIG_ADDR         (0)                         // Address of config structure in persistent memory
#define BANNER              "MechClock v1.1.0"

#define TIMEZONE            "PST8PDT,M3.2.0,M11.1.0"    // Default time zone definition in POSIX format

#define LED                 (LED_BUILTIN)               // Blink the LED attached to this GPIO

// GPIO pins to which the steppers are attached
#define MINUTE_IN1          (21)
#define MINUTE_IN2          (20)
#define MINUTE_IN3          (19)
#define MINUTE_IN4          (18)
#define MINUTE10_IN1        (2)
#define MINUTE10_IN2        (3)
#define MINUTE10_IN3        (4)
#define MINUTE10_IN4        (5)
#define HOUR_IN1            (6)
#define HOUR_IN2            (7)
#define HOUR_IN3            (8)
#define HOUR_IN4            (9)
#define HOUR10_IN1          (10)
#define HOUR10_IN2          (11)
#define HOUR10_IN3          (12)
#define HOUR10_IN4          (13)

// GPIO pins to which the limit Hall-effect sensors are attached
#define MINUTE_HOME         (14)
#define MINUTE10_HOME       (15)
#define HOUR_HOME           (16)
#define HOUR10_HOME         (17)

// Illumination control stuff
#define LIGHT_SENSE         (26)    // Phototransistor pin: Analog voltage proportional to light intensity
#define LIGHT_CONTROL       (22)    // PWM pin for illumination control

/****
 *  Type definitions
 ****/
struct config_t {   // Type definition for configuration data stored in "EEPROM" (actually flash memory in the rp2040)
    uint16_t signature;             // How we guess whether this is our config data
    char ssid[33];                  // The WiFi SSID
    char pw[33];                    // The WiFi password
    char timezone[49];              // The timezone in POSIX format
    int16_t jog[SSD_N_MODULES];     // The jog values (offset from Hall-effect home) for each of the digits of the display
    bool style24;                   // True if the display style is 24-hour (e.g., 13:00), false if 12-hour (e.g., 1:00)
    float dark;                     // Ambient illumination boundary between dark and normal indoor lighting
    float bright;                   // Ambient illumination boundary between normal indoor lighting and daylight
};

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

/****
 * Constants
 ****/
const time_t dawnOfHistory = mktime(&tmDawnOfHistory);

/****
 * Global variables
 ****/
const config_t defaultConfig {      // Default configuration data
    .signature = SIGNATURE,         // How we guess whether this is our config data
    .ssid = "Set the SSID",         // Place holder for SSID
    .pw = "Set the PW",             // Place holder for Password
    .timezone = TIMEZONE,           // Default for timezone
    .jog = {0, 0, 0, 0},            // Default home position Hall-effect sensor offsets
    .dark = ILL_DEFAULT_DARK,       // Default dark boundary value
    .bright = ILL_DEFAULT_BRIGHT    // Default bight boundary 
};

config_t config;                // Where we hold our configuration data

byte displayPins[4][5] =  {     // The GPIO pins the display is connected to
    {MINUTE_IN1, MINUTE_IN2, MINUTE_IN3, MINUTE_IN4, MINUTE_HOME}, 
    {MINUTE10_IN1, MINUTE10_IN2, MINUTE10_IN3, MINUTE10_IN4, MINUTE10_HOME},
    {HOUR_IN1, HOUR_IN2, HOUR_IN3, HOUR_IN4, HOUR_HOME},
    {HOUR10_IN1, HOUR10_IN2, HOUR10_IN3, HOUR10_IN4, HOUR10_HOME}
};
SSDisplay display {displayPins};// The steppers controlling the digit displays

CommandLine ui;                 // The commandline interpreter

Illuminate illuminate {LIGHT_SENSE, LIGHT_CONTROL};

bool haveSavedConfig;           // True if we have a saved config
bool wifiIsUp;                  // True if we got connected to Wifi
bool clockIsSet;                // True if we managed to get the system clock set via WiFi, Internet and NTP
bool testMode;                  // Set test command; stops clock from changing if it would otherwise do so.

/**
 * @brief Dump the contents of config structure to Serial (for debugging)
 * 
 */
void dumpConfig() {
    if (config.signature != SIGNATURE) {
        Serial.println("Invalid config:");
        char* b = (char *)&config;
        for (int i = 0; i < sizeof(config); i++) {
            Serial.printf("%02x%c", *(b + i), i % 32 == 31 ? '\n' : ' ');
        }
        Serial.print("\n");
        return;
    }
    Serial.println("Valid config:");
    Serial.printf(".signature: 0x%02x\n"
                  ".ssid:      %s\n"
                  ".pw:        %s\n"
                  ".timezone:  %s\n",
                  config.signature, config.ssid, config.pw, config.timezone);
}

bool connectToWifi() {
    int8_t retryCount = 0;
    while (WiFi.status() != WL_CONNECTED && retryCount < WIFI_CONN_MAX_RETRY) {
        WiFi.begin(config.ssid, config.pw);
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
    setenv("TZ", config.timezone, 1);               // Do POSIX ritual to make local time be our time zone
    tzset();
    NTP.begin("pool.ntp.org", "time.nist.gov");     // Set system clock with current epoch using NTP
    int8_t retryCount = 0;
    while ((now = time(nullptr)) < dawnOfHistory && ++retryCount < NTP_MAX_RETRY) {
        delay(PAUSE_MS);
    }
    if (retryCount >= NTP_MAX_RETRY) {
        return false;
    }
    return true;
}

/**
 * @brief   'help' and 'h' command handler: Display help text.
 * 
 * @param h         The command handler helper we use to access what the user typed
 * @return String   The result to be displayed to the user
 */
String onHelp(CommandHandlerHelper* h) {
    return 
        BANNER "help\n"
        "help               Display this command summary.\n"
        "h                  Same as 'help'.\n"
        "ambient [dark day] Set or display the ambient light boundaries between dark <-> lamp and lamp <-> day\n"
        "                     Where 0 <= dark <= day <= 100. Save to make persistent.\n"
        "assume <m> <n>     Assume that module <m> is showing digit <n>.\n"
        "count [<m>]        On module <m>, display the digits 0..9 slowly one after another.\n"
        "                     Modules are numbered 0 .. 3 from left to right.\n"
        "home <m>           Move module <m> to its home position, displaying 0.\n"
        "jog <m> [<steps>]  Jog module <m> by <steps>, 0 <= m <= 3, -2047 <= <steps> <= 2047.\n"
        "                     If <steps> is 0 or omitted, display current jog for module <m>.\n"
        "                     Save to make persistent.\n"
        "save               Save the current configuration data in persistent memory.\n"
        "                     Until a save is done, configuration changes are not made persistent.\n"
        "show <m> <n>       Make module <m> show digit <n>, 0 <= m <= 3, 0 <= n <= 9.\n"
        "status             Display the system status.\n"
        "style [12 | 24]    Set or display the style of the display, 12-hour or 24-hour\n"
        "                     Save to make persistent.\n"
        "test [on | off]    Turn test mode on or off or say whether test is on\n"
        "tz [<POSIX tz>]    Set or display the POSIX-format timeszone to use.\n"
        "                     Save to make persistent.\n"
        "wifi pw <password> Set the WiFi password to <password>.\n"
        "                     Save to make persistent.\n"
        "wifi ssid <ssid>   Set the WiFi SSID we should use to <ssid>\n"
        "                     Save to make persistent.\n";
}

/**
 * @brief   'ambient [dark day]" command handler: Set or display the ambient light boundaries 
 *          between dark<->>lamp and lamp<->day. Where 0.0 <= dark <= day <= 1.0
 * 
 * @param h         The command handler helper we use to access what the user typed
 * @return String   The result to be displayed to the user
 */
String onAmbient(CommandHandlerHelper* h) {
    float darkToLamps = h->getWord(1).toFloat();
    float lampsToDay = h->getWord(2).toFloat();
    if (darkToLamps == 0.0 && lampsToDay == 0.0) {
        return String("Dark-to-lamps: ") + String(100.0 * illuminate.getDarkToLamps()) + 
          "%, lamps-to-day: " + String(100.0 * illuminate.getLampsToDay()) + "%.\n";
    }
    if (illuminate.setAmbientBounds(darkToLamps / 100.0, lampsToDay / 100.0)) {
        config.dark = darkToLamps / 100.0;
        config.bright = lampsToDay / 100.0;
        return String("Dark-to-lamps boundary set to ") + String(darkToLamps) + 
          "%, lamps-to-day boundary set to " + String(lampsToDay) + "%.\n";
    }
    return "Can't set those boundaries. Must be 0.0% <= dark-to-lamps <= lamps-to-day <= 100.0%.\n";
}

/**
 * @brief   'assume <m> <n>' command handler: Assume that the display module m is showing the 
 *          digit <n>
 * 
 * @param h         The command handler helper we use to access what the user typed
 * @return String   The result to be displayed to the user
 */
String onAssume(CommandHandlerHelper* h) {
    int16_t m = h->getWord(1).toInt();
    int16_t n = h->getWord(2).toInt();
    if (m < 0 || m >= SSD_N_MODULES || n < 0 || n > 9) {
        return "Assume needs a module number in the range 0 .. 3 and a digit in the range 0 .. 9.\n";
    }
    display.assume(m, n);
    return String("Assuming digit ") + String(m) + " shows " + String(n) + ".\n";
}

/**
 * @brief   'count [<m>] command handler: On module <m>, display the digits 0..9 slowly one after 
 *          another. Modules are numbered 3 .. 0 from left to right. I.e., module 0 is minutes.
 * 
 * @param h         The command handler helper we use to access what the user typed
 * @return String   The result to be displayed to the user
 */
String onCount(CommandHandlerHelper* h) {
    int16_t m = h->getWord(1).toInt();
    if (m < 0 || m >= SSD_N_MODULES) {
        return "Module number must be in the range 0..3\n";
    }
    int16_t curVal = display.getVal(m);
    for (int16_t d = 1; d <= 10; d++) {
        display.setVal(m, (curVal + d) % 10);
        delay(COUNT_PAUSE_MS);
    }
    return "";
}

/**
 * @brief   'home <m> command handler: Move module <m> to its sensor based home position
 *          (which for convenience of construction turns out to be "5").
 * 
 * @param h         The command handler helper we use to access what the user typed
 * @return String   The result to be displayed to the user
 */
String onHome(CommandHandlerHelper* h) {
    int16_t m = h->getWord(1).toInt();
    if (m < 0 || m >= SSD_N_MODULES) {
        return "Command 'home' needs a module number, 0 .. 3\n";
    }
    display.home(m);
    display.jog(m, config.jog[m]);
    return "";
}

/**
 * @brief   'jog <m> [<steps>]' command handler: Jog module <m> by <steps>, 0 <= m <= 3, 
 *          -2047 <= <steps> <= 2047. Used to fine tune display of digits. If <steps> is 0
 *          or omitted, display currrent jog value for module <m>.
 * 
 * @details The net of all the jog commands issued is saved in config and is used during 
 *          initialization to fine tune each digit's display.
 * 
 * @param h         The command handler helper we use to access what the user typed
 * @return String   The result to be displayed to the user
 */
String onJog(CommandHandlerHelper* h) {
    int16_t m = h->getWord(1).toInt();
    if (m < 0 || m > 3) {
        return "Module number out of range. Must be 0..3\n";
    }
    int16_t n = h->getWord(2).toInt();
    if (n < -2047 || n > 2047) {
        return "Step value out of range. Must be -2047..2047\n";
    }
    if (n == 0) {
        return String("Jog for module ") + String(m) + " is " + String(config.jog[m]) + ".\n";
    }
    config.jog[m] += n;
    display.jog(m, n);
    return "";
}

/**
 * @brief   'save' command handler: Save the configuration data to persistent memory
 * 
 * @param h         The command handler helper we use to access what the user typed
 * @return String   The result to be displayed to the user
 */
String onSave(CommandHandlerHelper* h) {
    EEPROM.put(CONFIG_ADDR, config);
    return (EEPROM.commit() ? "Configuration saved\n" : "Configuration save failed.\n");
}

/**
 * @brief   'show <m> <n>' command handler: Make module <m> show digit <n>, 0 <= m <= 3, 0 <= n <= 9
 * 
 * @param h         The command handler helper we use to access what the user typed
 * @return String   The result to be displayed to the user
 */
String onShow(CommandHandlerHelper* h) {
    int16_t m = h->getWord(1).toInt();
    if (m < 0 || m > 3) {
        return "Module number out of range. Must be 0..3\n";
    }
    int16_t n = h->getWord(2).toInt();
    if (n < 0 || n > 9) {
        return "Digit value to be displayed must be in the range 0..9\n";
    }
    display.setVal(m, n);
    return "";
}

/**
 * @brief   'status' command handler: Display the current status of the system
 * 
 * @param h         The command handler helper we use to access what the user typed
 * @return String   The result to be displayed to the user
 */
String onStatus(CommandHandlerHelper* h) {
    String answer = BANNER " status\n";
    if (testMode) {
        answer += "Display is in test mode.\n";
    }
    if (wifiIsUp) {
        answer += "Connected to '" + String(config.ssid) + "'\n";
    }
    if (clockIsSet) {
        time_t curTime = time(nullptr);
        struct tm *t;
        t = localtime(&curTime);
        answer += "It's currently " + String(t->tm_hour) + ":" + (t->tm_min < 10 ? "0" : "") + String(t->tm_min) + "\n";
    } else {
        answer += "The system clock is not set.\n";
    }
    answer += "Display is currently showing: " + display.toString();
    return answer + "\n";
}

String onStyle(CommandHandlerHelper* h) {
    String answer = "Display mode set to ";
    switch (h->getWord(1).toInt()) {
        case 0:
            answer = "The display mode is ";
            answer += String(display.styleIs24() ? "24-hour\n" : "12-hour\n");
            break;
        case 12:
            display.setStyle24(false);
            config.style24 = false;
            answer += "12-hour\n";
            break;
        case 24:
            display.setStyle24(true);
            config.style24 = true;
            answer += "24-hour\n";
            break;
        default:
            answer = "The style must be 12, 24, or ommited (to show the current style setting)\n";
    }
    return answer;
}

/**
 * @brief   'test [on | off]' command handler: Turn test mode on or off or say whether test is on
 * 
 * @param h         The command handler helper we use to access what the user typed
 * @return String   The result to be displayed to the user
 */
String onTest(CommandHandlerHelper* h) {
    String parm = h->getWord(1);
    if (parm.length() == 0) {
        return String("Test is ") + (testMode ? "on.\n" : "off.\n");
    } else if (parm.equalsIgnoreCase("on")) {
        testMode = true;
    } else if (parm.equalsIgnoreCase("off")) {
        testMode = false;
    } else {
        return String("Test command doesn't know '") + parm + "'.\n";
    }
    return "";
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
        return "Timezone is: '" + String(config.timezone) + "'.\n";
    }
    if (target.length() > sizeof(config.timezone) - 1) {
        return "timezone string must be less than " + String(sizeof(config.timezone)) + " characters.\n";
    }
    strcpy(config.timezone, target.c_str());
    return "Timezone set to '" + target + "'.\n";
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
        return "Wifi ssid: '" + String(config.ssid) + "'\nWifi pw:   '" + String(config.pw) + "'\n";
    }
    if (!(subCmd.equals("pw") || subCmd.equals("ssid"))) {
        return "wifi command only knows about 'pw' and 'ssid'.\n";
    }
    String target = h->getCommandLine().substring(subCmd.length());
    target = target.substring(target.indexOf(subCmd) + subCmd.length());
    target.trim();
    if (target.length() <= 0) {
        return "Can't set the WiFi " + subCmd +  " to nothing at all.\n";
    }
    if (subCmd.equals("pw")) {
        if (sizeof(config.pw) < target.length() + 1) {
            return "Maximum length of a password is " + String(sizeof(config.pw) - 1) + " characters\n";
        }
        strcpy(config.pw, target.c_str());
    } else {
        if (sizeof(config.ssid) < target.length() + 1) {
            return "Maximum length of an ssid is " + String(sizeof(config.pw) - 1) + " characters\n";
        }
        strcpy(config.ssid, target.c_str());
    }
    return "Changed " + subCmd + " to '" + target + "'.\n";
}

void setup() {
    haveSavedConfig = true;         // Assume we'll succeed in retrieveing the config
    wifiIsUp = false;               // Assume we'll fail in getting the WiFi up
    clockIsSet = false;             // Assume we'll succeed in setting the system clock from NTP
    testMode = false;               // Not in testMode initially

    // Init builtin LED
    pinMode(LED, OUTPUT);
    digitalWrite(LED, HIGH);

    // Get Serial up and running
    Serial.begin(9600);
    long msStart = millis();
    while (!Serial && millis() - msStart < SERIAL_WAIT_MS) {
        delay(PAUSE_MS);
        digitalWrite(LED, digitalRead(LED) == HIGH ? LOW : HIGH);
    }
    Serial.println(BANNER);
    digitalWrite(LED, LOW);

    // Try to retrieve the configuration
    EEPROM.begin(4096);
    config = EEPROM.get(CONFIG_ADDR, config);
    if (config.signature != SIGNATURE) {
        config = defaultConfig;
        Serial.println("There's no stored configuration data; won't be able to connect to WiFi.");
        haveSavedConfig = false;    // Show we we didn't get a saved config
    }

    // Initialize the command interpreter
    if (!(
        ui.attachCmdHandler("help", onHelp) &&
        ui.attachCmdHandler("h", onHelp) &&
        ui.attachCmdHandler("ambient", onAmbient) &&
        ui.attachCmdHandler("assume", onAssume) &&
        ui.attachCmdHandler("count", onCount) &&
        ui.attachCmdHandler("home", onHome) &&
        ui.attachCmdHandler("jog", onJog) &&
        ui.attachCmdHandler("save", onSave) &&
        ui.attachCmdHandler("show", onShow) &&
        ui.attachCmdHandler("status", onStatus) &&
        ui.attachCmdHandler("style", onStyle) &&
        ui.attachCmdHandler("test", onTest) &&
        ui.attachCmdHandler("tz", onTz) &&
        ui.attachCmdHandler("wifi", onWifi)
    )) {
        Serial.print("Too many command handlers.\n");
    }

    // Connect to WiFi if we have a saved config
    if (haveSavedConfig) {
        Serial.printf("Attempting to connect to WiFi with ssid '%s'.\n", config.ssid);
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
    display.begin(config.jog);                          // Get the display going
    display.setStyle24(config.style24);
    illuminate.begin();

    // Show we're ready to go
    Serial.print("Type 'h' or 'help' for a command summary.\n");
}

void loop() {
    static int lastMin = -1;                // Minutes after the hour that we last updated the display

    // Let the command interpreter do its thing
    ui.run();

    // Update the illuminaton level
    illuminate.run();

    // If the display is supposed to be showing the time
    if (clockIsSet && !testMode) {
        //   Get the current time in struct tm format
        time_t curTime = time(nullptr);
        struct tm *curTm;
        curTm = localtime(&curTime);

        //   Update display if the minute changed
        if (curTm->tm_min != lastMin) {    
            if (lastMin == -1) {
                Serial.printf("Setting the display to the current time: %02d:%02d\n", curTm->tm_hour, curTm->tm_min);
                ui.cancelCmd();
            } 
            display.showTime(curTm->tm_hour, curTm->tm_min);
            lastMin = curTm->tm_min;
        }
    }
}
