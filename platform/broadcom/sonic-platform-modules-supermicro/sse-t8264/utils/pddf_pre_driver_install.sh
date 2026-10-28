#!/bin/bash

modprobe i2c-i801
modprobe i2c-ismt bus_speed=400 delay=100
