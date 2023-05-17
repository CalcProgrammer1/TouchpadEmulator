# Touchpad Emulator

Emulates a laptop-style touchpad device using a touchscreen. My goal with this is to provide a virtual mouse for Linux phones and tablets that functions similarly to the virtual mouse in Microsoft's Remote Desktop Android app.

This application has mostly been tested in the Phosh environment on the **PinePhone**. It should support all distributions using Phosh.


**Input Events:**

* Touchpad Emulator now uses input event names to automatically determine the event IDs.  The event names are currently hard-coded to be PinePhone-specific, but I plan to move this to a config file soon.
  * Touchscreen: "Goodix Capacitive TouchScreen"
  * Buttons: "1c21800.lradc"


## Installation

Touchpad Emulator is available via AUR: [touchpad-emulator-git](https://aur.archlinux.org/packages/touchpad-emulator-git)

Or you can build it yourself:

### Building

#### Dependencies

* Alpine/PostmarketOS - `sudo apk add build-base linux-headers dbus-glib-dev`
* Debian/Mobian - `sudo apt install build-essential linux-headers`

#### Compile & Install

```
make
sudo make install
```

Uninstall with `sudo make uninstall`

#### Running

* `sh LaunchTouchpadEmulator.sh`


## Controls

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
