#!/usr/bin/env python


try:
    import re
    from sonic_platform_pddf_base.pddf_thermal import PddfThermal
except ImportError as e:
    raise ImportError(str(e) + "- required module not found")



class Thermal(PddfThermal):
    """PDDF Platform-Specific Thermal class"""

    def __init__(self, index, pddf_data=None, pddf_plugin_data=None, is_psu_thermal=False, psu_index=0):
        PddfThermal.__init__(self, index, pddf_data, pddf_plugin_data, is_psu_thermal, psu_index)

    def _get_psu_index_from_name(self):
        """Return PSU index if display name starts with PSUn, else None."""
        name = self.get_name()
        if not name:
            return None
        match = re.match(r'^PSU(\d+)\b', name)
        if match:
            return int(match.group(1))
        return None

    def _is_associated_psu_present(self):
        """
        For TEMP sensors named PSUn*, require the matching PSU to be present.
        Other sensors are treated as present.
        """
        psu_index = self._get_psu_index_from_name()
        if psu_index is None:
            return True

        device = "PSU{}".format(psu_index)
        output = self.pddf_obj.get_attr_name_output(device, "psu_present")
        if not output:
            return False

        mode = output['mode']
        status = output['status'].rstrip('\n')
        vmap = self.plugin_data['PSU']['psu_present'][mode]['valmap']
        return vmap.get(status, False)

    def get_presence(self):
        if not self._is_associated_psu_present():
            return False
        return super().get_presence()

    def get_temperature(self):
        if not self._is_associated_psu_present():
            return None
        return super().get_temperature()

    def get_high_threshold(self):
        if not self._is_associated_psu_present():
            return None
        return super().get_high_threshold()

    def get_low_threshold(self):
        if not self._is_associated_psu_present():
            return None
        return super().get_low_threshold()

    def get_high_critical_threshold(self):
        if not self._is_associated_psu_present():
            return None
        return super().get_high_critical_threshold()

    def get_low_critical_threshold(self):
        if not self._is_associated_psu_present():
            return None
        return super().get_low_critical_threshold()
