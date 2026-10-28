#!/bin/bash

echo 0 > /sys/class/CPLD1/cpld_device/qsfp_en_led || true
echo 0 > /sys/class/CPLD1/cpld_device/osfp_en_led_0 || true
echo 0 > /sys/class/CPLD1/cpld_device/osfp_en_led_1 || true
echo 0 > /sys/class/CPLD1/cpld_device/osfp_en_led_2 || true
echo 0 > /sys/class/CPLD1/cpld_device/osfp_en_led_3 || true
echo 0 > /sys/class/CPLD1/cpld_device/osfp_en_led_4 || true
echo 0 > /sys/class/CPLD1/cpld_device/osfp_en_led_5 || true
echo 0 > /sys/class/CPLD1/cpld_device/osfp_en_led_6 || true
echo 0 > /sys/class/CPLD1/cpld_device/osfp_en_led_7 || true

iface="1-2.2:2.0"
[ -d /sys/bus/usb/drivers/rndis_host ] || modprobe rndis_host
( [ -e "/sys/bus/usb/drivers/rndis_host/$iface" ] || \
  echo -n "$iface" > /sys/bus/usb/drivers/rndis_host/bind )
