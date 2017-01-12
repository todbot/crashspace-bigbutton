# crashspace-bigbutton

Firmware and schematics for the [CrashSpace](https://blog.crashspace.org/)
"BigButton" interface to https://crashspacela.com/sign/

<img src="./docs/bigbutton-header.jpg">

The BigButton is a simple internet-connected button that allows
[CrashSpace](https://blog.crashspace.org/) members to notify others that
the space is open via a publicly-viewable website:
https://crashspacela.com/sign/

Pushing the button indicates you promise to be at the space for at least
an hour.  The button & the site both count down to zero.

## Functionality

* The BigButton consists of a pushbutton and a ring of LEDs
* The user pushes the button to signify they're at the space
* The LED ring displays whether or not CrashSpace is "open" or "closed"
* The LED ring visually shows a count down from 60 minutes, 
  giving a rough indication of time left
* Pushing the button sends an HTTP request to the CrashSpace "sign" server
   at https://crashspacela.com/sign/
* (new!) The button periodically fetches a JSON feed from the
  sign server, getting time left and whether space is open or not.
  This allows for multiple buttons.

## Implementation

The original implementation was an Arduino Uno with WS2812 LEDs inside a
large taplight hooked to a Linux netbook for WiFi connectivity.
An upgrade button created several years late used an early version of the
Particle Photon Internet Button kit in a custom 3d printed enclosure.

This version uses an ESP82166 Wemos D1 mini WiFi board mounted to a
custom carrier board containing 12 WS2812 LEDs.  It uses the ESP8266
Arduino core to run an Arduino sketch.


## Implementation - Hardware

The hardware implementation is simple.
The Wemos D1 mini ESP8266 board does most of the work

<img src="./docs/bigbutton-front-back.jpg">

talk about diode level shifter trick

talk about othermill

The schematic and layout for the carrier board is pretty simple.
<img src="./docs/CrashButtonESPD1-sch.png" width=650>
<img src="./docs/CrashButtonESPD1-brd.png" width=650>



## Implementation - Firmware

This board runs the ["CrashButtonESP"](./CrashButtonESP/) Arduino sketch.

talk about json feed

talk about ArduinoJson
