/*
   ESP8266 FastLED WebServer: https://github.com/jasoncoon/esp8266-fastled-webserver
   Copyright (C) 2015-2018 Jason Coon

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#define FASTLED_INTERRUPT_RETRY_COUNT 1
//#define FASTLED_ALLOW_INTERRUPTS 0
#include <FastLED.h>
FASTLED_USING_NAMESPACE
#include <FS.h>

#include <WiFi.h>
#include <SPIFFS.h>

#include <EEPROM.h>
#include "GradientPalettes.h"
#include "Field.h"


/*######################## MAIN CONFIG ########################*/
#define LED_TYPE            WS2812B                     // You might also use a WS2811 or any other strip that is Fastled compatible 
#define DATA_PIN            D3                          // Be aware: the pin mapping might be different on boards like the NodeMCU
//#define CLK_PIN           D5                          // Only required when using 4-pin SPI-based LEDs
#define MOSFET_PIN          D4                          // Pin for a MOSFET, can be used to physical power off the LED stripe
#define MOSFET_LEVEL        (HIGH)                      // logic level for LED state 'on'
#define CORRECTION          UncorrectedColor            // If colors are weird use TypicalLEDStrip
#define COLOR_ORDER         GRB                         // Change this if colors are swapped (in my case, red was swapped with green)
#define MILLI_AMPS          25000                       // IMPORTANT: set the max milli-Amps of your power supply (4A = 4000mA)
#define VOLTS               5                           // Voltage of the Power Supply

#define LED_DEBUG 0                     // enable debug messages on serial console, set to 0 to disable debugging

#define LED_DEVICE_TYPE 3               // The following types are available

/*
    0: Generic LED-Strip: a regular LED-Strip without any special arrangement (and Infinity Mirror + Bottle Lighting Pad)
        * Easiest: 5V WS2812B LED-Strip:            https://s.click.aliexpress.com/e/_dZ1hCJ7
        * (Long Ranges) 12V WS2811 LED-Strip:       https://s.click.aliexpress.com/e/_d7Ehe3L
        * (High-Speed) 5V SK9822 LED-Strip:         https://s.click.aliexpress.com/e/_d8pzc89
        * (Expensive) 5V APA102 LED-Strip:          https://s.click.aliexpress.com/e/_Bf9wVZUD
        * (Flexible) 5V WS2812 S LED-Strip:         https://s.click.aliexpress.com/e/_d6XxPOH
        * Wemos D1 Mini:                            https://s.click.aliexpress.com/e/_dTVGMGl
        * 5V Power Supply:                          https://s.click.aliexpress.com/e/_dY5zCWt
        * Solderless LED-Connector:                 https://s.click.aliexpress.com/e/_dV4rsjF
        * 3D-Printed Wemos-D1 case:                 https://www.thingiverse.com/thing:3544576
    1: LED-Matrix: With a flexible LED-Matrix you can display the audio like a Audio Visualizer
        * Flexible WS2812 Matrix:                   https://s.click.aliexpress.com/e/_d84R5kp
        * Wemos D1 Mini:                            https://s.click.aliexpress.com/e/_dTVGMGl
        * 5V Power Supply:                          https://s.click.aliexpress.com/e/_dY5zCWt
    2: 3D-Printed 7-Segment Clock, display the time in a cool 7-segment style, syncs with a ntp of your choice
        * unfortunatly the "thing's" description isn't updated yet to the new standalone system
        * Project link, small version:              https://www.thingiverse.com/thing:3117494
        * Project link, large version:              https://www.thingiverse.com/thing:2968056
    3: 3D-Printed Desk Lamp, a Lamp that reacts to sound for your desk
        * Project link, twisted version:            https://www.thingiverse.com/thing:4129249
        * Project link, round version:              https://www.thingiverse.com/thing:3676533
    4: 3D-Printed Nanoleafs, a Nanoleaf clone that can be made for cheap
        * Project link:                             https://www.thingiverse.com/thing:3354082
    5: 3D-Printed Animated RGB Logos
        * Project link, Twenty-One-Pilots:          https://www.thingiverse.com/thing:3523487
        * Project link, Thingiverse:                https://www.thingiverse.com/thing:3531086
*/

//---------------------------------------------------------------------------------------------------------//
// Device Configuration:
//---------------------------------------------------------------------------------------------------------//
#if LED_DEVICE_TYPE == 3              // Desk Lamp
    #define LINE_COUNT    8             // Amount of led strip pieces
    #define LEDS_PER_LINE 10            // Amount of led pixel per single led strip piece
#endif

/*#########################################################################################################//
-----------------------------------------------------------------------------------------------------------//
  _____ ____   _  __ ____ ____ _____    ____ _  __ ___ 
 / ___// __ \ / |/ // __//  _// ___/   / __// |/ // _ \
/ /__ / /_/ //    // _/ _/ / / (_ /   / _/ /    // // /
\___/ \____//_/|_//_/  /___/ \___/   /___//_/|_//____/ 
-----------------------------------------------------------------------------------------------------------//
###########################################################################################################*/

#define VERSION "4.5"
#define VERSION_DATE "2020-02-14"

// define debugging MACROS
#if LED_DEBUG != 0
#define SERIAL_DEBUG_ADD(s) Serial.print(s);
#define SERIAL_DEBUG_ADDF(format, ...) Serial.printf(format, __VA_ARGS__);
#define SERIAL_DEBUG_EOL Serial.print("\n");
#define SERIAL_DEBUG_BOL Serial.printf("DEBUG [%lu]: ", millis());
#define SERIAL_DEBUG_LN(s) SERIAL_DEBUG_BOL SERIAL_DEBUG_ADD(s) SERIAL_DEBUG_EOL
#define SERIAL_DEBUG_LNF(format, ...) SERIAL_DEBUG_BOL SERIAL_DEBUG_ADDF(format, __VA_ARGS__) SERIAL_DEBUG_EOL
#else
#define SERIAL_DEBUG_ADD(s) do{}while(0);
#define SERIAL_DEBUG_ADDF(format, ...) do{}while(0);
#define SERIAL_DEBUG_EOL do{}while(0);
#define SERIAL_DEBUG_BOL do{}while(0);
#define SERIAL_DEBUG_LN(s) do{}while(0);
#define SERIAL_DEBUG_LNF(format, ...) do{}while(0);
#endif

#if LED_DEVICE_TYPE == 3
    #define NUM_LEDS      (LINE_COUNT * LEDS_PER_LINE)
    #define PACKET_LENGTH LEDS_PER_LINE
    #define BAND_GROUPING    1
#endif

// include config management
#include "config.h"


// #include "FSBrowser.h" currently not used
#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))
#define FRAMES_PER_SECOND  120  // here you can control the speed. With the Access Point / Web Server the animations run a bit slower.
#define SOUND_REACTIVE_FPS FRAMES_PER_SECOND

CRGB leds[NUM_LEDS];

const uint8_t brightnessCount = 5;
uint8_t brightnessMap[brightnessCount] = { 5, 32, 64, 128, 255 };
uint8_t brightnessIndex = 3;

// ten seconds per color palette makes a good demo
// 20-120 is better for deployment
uint8_t secondsPerPalette = 10;

// COOLING: How much does the air cool as it rises?
// Less cooling = taller flames.  More cooling = shorter flames.
// Default 50, suggested range 20-100
uint8_t cooling = 3;

// SPARKING: What chance (out of 255) is there that a new spark will be lit?
// Higher chance = more roaring fire.  Lower chance = more flickery fire.
// Default 120, suggested range 50-200.
uint8_t sparking = 50;

uint8_t speed = 70;

///////////////////////////////////////////////////////////////////////

// Forward declarations of an array of cpt-city gradient palettes, and
// a count of how many there are.  The actual color palette definitions
// are at the bottom of this file.
extern const TProgmemRGBGradientPalettePtr gGradientPalettes[];

uint8_t gCurrentPaletteNumber = 0;

CRGBPalette16 gCurrentPalette(CRGB::Black);
CRGBPalette16 gTargetPalette(gGradientPalettes[0]);

CRGBPalette16 IceColors_p = CRGBPalette16(CRGB::Black, CRGB::Blue, CRGB::Aqua, CRGB::White);

uint8_t currentPatternIndex = 2; // Index number of which pattern is current
uint8_t previousPatternIndex = 2; // Index number of last pattern
uint8_t autoplay = 0;

uint8_t autoplayDuration = 10;
unsigned long autoPlayTimeout = 0;

uint8_t currentPaletteIndex = 0;

uint8_t gHue = 0; // rotating "base color" used by many of the patterns
uint8_t slowHue = 0; // slower gHue
uint8_t verySlowHue = 0; // very slow gHue

CRGB solidColor = CRGB::Blue;

typedef struct {
    CRGBPalette16 palette;
    String name;
} PaletteAndName;
typedef PaletteAndName PaletteAndNameList[];

const CRGBPalette16 palettes[] = {
    RainbowColors_p,
    RainbowStripeColors_p,
    CloudColors_p,
    LavaColors_p,
    OceanColors_p,
    ForestColors_p,
    PartyColors_p,
    HeatColors_p
};

const uint8_t paletteCount = ARRAY_SIZE(palettes);

const String paletteNames[paletteCount] = {
    "Rainbow",
    "Rainbow Stripe",
    "Cloud",
    "Lava",
    "Ocean",
    "Forest",
    "Party",
    "Heat",
};

// I just don't know why. Anyone an idea?
void IfThisIsRemovedTheScatchWillFailToBuild(void) {};

typedef void(*Pattern)();
typedef Pattern PatternList[];
typedef struct {
    Pattern pattern;
    String name;
    // these settings decide if certain controls/fields are displayed in the web interface
    bool show_palette;
    bool show_speed;
    bool show_color_picker;
    bool show_cooling_sparking;
    bool show_twinkle;
} PatternAndName;
typedef PatternAndName PatternAndNameList[];

#include "TwinkleFOX.h"
#include "Twinkles.h"

// List of patterns to cycle through.  Each is defined as a separate function below.

PatternAndNameList patterns = {

    // Time patterns

#if LED_DEVICE_TYPE == 3                             // palet  speed  color  spark  twinkle
    { pride_Waves,            "Pride Waves",            true,  true,  false, false, false},
    { pride_Rings,            "Pride Rings",            true,  true,  false, false, false},
    { colorWaves_hori,        "Vertical Waves",         true,  true,  false, false, false},
    { colorWaves_vert,        "Color Rings",            true,  true,  false, false, false},
    { rainbow_vert,           "Vertical Rainbow",       true,  true,  false, false, false},
#endif

    // animation patterns                            // palet  speed  color  spark  twinkle
    { pride,                  "Pride",                  false, false, false, false, false},
    { colorWaves,             "Color Waves",            false, false, false, false, false},
    { rainbow,                "Horizontal Rainbow",     false, true,  false, false, false},
    { rainbowSolid,           "Solid Rainbow",          false, true,  false, false, false},
    { confetti,               "Confetti",               false, true,  false, false, false},
    { sinelon,                "Sinelon",                true,  true,  false, false, false},
    { bpm,                    "Beat",                   true,  true,  false, false, false},
    { juggle,                 "Juggle",                 false, true,  false, false, false},
    { fire,                   "Fire",                   false, true,  false, true,  false},
    { water,                  "Water",                  false, true,  false, true,  false},
    { solid_strobe,           "Strobe",                 false, true,  true,  false, false},
    { rainbow_strobe,         "Rainbow Strobe",         false, true,  false, false, false},
    { smooth_rainbow_strobe,  "Smooth Rainbow Strobe",  false, true,  false, false, false},

    // DigitalJohnson patterns                       // palet  speed  color  spark  twinkle
    { rainbowRoll,            "Rainbow Roll",           false, true,  false, false, false},
    { rainbowBeat,            "Rainbow Beat",           false, true,  false, false, false},
    { randomPaletteFades,     "Palette Fades",          true,  true,  false, false, false},
    { rainbowChase,           "Rainbow Chase",          false, true,  false, false, false},
    { randomDots,             "Rainbow Dots",           false, true,  false, false, false},
    { randomFades,            "Rainbow Fades",          false, true,  false, false, false},
    { policeLights,           "Police Lights",          false, true,  false, false, false},
    { glitter,                "Glitter",                false, true,  false, false, false},
    { snowFlakes,             "Snow Flakes",            false, true,  false, false, false},
    { lightning,              "Lightning",              false, false, false, false, false},

    // twinkle patterns                              // palet  speed  color  spark  twinkle
    { paletteTwinkles,        "Palette Twinkles",       true,  true,  false, false, true},
    { snowTwinkles,           "Snow Twinkles",          false, true,  false, false, true},
    { incandescentTwinkles,   "Incandescent Twinkles",  false, true,  false, false, true},

    // TwinkleFOX patterns                                 // palet  speed  color  spark  twinkle
    { retroC9Twinkles,        "Retro C9 Twinkles",            false, true,  false, false, true},
    { redWhiteTwinkles,       "Red & White Twinkles",         false, true,  false, false, true},
    { blueWhiteTwinkles,      "Blue & White Twinkles",        false, true,  false, false, true},
    { redGreenWhiteTwinkles,  "Red, Green & White Twinkles",  false, true,  false, false, true},
    { fairyLightTwinkles,     "Fairy Light Twinkles",         false, true,  false, false, true},
    { snow2Twinkles,          "Snow 2 Twinkles",              false, true,  false, false, true},
    { hollyTwinkles,          "Holly Twinkles",               false, true,  false, false, true},
    { iceTwinkles,            "Ice Twinkles",                 false, true,  false, false, true},
    { partyTwinkles,          "Party Twinkles",               false, true,  false, false, true},
    { forestTwinkles,         "Forest Twinkles",              false, true,  false, false, true},
    { lavaTwinkles,           "Lava Twinkles",                false, true,  false, false, true},
    { fireTwinkles,           "Fire Twinkles",                false, true,  false, false, true},
    { cloud2Twinkles,         "Cloud 2 Twinkles",             false, true,  false, false, true},
    { oceanTwinkles,          "Ocean Twinkles",               false, true,  false, false, true},
    { showSolidColor,         "Solid Color",                  false, false, true,  false, false}
};

const uint8_t patternCount = ARRAY_SIZE(patterns);

#include "Fields.h"

// ######################## define setup() and loop() ####################

void setup() {
    Serial.begin(115200);

    delay(100);
    Serial.print("\n\n");
    
    FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);         // WS2812 (Neopixel)
    
    FastLED.setDither(false);
    FastLED.setCorrection(CORRECTION);
    FastLED.setBrightness(brightness);
    FastLED.setMaxPowerInVoltsAndMilliamps(VOLTS, MILLI_AMPS);
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();

    // set a default config to be used on config reset
    default_cfg.brightness = brightness;
    default_cfg.currentPatternIndex = currentPatternIndex;
    default_cfg.power = power;
    default_cfg.autoplay = autoplay;
    default_cfg.autoplayDuration = autoplayDuration;
    default_cfg.currentPaletteIndex = currentPaletteIndex;
    default_cfg.speed = speed;

    loadConfig();

    FastLED.setBrightness(brightness);

    //  irReceiver.enableIRIn(); // Start the receiver

    SERIAL_DEBUG_EOL
    SERIAL_DEBUG_LN(F("System Information:"))
    SERIAL_DEBUG_LNF("Version: %s (%s)", VERSION, VERSION_DATE)
    SERIAL_DEBUG_LNF("Heap: %d", system_get_free_heap_size())
    SERIAL_DEBUG_LNF("SDK: %s", system_get_sdk_version())
    SERIAL_DEBUG_LNF("CPU Speed: %d MHz", ESP.getCpuFreqMHz())
    SERIAL_DEBUG_LNF("Flash Size: %dKB", ESP.getFlashChipSize())
    SERIAL_DEBUG_LNF("MAC address: %s", WiFi.macAddress().c_str())
    SERIAL_DEBUG_EOL

#if defined(MOSFET_PIN) && defined(MOSFET_LEVEL)           
    pinMode(MOSFET_PIN, OUTPUT);
    digitalWrite(MOSFET_PIN, power == 1 ? MOSFET_LEVEL : !MOSFET_LEVEL);
#endif

    // starting file system
    if (!SPIFFS.begin ()) {
        Serial.println(F("An Error has occurred while mounting SPIFFS"));
        return;
    }

    // FS debug information
    // THIS NEEDS TO BE PAST THE WIFI SETUP!! OTHERWISE WIFI SETUP WILL BE DELAYED
    #if LED_DEBUG != 0
        SERIAL_DEBUG_LN(F("SPIFFS contents:"))
        File root = SPIFFS.open("/");
        File file = root.openNextFile();
        while (file) {
            SERIAL_DEBUG_LNF("FS File: %s, size: %lu", file.name(), file.size())
            file = root.openNextFile();
        }
        SERIAL_DEBUG_EOL
        unsigned int totalBytes = SPIFFS.totalBytes();
        unsigned int usedBytes = SPIFFS.usedBytes();
        if (usedBytes == 0) {
            SERIAL_DEBUG_LN(F("NO WEB SERVER FILES PRESENT! SEE: https://github.com/NimmLor/esp8266-fastled-iot-webserver/blob/master/Software_Installation.md#32-sketch-data-upload\n"))
        }
        SERIAL_DEBUG_LNF("FS Size: %luKB, used: %luKB, %0.2f%%", \
                          totalBytes, usedBytes, \
                          (float) 100 / totalBytes * usedBytes)
        SERIAL_DEBUG_EOL
    #endif

    // print setup details
    SERIAL_DEBUG_LNF("Arduino Core Version: %s", ARDUINO_ESP32_RELEASE)
    SERIAL_DEBUG_LN(F("Enabled Features:"))
    #ifdef ENABLE_MQTT_SUPPORT
        SERIAL_DEBUG_LNF("Feature: MQTT support enabled (mqtt version: %s)", String(MQTT_VERSION).c_str())
    #endif
    SERIAL_DEBUG_EOL

    switch(LED_DEVICE_TYPE) {
        case 0: SERIAL_DEBUG_LN("Configured device type: LED strip (0)") break;
        case 1: SERIAL_DEBUG_LN("Configured device type: LED MATRIX (1)") break;
        case 2: SERIAL_DEBUG_LN("Configured device type: 7-Segment Clock (2)") break;
        case 3: SERIAL_DEBUG_LN("Configured device type: Desk Lamp (3)") break;
        case 4: SERIAL_DEBUG_LN("Configured device type: Nanoleafs (4)") break;
        case 5: SERIAL_DEBUG_LN("Configured device type: Animated Logos (5)") break;
    }

    SERIAL_DEBUG_LNF("NUM_LEDS: %d", NUM_LEDS)
    SERIAL_DEBUG_LNF("BAND_GROUPING: %d", BAND_GROUPING)
    SERIAL_DEBUG_LNF("PACKET_LENGTH: %d", PACKET_LENGTH)

    webServer.on("/reset", HTTP_POST, []() {

        // delete EEPROM settings
        if (webServer.arg("type") == String("all")) {
            resetConfig();
            SERIAL_DEBUG_LN("Resetting config")
        }

        // delete wireless config
        if (webServer.arg("type") == String("wifi") || webServer.arg("type") == String("all")) {
            setWiFiConf(String(""), String(""));
            SERIAL_DEBUG_LN("Resetting wifi settings");
        }
        webServer.send(200, "text/html", "<html><head></head><body><font face='arial'><b><h2>Config reset finished. Device is rebooting now and you need to connect to the wireless again.</h2></b></font></body></html>");
        delay(500);
        ESP.restart();
        });

    webServer.on("/fieldValue", HTTP_GET, []() {
        String name = webServer.arg("name");
        String value = getFieldValue(name, fields, fieldCount);
        webServer.send(200, "text/json", value);
        });

    webServer.on("/fieldValue", HTTP_POST, []() {
        String name = webServer.arg("name");
        String value = webServer.arg("value");
        String newValue = setFieldValue(name, value, fields, fieldCount);
        webServer.send(200, "text/json", newValue);
        });

    webServer.on("/power", []() {
        String value = webServer.arg("value");
        value.toLowerCase();
        if (value == String("1") || value == String("on")) {
            setPower(1);
        } else if (value == String("0") || value == String("off")) {
            setPower(0);
        } else if (value == String("toggle")) {
            setPower((power == 1) ? 0 : 1);
        }
        sendInt(power);
        });

    webServer.on("/cooling", []() {
        String value = webServer.arg("value");
        cooling = value.toInt();
        broadcastInt("cooling", cooling);
        sendInt(cooling);
        });

    webServer.on("/sparking", []() {
        String value = webServer.arg("value");
        sparking = value.toInt();
        broadcastInt("sparking", sparking);
        sendInt(sparking);
        });

    webServer.on("/speed", []() {
        String value = webServer.arg("value");
        setSpeed(value.toInt());
        sendInt(speed);
        });

    webServer.on("/twinkleDensity", []() {
        String value = webServer.arg("value");
        twinkleDensity = value.toInt();
        SERIAL_DEBUG_LNF("Setting: twinkle density %d", twinkleDensity)
        broadcastInt("twinkleDensity", twinkleDensity);
        sendInt(twinkleDensity);
        });

    webServer.on("/solidColor", []() {
        String r = webServer.arg("r");
        String g = webServer.arg("g");
        String b = webServer.arg("b");
        setSolidColor(r.toInt(), g.toInt(), b.toInt(), false);
        sendString(String(solidColor.r) + "," + String(solidColor.g) + "," + String(solidColor.b));
        });

    webServer.on("/hue", []() {
        String value = webServer.arg("value");
        setSolidColorHue(value.toInt(), false);
        sendString(String(solidColor.r) + "," + String(solidColor.g) + "," + String(solidColor.b));
        });

    webServer.on("/saturation", []() {
        String value = webServer.arg("value");
        setSolidColorSat(value.toInt(), false);
        sendString(String(solidColor.r) + "," + String(solidColor.g) + "," + String(solidColor.b));
        });

    webServer.on("/pattern", []() {
        String value = webServer.arg("value");
        setPattern(value.toInt());
        sendInt(currentPatternIndex);
        });

    webServer.on("/patternName", []() {
        String value = webServer.arg("value");
        setPatternName(value);
        sendInt(currentPatternIndex);
        });

    webServer.on("/palette", []() {
        String value = webServer.arg("value");
        setPalette(value.toInt());
        sendInt(currentPaletteIndex);
        });

    webServer.on("/paletteName", []() {
        String value = webServer.arg("value");
        setPaletteName(value);
        sendInt(currentPaletteIndex);
        });

    webServer.on("/brightness", []() {
        String value = webServer.arg("value");
        setBrightness(value.toInt());
        sendInt(brightness);
        });

    webServer.on("/autoplay", []() {
        String value = webServer.arg("value");
        value.toLowerCase();
        if (value == String("1") || value == String("on")) {
            setAutoplay(1);
        } else if (value == String("0") || value == String("off")) {
            setAutoplay(0);
        } else if (value == String("toggle")) {
            setAutoplay((autoplay == 1) ? 0 : 1);
        }
        sendInt(autoplay);
        });

    webServer.on("/autoplayDuration", []() {
        String value = webServer.arg("value");
        setAutoplayDuration(value.toInt());
        sendInt(autoplayDuration);
        });


    //list directory
    /* // Currently no directory/file functions are used
    webServer.on("/list", HTTP_GET, handleFileList);
    //load editor
    webServer.on("/edit", HTTP_GET, []() {
        if (!handleFileRead("/edit.htm")) webServer.send(404, "text/plain", "FileNotFound");
        });
    //create file
    webServer.on("/edit", HTTP_PUT, handleFileCreate);
    //delete file
    webServer.on("/edit", HTTP_DELETE, handleFileDelete);
    //first callback is called after the request has ended with all parsed arguments
    //second callback handles file uploads at that location
    webServer.on("/edit", HTTP_POST, []() {
        webServer.send(200, "text/plain", "");
        }, handleFileUpload);
        */
    webServer.serveStatic("/", SPIFFS, "/", "max-age=86400");
    autoPlayTimeout = millis() + (autoplayDuration * 1000);
}

void loop() {

    static unsigned int loop_counter = 0;
    static unsigned int current_fps = FRAMES_PER_SECOND;
    static unsigned int frame_delay = (1000 / FRAMES_PER_SECOND) * 1000; // in micro seconds

    // insert a delay to keep the framerate modest
    // delayMicroseconds max value is 16383
    if (frame_delay < 16000){
        delayMicroseconds(frame_delay);
    } else {
        delay(frame_delay / 1000);
    }

    // Add entropy to random number generator; we use a lot of it.
    random16_add_entropy(random(65535));

    EVERY_N_SECONDS(10) {
      SERIAL_DEBUG_LNF("Heap: %d", system_get_free_heap_size())
    }

    // change to a new cpt-city gradient palette
    EVERY_N_SECONDS(secondsPerPalette) {
        gCurrentPaletteNumber = addmod8(gCurrentPaletteNumber, 1, gGradientPaletteCount);
        gTargetPalette = gGradientPalettes[gCurrentPaletteNumber];
    }

    EVERY_N_MILLISECONDS(40) {
        // slowly blend the current palette to the next
        nblendPaletteTowardPalette(gCurrentPalette, gTargetPalette, 8);
    }

    updateHue();

    if (autoplay && (millis() > autoPlayTimeout)) {
        adjustPattern(true);
        autoPlayTimeout = millis() + (autoplayDuration * 1000);
    }

    if (power == 0) {
        fadeToBlackBy(leds, NUM_LEDS, 5);
    } else {
        // Call the current pattern function once, updating the 'leds' array
        patterns[currentPatternIndex].pattern();
    }

    FastLED.show();

    // call to save config if config has changed
    saveConfig();

    // every second calculate the FPS and adjust frame delay to keep FPS smooth
    EVERY_N_SECONDS(1) {
        current_fps = loop_counter;
        // frame delay stepping: 50 us
        // fps sliding window +/- 1 frame
        // too fast, we need to slow down. Don't increase the frame delay past 20 ms
        if (current_fps > FRAMES_PER_SECOND + 1 && frame_delay <= 20000) {
            int factor = current_fps - FRAMES_PER_SECOND; // factor for faster speed adjustment
            if (factor < 1) factor = 1;
            frame_delay += (50 * factor);

        // too slow, we need to speed up a little bit
        } else if (current_fps < FRAMES_PER_SECOND - 1 && frame_delay > 0) {
            int factor = FRAMES_PER_SECOND - current_fps;
            if (factor < 1) factor = 1;

            if (frame_delay < (50 * factor)) {
                frame_delay = 0;
            } else {
                frame_delay -= (50 * factor);
            }
        }
        SERIAL_DEBUG_LNF("Stats: %lu frames/s, frame delay: %d us", current_fps, frame_delay)
        loop_counter = 0;
    }
    loop_counter += 1;
    previousPatternIndex = currentPatternIndex;
}

void loadConfig() {

    SERIAL_DEBUG_LN(F("Loading config"))

    // Loads configuration from EEPROM into RAM
    EEPROM.begin(4095);
    EEPROM.get(0, cfg );
    EEPROM.end();

    brightness = cfg.brightness;

    currentPatternIndex = cfg.currentPatternIndex;
    if (currentPatternIndex < 0)
        currentPatternIndex = 0;
    else if (currentPatternIndex >= patternCount)
        currentPatternIndex = patternCount - 1;

    byte r = cfg.red;
    byte g = cfg.green;
    byte b = cfg.blue;

    if (r != 0 && g != 0 && b != 0) {
        solidColor = CRGB(r, g, b);
    }

    power = cfg.power;

    autoplay = cfg.autoplay;
    autoplayDuration = cfg.autoplayDuration;

    currentPaletteIndex = cfg.currentPaletteIndex;
    if (currentPaletteIndex < 0)
        currentPaletteIndex = 0;
    else if (currentPaletteIndex >= paletteCount)
        currentPaletteIndex = paletteCount - 1;

    speed = cfg.speed;
    twinkleSpeed = map(speed, 0, 255, 0, 8);

    if (!isValidHostname(cfg.hostname, sizeof(cfg.hostname))) {
        strncpy(cfg.hostname, DEFAULT_HOSTNAME, sizeof(cfg.hostname));
        setConfigChanged();
    }
}

// ######################## web server functions #########################

String getRebootString() {
    return "<html><head><meta http-equiv=\"refresh\" content=\"4; url=/\"/></head><body><font face='arial'><b><h2>Rebooting... returning in 4 seconds</h2></b></font></body></html>";
}

void handleReboot() {
    webServer.send(200, "text/html", getRebootString());
    delay(500);
    ESP.restart();
}


void sendInt(uint8_t value) {
    sendString(String(value));
}


// ############## functions to update current settings ###################

void setSolidColor(uint8_t r, uint8_t g, uint8_t b, bool updatePattern)
{
    solidColor = CRGB(r, g, b);

    cfg.red = r;
    cfg.green = g;
    cfg.blue = b;
    setConfigChanged();

    if (updatePattern && currentPatternIndex != patternCount - 2)setPattern(patternCount - 1);

    SERIAL_DEBUG_LNF("Setting: solid Color: red %d, green %d, blue %d", r, g ,b)
    broadcastString("color", String(solidColor.r) + "," + String(solidColor.g) + "," + String(solidColor.b));
}

void setSolidColor(CRGB color, bool updatePattern)
{
    setSolidColor(color.r, color.g, color.b, updatePattern);
}

void setSolidColorHue(uint8_t hue, bool updatePattern)
{
    CRGB color = solidColor;
    CHSV temp_chsv = rgb2hsv_approximate(color);
    temp_chsv.hue = hue;
    hsv2rgb_rainbow(temp_chsv, color);
    setSolidColor(color.r, color.g, color.b, updatePattern);
}

void setSolidColorSat(uint8_t sat, bool updatePattern)
{
    CRGB color = solidColor;
    CHSV temp_chsv = rgb2hsv_approximate(color);
    temp_chsv.sat = sat;
    hsv2rgb_rainbow(temp_chsv, color);
    setSolidColor(color.r, color.g, color.b, updatePattern);
}

void setPower(uint8_t value)
{
    power = value == 0 ? 0 : 1;

    cfg.power = power;
    setConfigChanged();
    SERIAL_DEBUG_LNF("Setting: power %s", (power == 0) ? "off" : "on")
    broadcastInt("power", power);

    #if defined(MOSFET_PIN) && defined(MOSFET_LEVEL)
        digitalWrite(MOSFET_PIN, power == 1 ? MOSFET_LEVEL : !MOSFET_LEVEL);
    #endif
}

void setAutoplay(uint8_t value)
{
    autoplay = value == 0 ? 0 : 1;

    cfg.autoplay = autoplay;
    setConfigChanged();
    SERIAL_DEBUG_LNF("Setting: autoplay %s", (autoplay == 0) ? "off" : "on")
    broadcastInt("autoplay", autoplay);
}

void setAutoplayDuration(uint8_t value)
{
    autoplayDuration = value;

    cfg.autoplayDuration = autoplayDuration;
    setConfigChanged();

    autoPlayTimeout = millis() + (autoplayDuration * 1000);
    SERIAL_DEBUG_LNF("Setting: autoplay duration: %d seconds", autoplayDuration)
    broadcastInt("autoplayDuration", autoplayDuration);
}

// increase or decrease the current pattern number, and wrap around at the ends
void adjustPattern(bool up)
{
    if (autoplay == 1) {
#ifdef RANDOM_AUTOPLAY_PATTERN
        uint8_t lastpattern = currentPatternIndex;
        while (currentPatternIndex == lastpattern)
        {
            uint8_t newpattern = random8(0, patternCount - 1);
            if (newpattern != lastpattern) currentPatternIndex = newpattern;
        }
#else // RANDOM_AUTOPLAY_PATTERN
        currentPatternIndex++;
#endif
    }

    if (autoplay == 0)
    {
        if (up)
            currentPatternIndex++;
        else
            currentPatternIndex--;
    }
    // wrap around at the ends
    if (currentPatternIndex < 0)
        currentPatternIndex = patternCount - 1;
    if (currentPatternIndex >= patternCount)
        currentPatternIndex = 0;

    if (autoplay == 0) {
        cfg.currentPatternIndex = currentPatternIndex;
        setConfigChanged();
    }

    SERIAL_DEBUG_LNF("Setting: pattern: %s", patterns[currentPatternIndex].name.c_str())

    broadcastInt("pattern", currentPatternIndex);
}

void setPattern(uint8_t value)
{
    if (value >= patternCount)
        value = patternCount - 1;

    currentPatternIndex = value;

    if (autoplay != 1) {
        cfg.currentPatternIndex = currentPatternIndex;
        setConfigChanged();
    }

    SERIAL_DEBUG_LNF("Setting: pattern: %s", patterns[currentPatternIndex].name.c_str())

    broadcastInt("pattern", currentPatternIndex);
}

void setPatternName(String name)
{
    for (uint8_t i = 0; i < patternCount; i++) {
        if (patterns[i].name == name) {
            setPattern(i);
            break;
        }
    }
}

void setPalette(uint8_t value)
{
    if (value >= paletteCount)
        value = paletteCount - 1;

    currentPaletteIndex = value;

    cfg.currentPaletteIndex = currentPaletteIndex;
    setConfigChanged();

    SERIAL_DEBUG_LNF("Setting: pallette: %s", paletteNames[currentPaletteIndex].c_str())
    broadcastInt("palette", currentPaletteIndex);
}

void setPaletteName(String name)
{
    for (uint8_t i = 0; i < paletteCount; i++) {
        if (paletteNames[i] == name) {
            setPalette(i);
            break;
        }
    }
}

void adjustBrightness(bool up)
{
    if (up && brightnessIndex < brightnessCount - 1)
        brightnessIndex++;
    else if (!up && brightnessIndex > 0)
        brightnessIndex--;

    setBrightness(brightnessMap[brightnessIndex]);
}

void setBrightness(uint8_t value)
{
    if (value > 255)
        value = 255;
    else if (value < 0) value = 0;

    brightness = value;

    FastLED.setBrightness(brightness);

    cfg.brightness = brightness;
    setConfigChanged();
    SERIAL_DEBUG_LNF("Setting: brightness: %d", brightness)
    broadcastInt("brightness", brightness);
}

void setSpeed(uint8_t value)
{
    if (value > 255)
        value = 255;
    else if (value < 0) value = 0;

    speed = value;

    twinkleSpeed = map(speed, 0, 255, 0, 8);

    cfg.speed = speed;
    setConfigChanged();
    SERIAL_DEBUG_LNF("Setting: speed: %d", speed)
    broadcastInt("speed", speed);
}

// genric functions to map current values to desired range
float getBrightnessMapped(float min, float max) {
    return mapfloat((float) brightness, 0.0, 255.0, min, max);
}
uint8_t getBrightnessMapped(uint8_t min, uint8_t max) {
    return map(brightness, 0, 255, min, max);
}
float getHueMapped(float min, float max) {
    return mapfloat(rgb2hsv_approximate(solidColor).hue, 0.0, 255.0, min, max);
}
uint8_t getHueMapped(uint8_t min, uint8_t max) {
    return map(rgb2hsv_approximate(solidColor).hue, 0, 255, min, max);
}
float getSatMapped(float min, float max) {
    return map(rgb2hsv_approximate(solidColor).sat, 0.0, 255.0, min, max);
}
uint8_t getSatMapped(uint8_t min, uint8_t max) {
    return map(rgb2hsv_approximate(solidColor).sat, 0, 255, min, max);
}

float mapfloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// ######################### pattern functions ###########################

void updateHue()
{
    uint8_t hueUpdateInterval = 40;
    uint8_t hueStep = 1;
    static unsigned long nextHueUpdate = millis();

    // adds speed control for some Rainbow patterns
    if (patterns[currentPatternIndex].name == String("Horizontal Rainbow") or \
        patterns[currentPatternIndex].name == String("Solid Rainbow") or \
        patterns[currentPatternIndex].name == String("Rainbow Roll")) {

        if (speed < 128) {
            hueUpdateInterval = map(speed, 0, 255, 100, 0);
        } else {
            hueUpdateInterval = map(speed, 0, 255, 200, 0);
            hueStep = 2;
        }
    }

    if (millis() > nextHueUpdate) {
        gHue += hueStep;  // slowly cycle the "base color" through the rainbow
        if (gHue % 16 == 0)slowHue++;
        if (gHue % 127 == 0)verySlowHue++;
        nextHueUpdate = millis() + hueUpdateInterval;
    }
}

bool updatePatternBasedOnSpeedSetting(uint8_t max_delay)
{
    uint8_t updateInterval = 0;
    static unsigned long nexUpdate = millis();

    updateInterval = map(speed, 0, 255, max_delay, 0);

    if (millis() > nexUpdate) {
        nexUpdate = millis() + updateInterval;
        return true;
    }

    return false;
}
void strandTest()
{
    static uint8_t i = 0;

    EVERY_N_SECONDS(1)
    {
        i++;
        if (i >= NUM_LEDS)
            i = 0;
    }

    fill_solid(leds, NUM_LEDS, CRGB::Black);

    leds[i] = solidColor;
}

void showSolidColor()
{
    fill_solid(leds, NUM_LEDS, solidColor);
}

// Patterns from FastLED example DemoReel100: https://github.com/FastLED/FastLED/blob/master/examples/DemoReel100/DemoReel100.ino

void smooth_rainbow_strobe()
{
    if (autoplay == 1)adjustPattern(true);
    uint8_t beat = beatsin8(speed, 0, 255);
    fill_solid(leds, NUM_LEDS, CHSV(gHue, 255, beat));
}

void strobe(bool rainbow)
{
    if (autoplay == 1)adjustPattern(true);
    static bool p = false;
    static long lm = 0;
    if (millis() - lm > (128 - (speed / 2)))
    {
        if (p) fill_solid(leds, NUM_LEDS, CRGB(0, 0, 0));
        else {
            if (rainbow) {
                fill_solid(leds, NUM_LEDS, CHSV(gHue, 255, 255));
            } else {
                fill_solid(leds, NUM_LEDS, solidColor);
            }
        }
        lm = millis();
        p = !p;
    }
}

void rainbow_strobe()
{
    strobe(true);
}

void solid_strobe()
{
    strobe(false);
}

void rainbow()
{
    // FastLED's built-in rainbow generator
    fill_rainbow(leds, NUM_LEDS, gHue, 255 / NUM_LEDS);
}

void rainbowWithGlitter()
{
    // built-in FastLED rainbow, plus some random sparkly glitter
    rainbow();
    addGlitter(80);
}

void rainbowSolid()
{
    fill_solid(leds, NUM_LEDS, CHSV(gHue, 255, 255));
}

void confetti()
{
    if (updatePatternBasedOnSpeedSetting(100) == false)
        return;

    // random colored speckles that blink in and fade smoothly
    fadeToBlackBy(leds, NUM_LEDS, 10);
    int pos = random16(NUM_LEDS);
    // leds[pos] += CHSV( gHue + random8(64), 200, 255);
    leds[pos] += ColorFromPalette(palettes[currentPaletteIndex], gHue + random8(64));
}

void sinelon()
{
    // a colored dot sweeping back and forth, with fading trails
    fadeToBlackBy(leds, NUM_LEDS, 20);
    int pos = beatsin16(speed / 4, 0, NUM_LEDS);
    static int prevpos = 0;
    CRGB color = ColorFromPalette(palettes[currentPaletteIndex], gHue, 255);
    if (pos < prevpos) {
        fill_solid(leds + pos, (prevpos - pos) + 1, color);
    }
    else {
        fill_solid(leds + prevpos, (pos - prevpos) + 1, color);
    }
    prevpos = pos;
}

void bpm()
{
    // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
    uint8_t beat = beatsin8(speed, 64, 255);
    CRGBPalette16 palette = palettes[currentPaletteIndex];
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = ColorFromPalette(palette, gHue + (i * 2), beat - gHue + (i * 10));
    }
}

void juggle()
{
    static uint8_t    numdots = 4; // Number of dots in use.
    static uint8_t   faderate = 2; // How long should the trails be. Very low value = longer trails.
    static uint8_t     hueinc = 255 / numdots - 1; // Incremental change in hue between each dot.
    static uint8_t    thishue = 0; // Starting hue.
    static uint8_t     curhue = 0; // The current hue
    static uint8_t    thissat = 255; // Saturation of the colour.
    static uint8_t thisbright = 255; // How bright should the LED/display be.
    static uint8_t   basebeat = 5; // Higher = faster movement.

    static uint8_t lastSecond = 99;  // Static variable, means it's only defined once. This is our 'debounce' variable.
    uint8_t secondHand = (millis() / 1000) % 30; // IMPORTANT!!! Change '30' to a different value to change duration of the loop.

    if (updatePatternBasedOnSpeedSetting(100) == false)
        return;

    if (lastSecond != secondHand) { // Debounce to make sure we're not repeating an assignment.
        lastSecond = secondHand;
        switch (secondHand) {
        //case  0: numdots = 1; basebeat = 20; hueinc = 16; faderate = 2; thishue = 0; break; // You can change values here, one at a time , or altogether.
        case 10: numdots = 4; basebeat = 10; hueinc = 16; faderate = 8; thishue = 128; break;
        case 20: numdots = 8; basebeat = 5; hueinc = 0; faderate = 8; thishue = random8(); break; // Only gets called once, and not continuously for the next several seconds. Therefore, no rainbows.
        case 30: break;
        }
    }

    // Several colored dots, weaving in and out of sync with each other
    curhue = thishue; // Reset the hue values.
    fadeToBlackBy(leds, NUM_LEDS, faderate);
    for (int i = 0; i < numdots; i++) {
        //beat16 is a FastLED 3.1 function
        leds[beatsin16(basebeat + i + numdots, 0, NUM_LEDS)] += CHSV(gHue + curhue, thissat, thisbright);
        curhue += hueinc;
    }
}

void fire()
{
    heatMap(HeatColors_p, true);
}

void water()
{
    heatMap(IceColors_p, false);
}

// Pride2015 by Mark Kriegsman: https://gist.github.com/kriegsman/964de772d64c502760e5
// This function draws rainbows with an ever-changing,
// widely-varying set of parameters.
void pride()
{
    static uint16_t sPseudotime = 0;
    static uint16_t sLastMillis = 0;
    static uint16_t sHue16 = 0;

    uint8_t sat8 = beatsin88(87, 220, 250);
    uint8_t brightdepth = beatsin88(341, 96, 224);
    uint16_t brightnessthetainc16 = beatsin88(203, (25 * 256), (40 * 256));
    uint8_t msmultiplier = beatsin88(147, 23, 60);

    uint16_t hue16 = sHue16;//gHue * 256;
    uint16_t hueinc16 = beatsin88(113, 1, 3000);

    uint16_t ms = millis();
    uint16_t deltams = ms - sLastMillis;
    sLastMillis = ms;
    sPseudotime += deltams * msmultiplier;
    sHue16 += deltams * beatsin88(400, 5, 9);
    uint16_t brightnesstheta16 = sPseudotime;
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
        hue16 += hueinc16;
        uint8_t hue8 = hue16 / 256;

        brightnesstheta16 += brightnessthetainc16;
        uint16_t b16 = sin16(brightnesstheta16) + 32768;

        uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
        uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
        bri8 += (255 - brightdepth);

        CRGB newcolor = CHSV(hue8, sat8, bri8);

        uint16_t pixelnumber = i;
        pixelnumber = (NUM_LEDS - 1) - pixelnumber;
        nblend(leds[pixelnumber], newcolor, 64);
    }
}

void radialPaletteShift()
{
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
        // leds[i] = ColorFromPalette( gCurrentPalette, gHue + sin8(i*16), brightness);
        leds[i] = ColorFromPalette(gCurrentPalette, i + gHue, 255, LINEARBLEND);
    }
}

// based on FastLED example Fire2012WithPalette: https://github.com/FastLED/FastLED/blob/master/examples/Fire2012WithPalette/Fire2012WithPalette.ino
void heatMap(CRGBPalette16 palette, bool up)
{
    if (updatePatternBasedOnSpeedSetting(50) == false)
        return;

    fill_solid(leds, NUM_LEDS, CRGB::Black);

    // Add entropy to random number generator; we use a lot of it.
    random16_add_entropy(random(256));

    // Array of temperature readings at each simulation cell
    static byte heat[NUM_LEDS];

    byte colorindex;

    // Step 1.  Cool down every cell a little
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
        heat[i] = qsub8(heat[i], random8(0, ((cooling * 10) / NUM_LEDS) + 2));
    }

    // Step 2.  Heat from each cell drifts 'up' and diffuses a little
    for (uint16_t k = NUM_LEDS - 1; k >= 2; k--) {
        heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
    }

    // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
    if (random8() < sparking) {
        int y = random8(7);
        heat[y] = qadd8(heat[y], random8(160, 255));
    }

    // Step 4.  Map from heat cells to LED colors
    for (uint16_t j = 0; j < NUM_LEDS; j++) {
        // Scale the heat value from 0-255 down to 0-240
        // for best results with color palettes.
        colorindex = scale8(heat[j], 190);

        CRGB color = ColorFromPalette(palette, colorindex);

        if (up) {
            leds[j] = color;
        }
        else {
            leds[(NUM_LEDS - 1) - j] = color;
        }
    }
}

void addGlitter(uint8_t chanceOfGlitter)
{
    if (random8() < chanceOfGlitter) {
        leds[random16(NUM_LEDS)] += CRGB::White;
    }
}

///////////////////////////////////////////////////////////////////////

// Forward declarations of an array of cpt-city gradient palettes, and
// a count of how many there are.  The actual color palette definitions
// are at the bottom of this file.
extern const TProgmemRGBGradientPalettePtr gGradientPalettes[];
extern const uint8_t gGradientPaletteCount;

uint8_t beatsaw8(accum88 beats_per_minute, uint8_t lowest, uint8_t highest,
    uint32_t timebase, uint8_t phase_offset)
{
    uint8_t beat = beat8(beats_per_minute, timebase);
    uint8_t beatsaw = beat + phase_offset;
    uint8_t rangewidth = highest - lowest;
    uint8_t scaledbeat = scale8(beatsaw, rangewidth);
    uint8_t result = lowest + scaledbeat;
    return result;
}

void colorWaves()
{
    colorwaves(leds, NUM_LEDS, gCurrentPalette);
}

// ColorWavesWithPalettes by Mark Kriegsman: https://gist.github.com/kriegsman/8281905786e8b2632aeb
// This function draws color waves with an ever-changing,
// widely-varying set of parameters, using a color palette.
void colorwaves(CRGB* ledarray, uint16_t numleds, CRGBPalette16& palette)
{
    static uint16_t sPseudotime = 0;
    static uint16_t sLastMillis = 0;
    static uint16_t sHue16 = 0;

    // uint8_t sat8 = beatsin88( 87, 220, 250);
    uint8_t brightdepth = beatsin88(341, 96, 224);
    uint16_t brightnessthetainc16 = beatsin88(203, (25 * 256), (40 * 256));
    uint8_t msmultiplier = beatsin88(147, 23, 60);

    uint16_t hue16 = sHue16;//gHue * 256;
    uint16_t hueinc16 = beatsin88(113, 300, 1500);

    uint16_t ms = millis();
    uint16_t deltams = ms - sLastMillis;
    sLastMillis = ms;
    sPseudotime += deltams * msmultiplier;
    sHue16 += deltams * beatsin88(400, 5, 9);
    uint16_t brightnesstheta16 = sPseudotime;

    for (uint16_t i = 0; i < numleds; i++) {
        hue16 += hueinc16;
        uint8_t hue8 = hue16 / 256;
        uint16_t h16_128 = hue16 >> 7;
        if (h16_128 & 0x100) {
            hue8 = 255 - (h16_128 >> 1);
        }
        else {
            hue8 = h16_128 >> 1;
        }

        brightnesstheta16 += brightnessthetainc16;
        uint16_t b16 = sin16(brightnesstheta16) + 32768;

        uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
        uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
        bri8 += (255 - brightdepth);

        uint8_t index = hue8;
        //index = triwave8( index);
        index = scale8(index, 240);

        CRGB newcolor = ColorFromPalette(palette, index, bri8);

        uint16_t pixelnumber = i;
        pixelnumber = (numleds - 1) - pixelnumber;

        nblend(ledarray[pixelnumber], newcolor, 128);
    }
}

// Alternate rendering function just scrolls the current palette
// across the defined LED strip.
void palettetest(CRGB* ledarray, uint16_t numleds, const CRGBPalette16& gCurrentPalette)
{
    static uint8_t startindex = 0;
    startindex--;
    fill_palette(ledarray, numleds, startindex, (256 / NUM_LEDS) + 1, gCurrentPalette, 255, LINEARBLEND);
}



//########################### Patterns by DigitalJohnson ###########################

TBlendType    blendType;
TBlendType currentBlending; // Current blending type

struct timer_struct
{
    unsigned long period;
    unsigned long mark;
    bool enabled = false;
};

// Brightness level per pattern
const uint8_t brightVal[ARRAY_SIZE(patterns)] =
{
    192, 192, 225, 225, 225, 225, 225, 255, 255, 192, 225
};

// Delay for incrementing gHue variable per pattern
const uint8_t hueStep[ARRAY_SIZE(patterns)] =
{
    10, 15, 8, 1, 10, 1, 1, 1, 1, 1, 1
};

// Delay inserted into loop() per pattern
unsigned long patternDelay[ARRAY_SIZE(patterns)] =
{
    0, 0, 0, 55, 55, 5, 10, 15, 15, 15, 0
};

// ######################### pattern functions ###########################

void rainbowRoll()
{
    // FastLED's built-in rainbow generator
    fill_rainbow(leds, NUM_LEDS, gHue, 7);
}

void rainbowBeat()
{
    // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
    uint8_t beat = beatsin8(speed, 64, 255); // Beat advances and retreats in a sine wave
    for (int i = 0; i < NUM_LEDS; i++)
    {
        leds[i] = ColorFromPalette(palettes[0], gHue + (i * 2), beat - gHue + (i * 10));
    }
}

// LEDs turn on one at a time at full brightness and slowly fade to black
// Uses colors from a palette of colors
void randomPaletteFades()
{
    if (updatePatternBasedOnSpeedSetting(100) == false)
        return;

    uint16_t i = random16(0, (NUM_LEDS - 1)); // Pick a random LED
    {
        uint8_t colorIndex = random8(0, 255); // Pick a random color (from palette)
        if (CRGB(0, 0, 0) == CRGB(leds[i])) // Only set new color to LED that is off
        {
            leds[i] = ColorFromPalette(palettes[currentPaletteIndex], colorIndex, 255, currentBlending);
            blur1d(leds, NUM_LEDS, 32); // Blur colors with neighboring LEDs
        }
    }
    fadeToBlackBy(leds, NUM_LEDS, 8); // Slowly fade LEDs to black
}

// Theater style chasing lights rotating in one direction while the
// rainbow colors rotate in the opposite direction.
void rainbowChase()
{
    if (updatePatternBasedOnSpeedSetting(200) == false)
        return;

    static int q = 0;
    fill_gradient(leds, (NUM_LEDS - 1), CHSV(gHue, 200, 255), 0, CHSV((gHue + 1), 200, 255), LONGEST_HUES);

    for (int i = 0; (NUM_LEDS - 3) > i; i += 3)
    {
        leds[((i + q) + 1)] = CRGB(0, 0, 0);
        leds[((i + q) + 2)] = CRGB(0, 0, 0);
    }
    if (2 > q) {
        q++;
    } else {
        q = 0;
    }
}

void randomDots() // Similar to randomFades(), colors flash on/off quickly
{
    if (updatePatternBasedOnSpeedSetting(200) == false)
        return;

    uint16_t pos;
    pos = random16(0, (NUM_LEDS - 1));
    if (CRGB(0, 0, 0) == CRGB(leds[pos]))
    {
        leds[pos] = CHSV((random8() % 256), 200, 255);
    }
    fadeToBlackBy(leds, NUM_LEDS, 64);
}

// Same as randomPaletteFades() but with completely random colors
void randomFades()
{
    if (updatePatternBasedOnSpeedSetting(200) == false)
        return;

    uint16_t pos;
    pos = random16(0, (NUM_LEDS - 1));
    if (CRGB(0, 0, 0) == CRGB(leds[pos]))
    {
        leds[pos] = CHSV((random8() % 256), 200, 255);
    }
    fadeToBlackBy(leds, NUM_LEDS, 8);
}

// Same as randomDots() but with red and blue flashes only
void policeLights()
{
    if (updatePatternBasedOnSpeedSetting(200) == false)
        return;

    fadeToBlackBy(leds, NUM_LEDS, 128);
    uint16_t p = random16(0, (NUM_LEDS - 1));
    uint8_t n = (1 & random8());
    if (n)
    {
        leds[p] = CRGB(255, 0, 0);
    }
    else
    {
        leds[p] = CRGB(0, 0, 255);
    }
}

// Same as randomDots() but faster white flashes only
void glitter()
{
    if (updatePatternBasedOnSpeedSetting(200) == false)
        return;

    fadeToBlackBy(leds, NUM_LEDS, 128);
    if (random8() < 225)
    {
        leds[random16(0, (NUM_LEDS - 1))] = CRGB::White;
    }
}

// Twinkling random dim white LEDs mixed with glitter() above
void snowFlakes()
{
    if (updatePatternBasedOnSpeedSetting(200) == false)
        return;

    uint8_t shader;
    for (int x = 0; NUM_LEDS > x; x++)
    {
        shader = random8(20, 30);
        leds[x] = CRGB(shader, shader, shader);
    }
    leds[random16(0, (NUM_LEDS - 1))] = CRGB::White;
}

// Simulates lightning with randomly timed and random size bolts
void lightning()
{
    static timer_struct boltTimer;
    if (previousPatternIndex != currentPatternIndex)
    {
        // slowly fade previous patern to black
        for (uint8_t i = 0; i < 90; i++) {
            fadeToBlackBy( leds, NUM_LEDS, 5);
            LEDS.show();
            delay(2);
        }
        fill_solid(leds, NUM_LEDS, CRGB::Black);
        LEDS.show();
        boltTimer.period = 0;
        boltTimer.mark = millis();
    }
    if (boltTimer.period < (millis() - boltTimer.mark))
    {
        uint16_t boltLength = random16(5, 30);
        uint8_t numStrobes = random8(1, 3);
        uint32_t highPulseTime[numStrobes];
        uint32_t lowPulseTime[numStrobes];
        for (uint8_t i = 0; numStrobes > i; i++)
        {
            highPulseTime[i] = (uint32_t)(random16(60, 250));
            lowPulseTime[i] = (uint32_t)(random16(50, 300));
        }
        uint16_t pos = random16(0, ((NUM_LEDS - 1) - boltLength));
        for (uint8_t i = 0; numStrobes > i; i++)
        {
            for (uint16_t x = pos; (pos + boltLength) > x; x++)
            {
                leds[x] = CRGB(255, 255, 255);
                LEDS.show();
                delay(3);
            }
            delay(highPulseTime[i]);
            if (numStrobes > 1)
            {
                for (uint16_t x = pos; (pos + boltLength) > x; x++)
                {
                    leds[x] = CRGB(127, 127, 127);
                    LEDS.show();
                    delay(3);
                }
                delay(lowPulseTime[i]);
            }
        }
        for (uint16_t x = pos; (pos + boltLength) > x; x++)
        {
            leds[x] = CRGB(0, 0, 0);
        }
        boltTimer.period = (unsigned long)(random16(1500, 5000));
        boltTimer.mark = millis();
    }
}

//######################### Patterns by Resseguie/FastLED-Patterns END #########################


//##################### Desk Lamp
#if LED_DEVICE_TYPE == 3

void pride_Waves()
{
    static uint16_t sPseudotime = 0;
    static uint16_t sLastMillis = 0;
    static uint16_t sHue16 = 0;

    uint8_t sat8 = beatsin88(87, 220, 250);
    uint8_t brightdepth = beatsin88(341, 96, 224);
    uint16_t brightnessthetainc16 = beatsin88(203, (25 * 256), (40 * 256));
    uint8_t msmultiplier = beatsin88(147, 23, 60);

    uint16_t hue16 = sHue16;//gHue * 256;
    uint16_t hueinc16 = beatsin88(113, 1, 3000);

    uint16_t ms = millis();
    uint16_t deltams = ms - sLastMillis;
    sLastMillis = ms;
    sPseudotime += deltams * msmultiplier;
    sHue16 += deltams * beatsin88(400, 5, 9);
    uint16_t brightnesstheta16 = sPseudotime;

    for (uint16_t i = 0; i < LINE_COUNT; i++) {
        hue16 += hueinc16;
        uint8_t hue8 = hue16 / 256;

        brightnesstheta16 += brightnessthetainc16;
        uint16_t b16 = sin16(brightnesstheta16) + 32768;

        uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
        uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
        bri8 += (255 - brightdepth);

        CRGB newcolor = CHSV(hue8, sat8, bri8);

        uint16_t pixelnumber = i;
        pixelnumber = (LINE_COUNT - 1) - pixelnumber;

        for (int l = 0; l < LEDS_PER_LINE; l++)
        {
            nblend(leds[pixelnumber * LEDS_PER_LINE + l], newcolor, 64);
        }
    }
}

void pride_Rings()
{
    static uint16_t sPseudotime = 0;
    static uint16_t sLastMillis = 0;
    static uint16_t sHue16 = 0;

    uint8_t sat8 = beatsin88(87, 220, 250);
    uint8_t brightdepth = beatsin88(341, 96, 224);
    uint16_t brightnessthetainc16 = beatsin88(203, (25 * 256), (40 * 256));
    uint8_t msmultiplier = beatsin88(147, 23, 60);

    uint16_t hue16 = sHue16;//gHue * 256;
    uint16_t hueinc16 = beatsin88(113, 1, 3000);

    uint16_t ms = millis();
    uint16_t deltams = ms - sLastMillis;
    sLastMillis = ms;
    sPseudotime += deltams * msmultiplier;
    sHue16 += deltams * beatsin88(400, 5, 9);
    uint16_t brightnesstheta16 = sPseudotime;

    for (uint16_t i = 0; i < LEDS_PER_LINE; i++) {
        hue16 += hueinc16;
        uint8_t hue8 = hue16 / 256;

        brightnesstheta16 += brightnessthetainc16;
        uint16_t b16 = sin16(brightnesstheta16) + 32768;

        uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
        uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
        bri8 += (255 - brightdepth);

        CRGB newcolor = CHSV(hue8, sat8, bri8);

        uint16_t pixelnumber = i;
        pixelnumber = (LEDS_PER_LINE - 1) - pixelnumber;

        for (int p = 0; p < LINE_COUNT; p++)
        {
            if (p % 2 == 0) nblend(leds[p * LEDS_PER_LINE + pixelnumber], newcolor, 64);
            else nblend(leds[p * LEDS_PER_LINE + (LEDS_PER_LINE - pixelnumber - 1)], newcolor, 64);
        }
    }
}

void ColorSingleRing(int pos, CHSV c)
{
    for (int p = 0; p < LINE_COUNT; p++)
    {
        if (p % 2 == 0) leds[p * LEDS_PER_LINE + pos] = c;
        else leds[p * LEDS_PER_LINE + (LEDS_PER_LINE - pos - 1)] = c;
    }
}

void ColorSingleRing(int pos, CRGB c)
{
    for (int p = 0; p < LINE_COUNT; p++)
    {
        if (p % 2 == 0) leds[p * LEDS_PER_LINE + pos] = c;
        else leds[p * LEDS_PER_LINE + (LEDS_PER_LINE - pos - 1)] = c;
    }
}

void colorWaves_hori()
{
    colorwaves_Lamp(leds, LINE_COUNT, gCurrentPalette, 1);
}

void colorWaves_vert()
{
    colorwaves_Lamp(leds, LEDS_PER_LINE, gCurrentPalette, 0);
}

void colorwaves_Lamp(CRGB* ledarray, uint16_t numleds, CRGBPalette16& palette, uint8_t horizonal)
{
    static uint16_t sPseudotime = 0;
    static uint16_t sLastMillis = 0;
    static uint16_t sHue16 = 0;

    // uint8_t sat8 = beatsin88( 87, 220, 250);
    uint8_t brightdepth = beatsin88(341, 96, 224);
    uint16_t brightnessthetainc16 = beatsin88(203, (25 * 256), (40 * 256));
    uint8_t msmultiplier = beatsin88(147, 23, 60);

    uint16_t hue16 = sHue16;//gHue * 256;
    uint16_t hueinc16 = beatsin88(113, 300, 1500);

    uint16_t ms = millis();
    uint16_t deltams = ms - sLastMillis;
    sLastMillis = ms;
    sPseudotime += deltams * msmultiplier;
    sHue16 += deltams * beatsin88(400, 5, 9);
    uint16_t brightnesstheta16 = sPseudotime;

    for (uint16_t i = 0; i < numleds; i++) {
        hue16 += hueinc16;
        uint8_t hue8 = hue16 / 256;
        uint16_t h16_128 = hue16 >> 7;
        if (h16_128 & 0x100) {
            hue8 = 255 - (h16_128 >> 1);
        }
        else {
            hue8 = h16_128 >> 1;
        }

        brightnesstheta16 += brightnessthetainc16;
        uint16_t b16 = sin16(brightnesstheta16) + 32768;

        uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
        uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
        bri8 += (255 - brightdepth);

        uint8_t index = hue8;
        //index = triwave8( index);
        index = scale8(index, 240);

        CRGB newcolor = ColorFromPalette(palette, index, bri8);

        uint16_t pixelnumber = i;
        pixelnumber = (numleds - 1) - pixelnumber;

        nblend(ledarray[pixelnumber], newcolor, 128);
        if (horizonal != 0)
        {
            for (int l = 0; l < LEDS_PER_LINE; l++)
            {
                nblend(ledarray[pixelnumber * LEDS_PER_LINE + l], newcolor, 128);
            }
        } else {
            for (int p = 0; p < LINE_COUNT; p++)
            {
                if (p % 2 == 0) nblend(leds[p * LEDS_PER_LINE + pixelnumber], newcolor, 128);
                else nblend(leds[p * LEDS_PER_LINE + (LEDS_PER_LINE - pixelnumber - 1)], newcolor, 128);
            }
        }
    }
}

void rainbow_vert()
{
    for (int l = 0; l < LEDS_PER_LINE; l++)
    {
        for (int p = 0; p < LINE_COUNT; p++)
        {
            if (p % 2 == 0) leds[p * LEDS_PER_LINE + l] = CHSV((((255.00 / (LEDS_PER_LINE)) * l) + gHue), 255, 255);
            else leds[p * LEDS_PER_LINE + (LEDS_PER_LINE - l - 1)] = CHSV((((255.00 / (LEDS_PER_LINE)) * l) + gHue), 255, 255);
        }
    }
}


#endif



/*######################## LOGO FUNCTIONS ########################*/

void logo()
{
#ifdef THINGIVERSE
    thingiverse();
#endif
}

void logo_static()
{
#ifdef THINGIVERSE
    thingiverse_static();
#endif
}

#ifdef THINGIVERSE
void thingiverse_static()
{
    if (RINGFIRST)
    {
        fill_solid(leds, RING_LENGTH, STATIC_RING_COLOR);
        fill_solid(leds + RING_LENGTH, HORIZONTAL_LENGTH, STATIC_LOGO_COLOR);
        fill_solid(leds + RING_LENGTH + HORIZONTAL_LENGTH, VERTICAL_LENGTH, STATIC_LOGO_COLOR);
    }
    else
    {
        fill_solid(leds, HORIZONTAL_LENGTH, STATIC_LOGO_COLOR);
        fill_solid(leds + HORIZONTAL_LENGTH, VERTICAL_LENGTH, STATIC_LOGO_COLOR);
        fill_solid(leds + HORIZONTAL_LENGTH + VERTICAL_LENGTH, RING_LENGTH, STATIC_RING_COLOR);
    }
}

void thingiverse()  // twenty one pilots
{
    static uint8_t    numdots = 4; // Number of dots in use.
    static uint8_t   faderate = 4; // How long should the trails be. Very low value = longer trails.
    static uint8_t     hueinc = 255 / numdots - 1; // Incremental change in hue between each dot.
    static uint8_t    thishue = 82; // Starting hue.
    static uint8_t     curhue = 82; // The current hue
    static uint8_t    thissat = 255; // Saturation of the colour.
    static uint8_t thisbright = 255; // How bright should the LED/display be.
    static uint8_t   basebeat = 5; // Higher = faster movement.

    static uint8_t lastSecond = 99;  // Static variable, means it's only defined once. This is our 'debounce' variable.
    uint8_t secondHand = (millis() / 1000) % ANIMATION_RING_DURATION; // IMPORTANT!!! Change '30' to a different value to change duration of the loop.

    if (lastSecond != secondHand) { // Debounce to make sure we're not repeating an assignment.
        lastSecond = secondHand;
        switch (secondHand) {
        case  0: numdots = 1; basebeat = 20; hueinc = 2; faderate = 4; thishue = random(143, 147); break; // You can change values here, one at a time , or altogether.
        case 10: numdots = 4; basebeat = 10; hueinc = 2; faderate = 8; thishue = random(142, 148); break;
        case 20: numdots = 8; basebeat = 3; hueinc = 0; faderate = 8; thishue = random(143, 147); break; // Only gets called once, and not continuously for the next several seconds. Therefore, no rainbows.
        case 30: break;
        }
    }

    // Several colored dots, weaving in and out of sync with each other
    curhue = thishue; // Reset the hue values.
    if (RINGFIRST)
    {
        fadeToBlackBy(leds, RING_LENGTH, faderate);
    }
    else fadeToBlackBy(leds + VERTICAL_LENGTH + HORIZONTAL_LENGTH, VERTICAL_LENGTH + HORIZONTAL_LENGTH + RING_LENGTH, faderate);
    for (int i = 0; i < numdots; i++) {
        if (RINGFIRST)leds[beatsin16(basebeat + i + numdots, 0, RING_LENGTH)] += CHSV(curhue, thissat, thisbright);
        else leds[beatsin16(basebeat + i + numdots, VERTICAL_LENGTH + HORIZONTAL_LENGTH, RING_LENGTH + VERTICAL_LENGTH + HORIZONTAL_LENGTH)] += CHSV(curhue, thissat, thisbright);
        curhue += hueinc;
    }

    // sinelone for the lines
    /*
    fadeToBlackBy(leds + twpOffsets[0], DOUBLE_STRIP_LENGTH + DOT_LENGTH + ITALIC_STRIP_LENGTH, 50);
    int16_t myspeed = 30 + speed * 1.5;
    if (myspeed > 255 || myspeed < 0)myspeed = 255;
    int pos = beatsin16(myspeed, twpOffsets[1], twpOffsets[1] + DOUBLE_STRIP_LENGTH + DOT_LENGTH + ITALIC_STRIP_LENGTH - 1);
    static int prevpos = 0;
    CRGB color = STATIC_LOGO_COLOR;
    if (pos < prevpos) {
      fill_solid(leds + pos, (prevpos - pos) + 1, color);
    }
    else {
      fill_solid(leds + prevpos, (pos - prevpos) + 1, color);
    }
    prevpos = pos;
    */
    //uint8_t b = beatsin8(10, 200, 255);

    uint8_t pos = 0;
    uint8_t spd = 100;
    uint8_t b = 255;
    bool even = true;
    if ((HORIZONTAL_LENGTH / 2.00) > (int)(HORIZONTAL_LENGTH / 2.00))even = false;

    if (!even)
    {
        // FIXME: This is BROKEN, beatsaw8 takes 5 arguments, 3 given here
        //pos = beatsin8(spd, 0, VERTICAL_LENGTH + (HORIZONTAL_LENGTH - 1) / 2);
        pos = beatsaw8(spd, 0, VERTICAL_LENGTH + (HORIZONTAL_LENGTH - 1) / 2);
        b = beatsaw8(spd * 2, 255 / 2, 255);
    }
    else
    {
        //pos = beatsin8(spd, 0, VERTICAL_LENGTH + (HORIZONTAL_LENGTH - 2) / 2);
    }
    if (!even)
    {
        if (pos < VERTICAL_LENGTH)
        {
            if (HORIZONTAL_BEFORE_VERTICAL)
            {
                if (!RINGFIRST) leds[HORIZONTAL_LENGTH + VERTICAL_LENGTH - pos - 1] = CHSV(145, 255, b);
                else { leds[HORIZONTAL_LENGTH + VERTICAL_LENGTH - pos - 1 + RING_LENGTH] = CHSV(145, 255, b); }
            }
            else
            {
                if (!RINGFIRST) leds[VERTICAL_LENGTH - pos - 1] = CHSV(145, 255, b);
                else { leds[VERTICAL_LENGTH - pos - 1 + RING_LENGTH] = CHSV(145, 255, b); }
            }
        }
        else if (pos == VERTICAL_LENGTH)
        {
            if (!RINGFIRST)
            {
                leds[(HORIZONTAL_LENGTH / 2)] = CHSV(145, 255, b);
            }
            else
            {
                leds[(HORIZONTAL_LENGTH / 2) + RING_LENGTH] = CHSV(145, 255, b);
            }
        }
        else
        {
            if (HORIZONTAL_BEFORE_VERTICAL)
            {
                if (!RINGFIRST)
                {
                    leds[HORIZONTAL_LENGTH - pos] = CHSV(145, 255, b);
                    leds[pos - 1] = CHSV(145, 255, b);
                }
                else
                {
                    leds[HORIZONTAL_LENGTH - pos + RING_LENGTH] = CHSV(145, 255, b);
                    leds[pos - 1 + RING_LENGTH] = CHSV(145, 255, b);
                }
            }
            else
            {
                if (!RINGFIRST)
                {
                    leds[HORIZONTAL_LENGTH - pos + VERTICAL_LENGTH] = CHSV(145, 255, b);
                    leds[pos - 1 + VERTICAL_LENGTH] = CHSV(145, 255, b);
                }
                else
                {
                    leds[HORIZONTAL_LENGTH - pos + RING_LENGTH + VERTICAL_LENGTH] = CHSV(145, 255, b);
                    leds[pos - 1 + RING_LENGTH + VERTICAL_LENGTH] = CHSV(145, 255, b);
                }
            }
        }
    }
    if (!RINGFIRST)
    {
        //fadeToBlackBy(leds, HORIZONTAL_LENGTH + VERTICAL_LENGTH, 50);
        fadeLightBy(leds, HORIZONTAL_LENGTH + VERTICAL_LENGTH, 5);
    }
    else
    {
        fadeLightBy(leds + RING_LENGTH, HORIZONTAL_LENGTH + VERTICAL_LENGTH, 5);
    }

    /*
    uint8_t b = 255;
    uint8_t pos = 0;
    if (RINGFIRST)
    {
      pos = beatsin8(30, RING_LENGTH, RING_LENGTH + VERTICAL_LENGTH + HORIZONTAL_LENGTH);
      fadeToBlackBy(leds + RING_LENGTH, RING_LENGTH+VERTICAL_LENGTH + HORIZONTAL_LENGTH, 10);

    }
    else
    {
      pos = beatsin8(30, 0, VERTICAL_LENGTH + HORIZONTAL_LENGTH);
      fadeToBlackBy(leds, VERTICAL_LENGTH + HORIZONTAL_LENGTH, 10);

    }
    //if (pos == 0 && RINGFIRST == false)fadeToBlackBy(leds, 1, 50);
    //else if(pos == RING_LENGTH && RINGFIRST == true)fadeToBlackBy(leds+RING_LENGTH, 1, 50);
    leds[pos] = CHSV(145, 255, b);
    */
}
#endif THINGIVERSE
