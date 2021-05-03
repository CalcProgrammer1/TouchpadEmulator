Touchpad Emulator

Emulates a laptop-style touchpad device using a touchscreen.  My goal with this is to provide a virtual mouse for Linux phones and tablets that functions similarly to the virtual mouse in Microsoft's Remote Desktop Android app.

Dependencies:

* Alpine/PostmarketOS - `sudo apk add build-base linux-headers`

* Debian/Mobian - `sudo apt install build-essential linux-headers`

Compiling:

* `gcc TouchpadEmulator.c -o TouchpadEmulator`

Running:

* `sudo modprobe uinput`
* `sudo chmod 777 /dev/uinput`
    * Could probably set up a udev rule here
* `./TouchpadEmulator /dev/input/eventX /dev/input/eventY`
    * `/dev/input/eventX` - Replace with input event for touchscreen
    * `/dev/input/eventY` - Replace with input event for GPIO keys (volume up/down)

Controls:

* Volume Up enables Touchpad Mouse mode
    * If already in Touchpad Mouse mode, Volume Up cycles through rotations - 0, 90, 180, 270 and back to 0

* Volume Down enables Touchscreen mode
    * If already in Touchscreen mode, in the keyboard_toggle branch, Volume Down toggles on-screen keyboard on and off

* Holding either Volume button for 3 seconds closes the program

* In Touchpad Mouse mode:
    * Moving one finger on touchscreen emulates mouse movement
    * Tapping one finger on touchscreen emulates mouse left click
    * Tap-and-hold one finger on touchscreen emulates mouse left click and drag
    * Holding one finger and tapping a second finger on touchscreen emulates mouse right click
    * Moving two fingers on touchscreen emulates scroll wheel (vertical axis only)