# Smart Thermostat 

COMP444 Final Project - ESP32 Smart Thermostat

## Project Overview

- WT32-SC01 ESP32 development module
  - Base station
  - Attached DHT22 for temp/humidity
  - Controls signal to home furnace
  - Programmable heating schedule
  - Manual override from screen
  - Receive temperatures from room modules
  - Connect to HA (Home Assistant) using the ESP Home integration
- ESP8266/ESP32 Modules
  - Records temp/humidity in each room
  - Sends metrics to HA or Base

## Required Setup
- Clone sowbug/Adafruit_FT6206_Library to ArduinoIDE libraries
- Modify TFT_eSPI/User_Setup_Select.h to point to the WT32-SC01 board