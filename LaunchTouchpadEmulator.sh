#! /bin/sh

pkexec bash -c "chmod 666 /dev/input/*; modprobe uinput; chmod 777 /dev/uinput"

TouchpadEmulator
