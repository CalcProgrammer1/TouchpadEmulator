#! /bin/sh

sudo modprobe uinput
sudo chmod 777 /dev/uinput

gcc TouchpadEmulator.c -o TouchpadEmulator

./TouchpadEmulator 2 1
