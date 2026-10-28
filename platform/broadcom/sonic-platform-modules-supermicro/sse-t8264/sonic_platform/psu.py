#!/usr/bin/env python
#


try:
    import os
    import time
    from sonic_platform_pddf_base.pddf_psu import PddfPsu
except ImportError as e:
    raise ImportError (str(e) + "- required module not found")


class _PddfPsuutilFanStub(object):
    """pddf_psuutil mfrinfo/seninfo always index _fan_list[0]; DC bricks have no fan."""

    def get_direction(self):
        return None

    def get_speed_rpm(self):
        return None


class Psu(PddfPsu):
    """PDDF Platform-Specific PSU class"""

    PDDF_DEVICE_JSON = '/usr/share/sonic/platform/pddf/pddf-device.json'
    _sku_psu_type = None

    def __init__(self, index, pddf_data=None, pddf_plugin_data=None):
        PddfPsu.__init__(self, index, pddf_data, pddf_plugin_data)
        # Keep JSON num_psu_fans=0 so PDDF does not create a real PSU fan
        # (0 RPM would look like a fault). pddf_psuutil still needs [0].
        if self.get_type() == 'DC' and not self._fan_list:
            self._fan_list.append(_PddfPsuutilFanStub())

    # Provide the functions/variables below for which implementation is to be overwritten
    def get_type(self):
        if Psu._sku_psu_type is None:
            Psu._sku_psu_type = self._type_from_pddf_symlink()
        return Psu._sku_psu_type

    def _type_from_pddf_symlink(self):
        """AC/DC SKU from pddf-device.json symlink (set by pddf_pre_device_create.sh)."""
        path = self.PDDF_DEVICE_JSON
        if not os.path.lexists(path):
            platform = os.path.basename(os.path.realpath('/usr/share/sonic/platform'))
            path = '/usr/share/sonic/device/{}/pddf/pddf-device.json'.format(platform)
        try:
            name = os.path.basename(os.path.realpath(path)).lower()
        except OSError:
            return 'AC'
        if name.endswith('-dc.json'):
            return 'DC'
        return 'AC'

    def get_num_fans(self):
        if self.get_type() == 'DC':
            return 0
        return super().get_num_fans()

    def get_all_fans(self):
        if self.get_type() == 'DC':
            return []
        return super().get_all_fans()

    def get_fan(self, index):
        if self.get_type() == 'DC':
            return None
        return super().get_fan(index)

    def get_capacity(self):
        return self.get_maximum_supplied_power()

    def get_maximum_supplied_power(self):
        if self.get_type() == 'DC':
            return 2000
        return super().get_maximum_supplied_power()

    def get_input_power(self):
        if self.get_type() == 'DC':
            return self.get_input_voltage() * self.get_input_current()
        return super().get_input_power()

    def get_status(self):
        if super().get_status():
            # Ensure get_model is ready for psud to update
            for _ in range(10):
                if self.get_model() != 'N/A':
                    return True
                time.sleep(0.5)
            return super().get_status()
        else:
            return False
