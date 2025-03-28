#! /bin/sh

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
