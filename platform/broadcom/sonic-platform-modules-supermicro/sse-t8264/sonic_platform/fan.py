#!/usr/bin/env python


try:
    import subprocess
    from sonic_platform_pddf_base.pddf_fan import PddfFan
except ImportError as e:
    raise ImportError(str(e) + "- required module not found")


class Fan(PddfFan):
    """PDDF Platform-Specific Fan class"""

    def __init__(self, tray_idx, fan_idx=0, pddf_data=None, pddf_plugin_data=None, is_psu_fan=False, psu_index=0):
        # idx is 0-based
        PddfFan.__init__(self, tray_idx, fan_idx, pddf_data, pddf_plugin_data, is_psu_fan, psu_index)

    def get_fault(self):
        if self.is_psu_fan:
            return False

        idx = (self.fantray_index-1)*self.platform['num_fans_pertray'] + self.fan_index
        attr_name = "fan" + str(idx) + "_fault"
        output = self.pddf_obj.get_attr_name_output("FAN-CTRL", attr_name)
        if not output:
            return False

        mode = output['mode']
        fault = output['status'].rstrip()
        vmap = self.plugin_data['FAN']['fault'][mode]['valmap']
        if fault in vmap:
            return vmap[fault]
        return False

    def get_status(self):
        speed = self.get_speed_rpm()
        fault = self.get_fault()
        return (speed != 0 and not fault)

    def get_direction(self):
        """
        Retrieves the direction of the fan based on IPMI FRU data.

        Returns:
            str: FAN_DIRECTION_INTAKE or FAN_DIRECTION_EXHAUST
        """
        if self.is_psu_fan:
            return super().get_direction()
        else:
            if not self.get_presence():
                return None

            fru_id = self.fantray_index + 2  # 1->3, 2->4, 3->5, 4->6
            cmd = ["ipmitool", "fru", "print", str(fru_id)]

            try:
                result = subprocess.run(cmd, text=True, capture_output=True, check=True)
                fru_data = result.stdout.splitlines()
            except subprocess.CalledProcessError as e:
                print(f"Error executing IPMI command: {e}")
                return self.FAN_DIRECTION_EXHAUST

            asset_tag = None
            for line in fru_data:
                if line.startswith(" Product Asset Tag"):
                    asset_tag = line.split(":")[-1].strip()
                    break

            return self.FAN_DIRECTION_INTAKE if asset_tag == "B2F" else self.FAN_DIRECTION_EXHAUST
