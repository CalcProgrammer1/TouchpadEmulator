#! /bin/sh

sudo chmod 666 /dev/input/*
sudo modprobe uinput
sudo chmod 777 /dev/uinput

./compile.sh

screen ./TouchpadEmulator 2 4
