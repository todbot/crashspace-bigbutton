# crashspace-bigbutton

Firmware and schematics for the [CrashSpace](https://blog.crashspace.org/)
"BigButton" hardware interface to https://crashspacela.com/sign/

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

## In use

  * Button is pulsing ORANGE: CrashSpace is closed
  * Push Button.  Button makes a RAINBOW for a second or two
  * Button is pulsing TEAL: CrashSpace is open
  * Button counts down my extinguishing TEAL LEDs in an arc
  * Button flashing RED: Internet is down

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
The Wemos D1 mini ESP8266 board does most of the work.
The I/O is a single button and a string of WS2812 LEDs.

The schematic and layout for the carrier board is pretty simple.
<img src="./docs/CrashButtonESPD1-sch.png" width=650>
<img src="./docs/CrashButtonESPD1-brd.png" width=650>


### Converting 3v3 ESP8266 output for WS2812 / Neopixel  LEDs

One interesting thing about the schematic how the ESP8266 (a 3v3 device)
manages to control 5V WS2812/Neopixel LEDs.  Some WS2812s can be driven by 3v3 logic HIGH, but it's iffy.  The standard solution is a level-shifter buffer to convert 3v3 HIGH to 5V HIGH.

The technique used on this board, however, is to create a "sacrificial" LED that powered not by 5V but by a stepped-down voltage from a standard 1.2V diode.  This approx. 3.8V power source is high enough to drive the LED but brings its concept of logic HIGH (which is 70% of Vcc) down to what a 3v3 device will output.  Basically, we're special intermediary power supply for a single LED. The rest of the WS2812 LEDs are driven by 5V.

#### Othermill design considerations

I wanted this board to be millable on an Othermill, so the

<img src="./docs/bigbutton-front-back.jpg">




## Implementation - Firmware

This board runs the ["CrashButtonESP"](./CrashButtonESP/) Arduino sketch.

talk about json feed

talk about ArduinoJson

### CrashSpace sign server JSON API

To enable the ability of the button to report real open status
(and not just a dumb countdown), the sign server API has been updated to have a JSON output mode.  There are two versions of JSON output:
a "minimal" version that just gives open status and minutes left,
and a full version that gives a list of recent button presses.
The Button uses the minimal one.  Both are shown below:

```json
% curl 'crashspacela.com/sign/?output=jsonmin'
{"is_open":false,"minutes_left":-920.45,"button_presses":[]}
```

```json
% curl 'crashspacela.com/sign/?output=json'
{
    "is_open": false,
    "minutes_left": -919.43333333333,
    "button_presses": [
        {
            "type": "update",
            "id": "crashbutton1",
            "msg": "Someone is at the space!",
            "diff_mins_max": 60,
            "description": "crashbutton1 - update - 60 mins.",
            "date": "Wed, 11 Jan 2017 21:38:03 -0800",
            "date_epoch": "1484199483"
        },
        {
            "type": "update",
            "id": "crashbutton1",
            "msg": "Someone is at the space!",
            "diff_mins_max": 60,
            "description": "crashbutton1 - update - 60 mins.",
            "date": "Wed, 11 Jan 2017 20:00:00 -0800",
            "date_epoch": "1484193600"
        },
        {
            "type": "update",
            "id": "crashbutton1",
            "msg": "Someone is at the space!",
            "diff_mins_max": 60,
            "description": "crashbutton1 - update - 60 mins.",
            "date": "Wed, 11 Jan 2017 19:16:26 -0800",
            "date_epoch": "1484190986"
        }
    ]
}
```
