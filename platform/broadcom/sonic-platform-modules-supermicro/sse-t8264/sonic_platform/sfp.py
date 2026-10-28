#!/usr/bin/env python

try:
    import os
    import time
    from sonic_platform_pddf_base.pddf_sfp import PddfSfp
    import re
except ImportError as e:
    raise ImportError (str(e) + "- required module not found")


class Sfp(PddfSfp):
    """
    PDDF Platform-Specific Sfp class
    """

    def __init__(self, index, pddf_data=None, pddf_plugin_data=None):
        PddfSfp.__init__(self, index, pddf_data, pddf_plugin_data)

    # Provide the functions/variables below for which implementation is to be overwritten
    def get_intr_status(self):
        """
        Retrieves the interrupt status for this transceiver
        Returns:
            A Boolean, True if there is interrupt, False if not
        """
        if self.sfp_type == 'QSFP28':
            intr_status = False

            # Interrupt status can be checked for absent ports too
            device = 'PORT{}'.format(self.port_index)
            output = self.pddf_obj.get_attr_name_output(device, 'xcvr_intr_status')

            if output:
                status = int(output['status'].rstrip())

                if status == 0:
                    intr_status = True
                else:
                    intr_status = False

            return intr_status
        else:
            return super().get_intr_status()

    def get_reset_status(self):
        """
        Retrieves the reset status of SFP
        Returns:
            A Boolean, True if reset enabled, False if disabled
        """
        if self.sfp_type == 'QSFP28':
            reset_status = False

            device = 'PORT{}'.format(self.port_index)
            output = self.pddf_obj.get_attr_name_output(device, 'xcvr_reset')

            if output:
                status = int(output['status'].rstrip())

                if status == 0:
                    reset_status = True
                else:
                    reset_status = False

            return reset_status
        else:
            return super().get_reset_status()

    def reset(self):
        """
        Reset SFP and return all user module settings to their default srate.
        Returns:
            A boolean, True if successful, False if not
        """
        if self.sfp_type == 'QSFP28':
            device = 'PORT{}'.format(self.port_index)
            path = self.pddf_obj.get_path(device, 'xcvr_reset')

            if path is None:
                return False
            else:
                path_dir = re.sub(r'/value$', '/direction', path)

                try:
                    with open(path_dir, 'r+') as f_dir:
                        f_dir.seek(0)
                        f_dir.write('low')
                        f_dir.flush()
                        time.sleep(1)
                        f_dir.seek(0)
                        f_dir.write('high')
                    return True
                except Exception:
                    return False
        else:
            return super().reset()

    def get_lpmode_via_pin(self):
        """
        Retrieves the lpmode (low power mode) status of this SFP
        Returns:
            A Boolean, True if lpmode is enabled, False if disabled
        """
        if self.sfp_type == 'QSFP28':
            lpmode = False

            # Interrupt status can be checked for absent ports too
            device = 'PORT{}'.format(self.port_index)
            output = self.pddf_obj.get_attr_name_output(device, 'xcvr_lpmode')

            if output:
                status = int(output['status'].rstrip())

                if status == 0:
                    lpmode = False
                else:
                    lpmode = True

            return lpmode
        else:
            return super().get_lpmode_via_pin()

    def set_lpmode_via_pin(self, lpmode):
        """
        Sets the lpmode (low power mode) of SFP
        Args:
            lpmode: A Boolean, True to enable lpmode, False to disable it
            Note  : lpmode can be overridden by set_power_override
        Returns:
            A boolean, True if lpmode is set successfully, False if not
        """
        if self.sfp_type == 'QSFP28':
            device = 'PORT{}'.format(self.port_index)
            path_val = self.pddf_obj.get_path(device, 'xcvr_lpmode')

            if path_val is None:
                return False

            path_dir = re.sub(r'/value$', '/direction', path_val)

            try:
                with open(path_dir, 'r+') as f_dir:
                    f_dir.seek(0)
                    f_dir.write('high' if lpmode else 'low')
                    f_dir.flush()
                return True
            except Exception:
                return False
        else:
            return super().set_lpmode_via_pin(lpmode)

    def read_eeprom_with_pread(self, offset: int, num_bytes: int) -> bytearray | None:
        """
        Read a raw sequence of bytes from EEPROM starting at 'offset' with length 'num_bytes'.

        Args:
            offset (int): The byte offset from which the read transaction will start.
            num_bytes (int): The number of bytes to read.

        Returns:
            bytearray: Raw bytes if read succeeds and exact 'num_bytes' are returned.
            None: If the read operation fails (invalid args, IO error, or short read).
        """
        # Basic argument validation
        if num_bytes <= 0 or offset < 0:
            return None

        # Ensure an EEPROM path is available on the instance
        eeprom_path = getattr(self, "eeprom_path", None)
        if not eeprom_path:
            return None

        fd = None
        try:
            # Open as read-only and perform position-independent read via pread
            fd = os.open(eeprom_path, os.O_RDONLY)
            data = os.pread(fd, num_bytes, offset)

            # Enforce exact-length read; return None on short read
            if len(data) != num_bytes:
                return None

            return bytearray(data)

        except Exception:
            return None

        finally:
            if fd is not None:
                try:
                    os.close(fd)
                except OSError:
                    # Swallow close errors to keep the interface simple
                    pass

    def get_temperature(self):
        """
        Read loopback temperature using read_eeprom_with_pread().

        Behavior:
          - Reads 2 bytes at offset 14 (Big-Endian u16).
          - Converts raw value to temperature via value / 256.0 (Celsius).
          - Returns a float representing the current temperature in Celsius.
          - Returns 0.0 if any error occurs (e.g., missing path, short read, parse failure).

        Returns:
            float: Temperature in Celsius, or 0.0 on failure.
        """
        if self.sfp_type == 'QSFP_DD' or self.sfp_type == 'OSFP':
            try:
                TEMP_OFFSET = 14       # Byte offset for the temperature field
                TEMP_LEN = 2           # Field length in bytes
                TEMP_SCALE = 256.0     # Temperature = raw_u16 / 256.0

                # Read raw bytes (bytearray) via pread-based helper
                raw = self.read_eeprom_with_pread(TEMP_OFFSET, TEMP_LEN)
                if raw is None or len(raw) != TEMP_LEN:
                    return 0.0

                # Parse Big-Endian 16-bit value
                raw_u16 = (raw[0] << 8) | raw[1]

                return raw_u16 / TEMP_SCALE
            except Exception:
                return 0.0
        else:
            return super().get_temperature()


