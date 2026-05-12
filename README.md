# AlarmClockRestoMod
ESP32 Software for an Alarm Clock Resto Mod.

The idea is to use the components of an old alarm clock and replace the electronics with an ESP32, adding a lot of features to it. From an outside perspective, the clock will look as original as possible.

## Features

* WiFi connectivity
  * NTP time synchronization
  * Web server for advanced configuration
  * OTA updates
* Bluetooth connection from phone and to headphone/speaker
* Clock:
  * 7 segment display with dimming function
  * RTC for time keeping when power is out
* Alarms:
  * Configurable via web interface (advanced) or via buttons (only next alarm)
  * Configurable music or internet radio or buzzer for each alarm
  * Snooze function (delay configurable via web interface)
  * Vacation mode (turn off all alarms for a configurable period of time)
* Radio:
  * Internet radio stations configurable via web interface
  * Tuner button to switch between internet radio stations or playlists on microSD card
* microSD Card to store pre-owned Music, playlists and configuration files


## Components Overview
* Display is a 7 segment 4 digits with a TM1637, dimmable by software
* DS3231 for time keeping when power is out
* microSD Card with:
  * MP3 songs and playlists
  * Configuration file for radio stations (accepts internet radio station or playlist)
  * Configuration file for different alarms
* The tuner button of the alarm clock is a variable capacitor (10 - 500pF) read via a MPR121
  * Each frequency corresponds to an internet radio
* Bluetooth to connect to cellphone (play music from it) or to an external speaker/headphone
* Normal speaker connected via MAX98357A
* Buttons (Telefunken Digital Electronic 101):
  * 2 momentary switches to set time (Fast and slow)
  * 1 latching switch to turn on auto dimm/full light on display
  * 3 mutually-exclusive latching switches: show time, show next alarm (use the 2 momentary switches to set the alarm time) or "sleep mode" (turn on radio for 60min)
  * 1 momentary switch for sleep function (during alarm, press once to delay alarm in 5min, press twice to turn off alarm; during sleep mode, turn off radio)
  * 1 Volume slider (potentiometer)
  * 4 mutually-exclusive latching switches: turn on radio, turn off radio, alarm with radio, alarm with buzzing sound
  * 2 mutually-exclusive latching switches: radio corresponds to playlists, radio corresponds to internet radios
  * 1 momentary switch: one click (tries to connect to configured bluetooth speaker), two clicks (turns on bluetooth for phone to connect); if bluetooth connection is not successful after 1 min, turn off bluetooth
* Alarms:
  * fully configurable via web interface
  * choose days of the week
  * snooze time configurable
  * vacation mode
  * music and internet radio configured via web interface
