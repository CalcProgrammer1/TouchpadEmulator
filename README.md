# Touchpad Emulator

Emulates a laptop-style touchpad device using a touchscreen. My goal with this is to provide a virtual mouse for Linux phones and tablets that functions similarly to the virtual mouse in Microsoft's Remote Desktop Android app.

This application has mostly been tested in the Phosh environment on the **PinePhone** and **PinePhone Pro**. It should support all distributions using Phosh.


**Input Events:**

* Touchpad Emulator now uses input event names to automatically determine the event IDs.  The event names are currently hard-coded to work with the following devices, but I plan to move this to a config file soon.

| Device               | Touchscreen Event               | Buttons Event   | Slider Event   |
| -------------------- | ------------------------------- | --------------- | -------------- |
| PINE64 PinePhone     | "Goodix Capacitive TouchScreen" | "1c21800.lradc" | N/A            |
| PINE64 PinePhone Pro | "Goodix Capacitive TouchScreen" | "adc-keys"      | N/A            |
| OnePlus 6T           | "Synaptics S3706B"              | "Volume keys"   | "Alert slider" |

## Installation

Touchpad Emulator is available via AUR: [touchpad-emulator-git](https://aur.archlinux.org/packages/touchpad-emulator-git)

Or you can build it yourself:

### Building

#### Dependencies

* Alpine/PostmarketOS - `sudo apk add build-base linux-headers dbus-glib-dev`
* Debian/Mobian - `sudo apt install build-essential linux-headers-arm64 libdbus-glib-1-dev pkexec`

#### Compile & Install

```
make
sudo make install
```

On Debian/Mobian you can build a .deb package using `dpkg-buildpackage -b`

Uninstall with `sudo make uninstall`

#### Running

* `sh LaunchTouchpadEmulator.sh`


## Controls

* TouchPad Emulator uses either the volume keys or the alert slider to switch modes.

  * Volume keys - Hold for more than 500ms to trigger mode change.  Quick tap to adjust volume.
    * Volume Up enables Touchpad Mouse mode
    * Volume Down enables Touchscreen mode
      * If already in Touchscreen mode, Volume Down toggles on-screen keyboard on and off
    * Holding either Volume button for 3 seconds closes the program

  * Alert slider
    * Up position: Touchpad Mouse mode
    * Center position: Touchscreen mode, on-screen keyboard disabled
    * Down position: Touchscreen mode, on-screen keyboard enabled

* In Touchpad Mouse mode:
    * Moving one finger on touchscreen emulates mouse movement
    * Tapping one finger on touchscreen emulates mouse left click
    * Tap-and-hold one finger on touchscreen emulates mouse left click and drag
    * Holding one finger and tapping a second finger on touchscreen emulates mouse right click
    * Moving two fingers on touchscreen emulates scroll wheel (vertical axis only)
