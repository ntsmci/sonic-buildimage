#!/bin/bash

# import sonic env
[ -f /etc/sonic/sonic-environment ] && . /etc/sonic/sonic-environment

TRUE=0
FALSE=1

# CPLD 0x36 offset 0xC4 bit[0]: 1 = AC, 0 = DC
PSU_TYPE_DC=0
PSU_TYPE_AC=1

PLATFORM=${PLATFORM:-x86_64-supermicro_sse_t8264-r0}
PDDF_DEV_PATH="/usr/share/sonic/device/$PLATFORM/pddf"
PDDF_DEV_FILE="/usr/share/sonic/device/$PLATFORM/pddf/pddf-device.json"

I2C_BUS=0
CPLD_ADDR=0x36
CPLD_REG=0xC4

function _check_filepath {
    filepath=$1
    if [ -z "${filepath}" ]; then
        echo "[ERR] the input string is empty!!!"
        return ${FALSE}
    elif [ ! -f "$filepath" ] && [ ! -c "$filepath" ]; then
        echo "[ERR] No such file: ${filepath}"
        return ${FALSE}
    else
        return ${TRUE}
    fi
}

function _link_pddf_device {
    src="$1"
    if _check_filepath "$src"; then
        ln -rsf "$src" "$PDDF_DEV_FILE"
        echo "pddf-device.json -> $(basename "$src")"
    fi
}

REG=$(i2cget -f -y "$I2C_BUS" "$CPLD_ADDR" "$CPLD_REG" 2>/dev/null)
if [ -z "$REG" ]; then
    echo "[WARN] failed to read CPLD $CPLD_ADDR offset $CPLD_REG on i2c-$I2C_BUS, default AC"
    PSU_TYPE=$PSU_TYPE_AC
else
    PSU_TYPE=$((REG & 0x1))
fi

if [ "$PSU_TYPE" = "$PSU_TYPE_DC" ]; then
    _link_pddf_device "$PDDF_DEV_PATH/pddf-device-dc.json"
else
    _link_pddf_device "$PDDF_DEV_PATH/pddf-device-ac.json"
fi
