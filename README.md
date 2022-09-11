# TwistedLampSensor

Project files :
- twisted_lamp_sensor_app => Flutter App to control the lamp
- testCabling => fixed led patter + BLE server with all sensors
- TwistedLampCo2TempBT => inspiration for led animation
- [Original project + files for 3d printer](https://www.thingiverse.com/thing:4129249)


Components :
- USB C breakout board with 5K1 resistor on CC1/CC2 to appear as a device
- ESP32 Lolin32 lite
- 2.5A USB C charger (3/4 A would be better)
- 80 WS2812 leds (GPIO 18)
- DHT22 temperature and humidity sensor (GPIO 23)
- [Adafruit CCS811 Air Quality Sensor VOC and eCO2](https://www.adafruit.com/product/3566) (SDA on GPIO 19 and SCL on GPIO 22)
