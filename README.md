Weerstation Bram
===============

Contents
----------------------

1. [ Introduction ](#intro)
2. [ Used components ](#components)
3. [ Diagram ](#diagram)
4. [ Code ](#code)


<a name="intro"></a>
## 1. Introduction

This repository provides an overview of a weatherstation build which is currently ongoing. The weatherstation is already reporting to a dashboard at (https://weerstationbram.nl)[https://weerstationbram.nl] but is still needing further upgrades. Upcoming changes:

- Adding tipping bucket by adding the radio transceiver chip
- More stable reading of the voltage divider by chinging the resistors

![Work in progress](./resources/readme_img/WorkInProgress.jpeg)

<a name="components"></a>
## 2. Used components

- [LilyGO TTGO T7 Mini32 V1.4 ESP32-WROVER](https://github.com/LilyGO/TTGO-T7-Demo) € 12.00
- [DHT-22 (AM2302)](https://www.adafruit.com/product/385) € 10.00
- [BMP280 Barometric Pressure + Temperature Sensor](hhttps://www.adafruit.com/product/2651) € 10.00
- [Tipping bucket from another station](https://www.handleidi.ng/auriol/ian-365824/handleiding)
- [Lithium Ion Polymer Battery - 3.7v 2500mAh](https://www.adafruit.com/product/328) € 14.95
- [Adafruit Universal USB / DC / Solar Lithium Ion/Polymer charger (bq24074)](https://www.adafruit.com/product/4755) € 14.95
- [Solar panel 6V 500mA 145x145mm](https://www.aliexpress.com/item/32877897718.html) € 5.50
- [PowerBoost 500 Basic - 5V USB Boost @ 500mA from 1.8V+](https://www.adafruit.com/product/1903) € 9.95
- [10K Precision Epoxy Thermistor (3950 NTC)](https://www.adafruit.com/product/372) € 4.00
- [2.1mm plug to screw terminal block](https://www.adafruit.com/product/369) € 2.00
- [JST-PH 2-pin Jumper Cable](https://www.adafruit.com/product/1131) € 2.00

- 10k Ohm pullup resistor for the DHT-22
- 4 x 100k Ohm resistors for a voltage divider to scale the charger board voltage down to 3.3V for GPIO

- Perfboard
- Solid core wire (red, black, yellow), nuts, bolts, spacers

- USB cable: USB A to micro USB
- Heat shrink wrap, hot glue, soldering equipment

Components for the enclosure:
- Weatherproof box
- Piece of acrylic sheet
- 2 mounting hooks

<a name="diagram"></a>
## 3. Diagram

TODO

<a name="code"></a>

The code is ran using the PlatformIO extension in VSCode. See `src/main.cpp` for the microcontroller logic. 

