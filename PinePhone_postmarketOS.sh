#! /bin/sh

sudo modprobe uinput
sudo chmod 777 /dev/uinput

gcc TouchpadEmulator.c -o TouchpadEmulator

./TouchpadEmulator /dev/input/event1 /dev/input/event4
