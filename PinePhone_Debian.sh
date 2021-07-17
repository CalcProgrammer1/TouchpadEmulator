#! /bin/sh

sudo modprobe uinput
sudo chmod 777 /dev/uinput

gcc TouchpadEmulator.c -o TouchpadEmulator

./TouchpadEmulator /dev/input/event2 /dev/input/event1
