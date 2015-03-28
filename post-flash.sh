#!/bin/bash

bus="/sys/bus/i2c/devices/i2c-0"

if [[ $EUID -eq 0 ]]
then
    echo sbs-battery 0x0b > $bus/new_device
else
    echo Must be run as root. 1>&2
    exit 1
fi

