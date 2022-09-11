/*
   ESP8266 + FastLED + IR Remote: https://github.com/NimmLor/esp8266-fastled-iot-webserver
   Copyright (C) 2021 Ricardo Bartels

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

// define EEPROM settings
// https://www.kriwanek.de/index.php/de/homeautomation/esp8266/364-eeprom-für-parameter-verwenden

#define CONFIG_SAVE_MAX_DELAY 10            // delay in seconds when the settings are saved after last change occured
#define CONFIG_COMMIT_DELAY   200           // commit delay in ms

typedef struct {
    uint8_t brightness;
    uint8_t currentPatternIndex;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t power;
    uint8_t autoplay;
    uint8_t autoplayDuration;
    uint8_t currentPaletteIndex;
    uint8_t speed;
    char hostname[33];
    uint8_t MQTTEnabled;
    char MQTTHost[65];
    uint16_t MQTTPort;
    char MQTTUser[33];
    char MQTTPass[65];
    char MQTTTopic[65];
    char MQTTSetTopic[65];
    char MQTTDeviceName[33];
} configData_t;

configData_t cfg;
configData_t default_cfg;

// save last "timestamp" the config has been saved
unsigned long last_config_change = 0;

void saveConfig(bool force = false) {

    if (last_config_change == 0 && force == false) {
        return;
    }

    static bool write_config = false;
    static bool write_config_done = false;
    static bool commit_config = false;

    if (force == true) {
        write_config = true;
        commit_config = true;
    }

    if (last_config_change > 0) {

        if (last_config_change + (CONFIG_SAVE_MAX_DELAY * 1000) < millis()) {

            // timer expired and config has not been written
            if (write_config_done == false) {
                write_config = true;

            // config has been written but we should wait 200ms to commit
            } else if (last_config_change + (CONFIG_SAVE_MAX_DELAY * 1000) + CONFIG_COMMIT_DELAY < millis()) {
                commit_config = true;
            }
        }
    }

    // Save configuration from RAM into EEPROM
    if (write_config == true) {
        SERIAL_DEBUG_LN(F("Saving Config"))
        EEPROM.begin(4095);
        EEPROM.put(0, cfg );
        write_config_done = true;
        write_config = false;
    }

    if (commit_config == true) {
        if (force == true) delay(CONFIG_COMMIT_DELAY);
        SERIAL_DEBUG_LN(F("Comitting config"))
        EEPROM.commit();
        EEPROM.end();

        // reset all triggers
        last_config_change = 0;
        write_config = false;
        write_config_done = false;
        commit_config = false;
    }
}

// trigger a config write/commit
void setConfigChanged() {
    // start timer
    last_config_change = millis();
}

// overwrite all config settings with "0"
void resetConfig() {

    // delete EEPROM config
    EEPROM.begin(4095);
    for (unsigned int i = 0 ; i < sizeof(cfg) ; i++) {
        EEPROM.write(i, 0);
    }
    delay(CONFIG_COMMIT_DELAY);
    EEPROM.commit();
    EEPROM.end();

    // set to default config
    cfg = default_cfg;
    saveConfig(true);
}
// EOF
