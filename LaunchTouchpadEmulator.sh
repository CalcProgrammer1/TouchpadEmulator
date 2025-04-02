#! /bin/bash

if [ "$1" = "--autostart" ] ; then
    mkdir -p ~/.config/autostart

    AUTOSTART=""
    AUTOSTART+="[Desktop Entry]\r\n"
    AUTOSTART+="Type=Application\r\n"
    AUTOSTART+="Name=Touchpad Emulator Autostart\r\n"
    AUTOSTART+="Exec=sh -c \"LaunchTouchpadEmulator.sh --start-disabled --force-autorotation\"\r\n"
    AUTOSTART+="Icon=TouchpadEmulator\r\n"

    echo -e $AUTOSTART > ~/.config/autostart/TouchpadEmulator-Autostart.desktop

    echo "Autostart enabled, TouchpadEmulator will start automatically on login."
    exit
fi

# Kill all existing instances of TouchpadEmulator before starting a new one
killall TouchpadEmulator

# If access is not already available, load the uinput module and enable user access
# to input devices and uinput.  This requires root permission, so use pkexec to show
# a graphical root prompt.
if ( ! [ -e /dev/uinput ] ) || \
   ( ! [ -w /dev/uinput ] ) || \
   ( ! [ -r /dev/input/event0 ] ) then
    pkexec bash -c "chmod 666 /dev/input/*; modprobe uinput; chmod 777 /dev/uinput"
fi

# Start TouchpadEmulator
TouchpadEmulator "$@"
