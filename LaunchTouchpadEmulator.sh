#! /bin/sh

# Kill all existing instances of TouchpadEmulator before starting a new one
killall TouchpadEmulator

# Load the uinput module and enable user access to input devices and uinput
# This requires root permission, so use pkexec to show a graphical root prompt
pkexec bash -c "chmod 666 /dev/input/*; modprobe uinput; chmod 777 /dev/uinput"

# Start TouchpadEmulator
TouchpadEmulator
