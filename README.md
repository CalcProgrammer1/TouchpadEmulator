Touchpad Emulator

Emulates a laptop-style touchpad device using a touchscreen.  My goal with this is to provide a virtual mouse for Linux phones and tablets that functions similarly to the virtual mouse in Microsoft's Remote Desktop Android app.

This application has mostly been tested in the Phosh environment on the PinePhone.  It should support all distributions using Phosh.

Dependencies:

* Alpine/PostmarketOS - `sudo apk add build-base linux-headers`

* Debian/Mobian - `sudo apt install build-essential linux-headers`

Compiling:

* `make`

Input Events:

* Touchpad Emulator now uses input event names to automatically determine the event IDs.  The event names are currently hard-coded to be PinePhone-specific, but I plan to move this to a config file soon.
  * Touchscreen: "Goodix Capacitive TouchScreen"
  * Buttons: "1c21800.lradc"

Running:

* `sh LaunchTouchpadEmulator.sh`

Installing:

* `sudo make install`

Uninstalling:

* `sudo make uninstall`

Controls:

* Volume Up enables Touchpad Mouse mode

* Volume Down enables Touchscreen mode
    * If already in Touchscreen mode, Volume Down toggles on-screen keyboard on and off

* Holding either Volume button for 3 seconds closes the program

* In Touchpad Mouse mode:
    * Moving one finger on touchscreen emulates mouse movement
    * Tapping one finger on touchscreen emulates mouse left click
    * Tap-and-hold one finger on touchscreen emulates mouse left click and drag
    * Holding one finger and tapping a second finger on touchscreen emulates mouse right click
    * Moving two fingers on touchscreen emulates scroll wheel (vertical axis only)