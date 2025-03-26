# Touchpad Emulator

Emulates a laptop-style touchpad device using a touchscreen. My goal with this is to provide a virtual mouse for Linux phones and tablets that functions similarly to the virtual mouse in Microsoft's Remote Desktop Android app.

This application has been tested on all of the devices in the table below using postmarketOS. It should support all distributions using Phosh or other GNOME-based environments.  Mouse functionality should work with other environments but the on-screen keyboard toggling feature may not work.


**Input Events:**

* Touchpad Emulator uses input event names to automatically determine the event IDs for the tested and verified devices in the table below:

| Device               | Touchscreen Event               | Buttons Event(s)            | Slider Event   |
| -------------------- | ------------------------------- | --------------------------- | -------------- |
| PINE64 PinePhone     | "Goodix Capacitive TouchScreen" | "1c21800.lradc"             | N/A            |
| PINE64 PinePhone Pro | "Goodix Capacitive TouchScreen" | "adc-keys"                  | N/A            |
| OnePlus 6T           | "Synaptics S3706B"              | "Volume keys"               | "Alert slider" |
| Xiaomi Pad 5 Pro     | "NVTCapacitiveTouchScreen"      | "gpio-keys", "pm8941_resin" | N/A            |
| Google Pixel 3a      | "Synaptics S3706B"              | "gpio-keys", "pm8941_resin" | N/A            |
| Xiaomi Poco F1       | "nvt-ts"                        | "gpio-keys", "pm8941_resin" | N/A            |
| LG Google Nexus 5    | "Synaptics PLG218"              | "gpio-keys"                 | N/A            |

*  Otherwise, if your device's input event names are not known, it will try a fallback detection scheme using input event capabilities instead.

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

#### Permissions

If installed from source, running LaunchTouchpadEmulator.sh will prompt for your password.  If installed via the Alpine package, the neccesary devices will be available to the `input` group.

You can add your user account (shown here as `user`) to the `input` group by running:

* `sudo addgroup user input`

After a reboot, you should no longer be prompted for password when running LaunchTouchpadEmulator.sh.

#### Autostart

After setting up permissions, you can enable Touchpad Emulator to automatically start when you log in.

* `cp /usr/share/applications/TouchpadEmulator.desktop ~/.config/autostart`

## Controls

* TouchPad Emulator uses either the volume keys or the alert slider to switch modes.

  * Volume keys
    * Volume Up
      * Quick tap (<500ms) to increase volume
      * Short hold (500ms) to change mode to Touchpad Mouse mode
      * Long hold (3s) to change touchscreen orientation (only enabled if automatic orientation detection failed)
    * Volume Down
      * Quick tap (<500ms) to decrease volume
      * Short hold (500ms) to change mode to Touchscreen mode
        * If already in Touchscreen mode, toggles on-screen keyboard on and off
      * Long hold (3s) closes the program

  * Alert slider
    * Up position: Touchpad Mouse mode
    * Center position: Touchscreen mode, on-screen keyboard disabled
    * Down position: Touchscreen mode, on-screen keyboard enabled

* In Touchpad Mouse mode:
    * Moving one finger on touchscreen emulates mouse movement
    * Tapping one finger on touchscreen emulates mouse left click
    * Double tap and hold one finger on touchscreen emulates mouse left click and drag
    * Single tap and hold one finger on touchscreen without moving cursor for 1 second emulates mouse drag
    * Holding one finger and tapping a second finger on touchscreen emulates mouse right click
    * Moving two fingers on touchscreen emulates scroll wheel (vertical axis only)
