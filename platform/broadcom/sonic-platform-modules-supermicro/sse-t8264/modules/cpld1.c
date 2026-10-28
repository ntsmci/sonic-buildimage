#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/watchdog.h>

static int i2c_bus = 0;
static int i2c_addr = 0x2b;

module_param(i2c_bus, int, S_IRUGO);
MODULE_PARM_DESC(i2c_bus, "I2C bus number");

module_param(i2c_addr, int, S_IRUGO);
MODULE_PARM_DESC(i2c_addr, "I2C device address");

#define OSFP_PORT_TOTAL 64

/* Refer to SSE_T8264 HW SPEC for more details */
/* The Switch Model ID of this project is 0x02 */
#define REG_MODEL_ID 0x00
#define REG_BOARD_ID 0x01
/* CPLD_ID */
#define REG_CPLD_ID 0x02
/* CPLD version */
#define REG_CPLD_VER 0x03
#define REG_DEV_RST_CTRL 0x06
#define BITPOS_WDT_CLR 0
#define BITPOS_RSTN_SWITCH 1
#define BITPOS_SWITCH_PWRDOWN_N 2
#define BITPOS_WDT_EN 3
/* BCM UART control */
#define REG_UART_CONTROL 0x09
/* CPLD2 */
#define REG_CPLD2_INTR 0x10
#define REG_CPLD2_BOARD_ID 0x11
#define REG_CPLD2_CPLD_ID 0x12
#define REG_CPLD2_CPLD_VER 0x13
#define REG_CPLD2_EVENT_FLAG 0x14
#define REG_CPLD2_EVENT_CLR 0xB0
#define REG_CPLD2_PFR_REV_ID3 0x15
#define REG_CPLD2_PFR_REV_ID2 0x16
#define REG_CPLD2_PFR_REV_ID1 0x17
/* CPLD3 */
#define REG_CPLD3_INTR 0x18
#define REG_CPLD3_BOARD_ID 0x19
#define REG_CPLD3_CPLD_ID 0x1A
#define REG_CPLD3_CPLD_VER 0x1B
#define REG_CPLD3_EVENT_FLAG 0x1C
#define REG_CPLD3_EVENT_CLR 0xB1
#define REG_CPLD3_PFR_REV_ID3 0x1D
#define REG_CPLD3_PFR_REV_ID2 0x1E
#define REG_CPLD3_PFR_REV_ID1 0x1F
/* E-fuse */
#define REG_EFUSE_PWRGOOD_1 0x20
#define REG_EFUSE_PWRGOOD_2 0x21
#define REG_EFUSE_FAULT_N_1 0x22
#define REG_EFUSE_FAULT_N_2 0x23
/* Watchdog */
#define REG_WDT_MAXL 0x32
#define REG_WDT_MAXM 0x33
#define REG_WDT_CNTL 0x34
#define REG_WDT_CNTM 0x35
#define REG_WDT_REC 0x40
#define BITPOS_WDT_REC 4
#define BITPOS_WDT_REC_CFG 4
/* LED Debug mode */
#define REG_LED_DEBUG 0x41
/* OSFP Enable LED flag*/
#define REG_OSFP_EN_LED_0 0x70 // bit[7:0]: TSC { 1,   0,   5,   4,   9,   8,   13,  12 }, PORT 1,2,3,4,5,6,7,8
#define REG_OSFP_EN_LED_1 0x71 // bit[7:0]: TSC { 17,  16,  21,  20,  25,  24,  29,  28 }, PORT 9,10, 11,12,13,14,15,16
#define REG_OSFP_EN_LED_2 0x72 // bit[7:0]: TSC { 33,  32,  37,  36,  41,  40,  45,  44 }, PORT 17,18,19,20,21,22,23,24
#define REG_OSFP_EN_LED_3 0x73 // bit[7:0]: TSC { 49,  48,  53,  52,  57,  56,  61,  60 }, PORT 25,26,27,28,29,30,31,32
#define REG_OSFP_EN_LED_4 0x74 // bit[7:0]: TSC { 62,  63,  58,  59,  54,  55,  50,  51 }, PORT 64,63,62,61,60,59,58,57
#define REG_OSFP_EN_LED_5 0x75 // bit[7:0]: TSC { 46,  47,  42,  43,  38,  39,  34,  35 }, PORT 56,55,54,53,52,51,50,49
#define REG_OSFP_EN_LED_6 0x76 // bit[7:0]: TSC { 30,  31,  26,  27,  22,  23,  18,  19 }, PORT 48,47,46,45,44,43,42,41
#define REG_OSFP_EN_LED_7 0x77 // bit[7:0]: TSC { 14,  15,  10,  11,  6,   7,   2,   3  }, PORT 40,39,38,37,36,35,34,33
/* MGMT QSFP */
#define REG_QSFP_STATUS 0x80 // bit[4]:Present, bit[0]:Enable
#define BITPOS_QSFP_EN_LED 0
#define BITPOS_QSFP_PRESENT 4
/* OSFP Sideband */
#define REG_OSFP_PRESENT_0 0x90 // bit[7:0]: TSC { 1,   0,   5,   4,   9,   8,   13,  12 }, PORT 1,2,3,4,5,6,7,8
#define REG_OSFP_PRESENT_1 0x91 // bit[7:0]: TSC { 17,  16,  21,  20,  25,  24,  29,  28 }, PORT 9,10, 11,12,13,14,15,16
#define REG_OSFP_PRESENT_2 0x92 // bit[7:0]: TSC { 33,  32,  37,  36,  41,  40,  45,  44 }, PORT 17,18,19,20,21,22,23,24
#define REG_OSFP_PRESENT_3 0x93 // bit[7:0]: TSC { 49,  48,  53,  52,  57,  56,  61,  60 }, PORT 25,26,27,28,29,30,31,32
#define REG_OSFP_PRESENT_4 0x94 // bit[7:0]: TSC { 62,  63,  58,  59,  54,  55,  50,  51 }, PORT 64,63,62,61,60,59,58,57
#define REG_OSFP_PRESENT_5 0x95 // bit[7:0]: TSC { 46,  47,  42,  43,  38,  39,  34,  35 }, PORT 56,55,54,53,52,51,50,49
#define REG_OSFP_PRESENT_6 0x96 // bit[7:0]: TSC { 30,  31,  26,  27,  22,  23,  18,  19 }, PORT 48,47,46,45,44,43,42,41
#define REG_OSFP_PRESENT_7 0x97 // bit[7:0]: TSC { 14,  15,  10,  11,  6,   7,   2,   3  }, PORT 40,39,38,37,36,35,34,33

#define REG_OSFP_INTR_0 0x98 // bit[7:0]: TSC { 1,   0,   5,   4,   9,   8,   13,  12 }, PORT 1,2,3,4,5,6,7,8
#define REG_OSFP_INTR_1 0x99 // bit[7:0]: TSC { 17,  16,  21,  20,  25,  24,  29,  28 }, PORT 9,10, 11,12,13,14,15,16
#define REG_OSFP_INTR_2 0x9A // bit[7:0]: TSC { 33,  32,  37,  36,  41,  40,  45,  44 }, PORT 17,18,19,20,21,22,23,24
#define REG_OSFP_INTR_3 0x9B // bit[7:0]: TSC { 49,  48,  53,  52,  57,  56,  61,  60 }, PORT 25,26,27,28,29,30,31,32
#define REG_OSFP_INTR_4 0x9C // bit[7:0]: TSC { 62,  63,  58,  59,  54,  55,  50,  51 }, PORT 64,63,62,61,60,59,58,57
#define REG_OSFP_INTR_5 0x9D // bit[7:0]: TSC { 46,  47,  42,  43,  38,  39,  34,  35 }, PORT 56,55,54,53,52,51,50,49
#define REG_OSFP_INTR_6 0x9E // bit[7:0]: TSC { 30,  31,  26,  27,  22,  23,  18,  19 }, PORT 48,47,46,45,44,43,42,41
#define REG_OSFP_INTR_7 0x9F // bit[7:0]: TSC { 14,  15,  10,  11,  6,   7,   2,   3  }, PORT 40,39,38,37,36,35,34,33

#define REG_OSFP_LPW_N_0 0xA0 // bit[7:0]: TSC { 1,   0,   5,   4,   9,   8,   13,  12 }, PORT 1,2,3,4,5,6,7,8
#define REG_OSFP_LPW_N_1 0xA1 // bit[7:0]: TSC { 17,  16,  21,  20,  25,  24,  29,  28 }, PORT 9,10, 11,12,13,14,15,16
#define REG_OSFP_LPW_N_2 0xA2 // bit[7:0]: TSC { 33,  32,  37,  36,  41,  40,  45,  44 }, PORT 17,18,19,20,21,22,23,24
#define REG_OSFP_LPW_N_3 0xA3 // bit[7:0]: TSC { 49,  48,  53,  52,  57,  56,  61,  60 }, PORT 25,26,27,28,29,30,31,32
#define REG_OSFP_LPW_N_4 0xA4 // bit[7:0]: TSC { 62,  63,  58,  59,  54,  55,  50,  51 }, PORT 64,63,62,61,60,59,58,57
#define REG_OSFP_LPW_N_5 0xA5 // bit[7:0]: TSC { 46,  47,  42,  43,  38,  39,  34,  35 }, PORT 56,55,54,53,52,51,50,49
#define REG_OSFP_LPW_N_6 0xA6 // bit[7:0]: TSC { 30,  31,  26,  27,  22,  23,  18,  19 }, PORT 48,47,46,45,44,43,42,41
#define REG_OSFP_LPW_N_7 0xA7 // bit[7:0]: TSC { 14,  15,  10,  11,  6,   7,   2,   3  }, PORT 40,39,38,37,36,35,34,33

#define REG_OSFP_RST_N_0 0xA8 // bit[7:0]: TSC { 1,   0,   5,   4,   9,   8,   13,  12 }, PORT 1,2,3,4,5,6,7,8
#define REG_OSFP_RST_N_1 0xA9 // bit[7:0]: TSC { 17,  16,  21,  20,  25,  24,  29,  28 }, PORT 9,10, 11,12,13,14,15,16
#define REG_OSFP_RST_N_2 0xAA // bit[7:0]: TSC { 33,  32,  37,  36,  41,  40,  45,  44 }, PORT 17,18,19,20,21,22,23,24
#define REG_OSFP_RST_N_3 0xAB // bit[7:0]: TSC { 49,  48,  53,  52,  57,  56,  61,  60 }, PORT 25,26,27,28,29,30,31,32
#define REG_OSFP_RST_N_4 0xAC // bit[7:0]: TSC { 62,  63,  58,  59,  54,  55,  50,  51 }, PORT 64,63,62,61,60,59,58,57
#define REG_OSFP_RST_N_5 0xAD // bit[7:0]: TSC { 46,  47,  42,  43,  38,  39,  34,  35 }, PORT 56,55,54,53,52,51,50,49
#define REG_OSFP_RST_N_6 0xAE // bit[7:0]: TSC { 30,  31,  26,  27,  22,  23,  18,  19 }, PORT 48,47,46,45,44,43,42,41
#define REG_OSFP_RST_N_7 0xAF // bit[7:0]: TSC { 14,  15,  10,  11,  6,   7,   2,   3  }, PORT 40,39,38,37,36,35,34,33

/* CPLD code released date --- Month */
#define REG_CPLD_MOT 0xFE
/* CPLD code released date --- Day */
#define REG_CPLD_DAY 0xFF

enum PORT_TYPE { NONE, OSFP, QSFP, SFP };

struct cpld_data {
	struct cdev cdev;
	struct mutex lock;
	struct device *cpld_device;
	struct device *sff_devices[OSFP_PORT_TOTAL];
};

struct cpld_port_data {
	struct cpld_data *parent;
	int port_id;
};

static int cpld_major;
static struct class *cpld_class = NULL;
static struct cpld_data *cpld_dev_data;
static struct device *sff_dev = NULL; // Just for CPLD1/SFF directory, not a real device

/* used to access CPLD1 registers not defined in this driver with sys filesystem */
static uint8_t testee_offset = 0;

static int read_cpld_register(u8 reg_addr, uint8_t *value)
{
	struct i2c_adapter *adapter;
	struct i2c_client client;
	int ret;

	adapter = i2c_get_adapter(i2c_bus);
	if (!adapter) {
		return -ENODEV;
	}
	client.adapter = adapter;
	client.addr = i2c_addr;
	client.flags = 0;

	ret = i2c_smbus_read_byte_data(&client, reg_addr);
	if (ret >= 0) {
		*value = ret;
	}
	i2c_put_adapter(adapter);
	return ret >= 0 ? 0 : ret;
}

static int write_cpld_register(u8 reg_addr, uint8_t value)
{
	struct i2c_adapter *adapter;
	struct i2c_client client;
	int ret;

	adapter = i2c_get_adapter(i2c_bus);
	if (!adapter) {
		return -ENODEV;
	}

	client.adapter = adapter;
	client.addr = i2c_addr;
	client.flags = 0;

	ret = i2c_smbus_write_byte_data(&client, reg_addr, value);

	i2c_put_adapter(adapter);
	return ret;
}

/* CPLD1 attributes */
static ssize_t cpld1_board_id_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t value;
	int ret;

	ret = read_cpld_register(REG_BOARD_ID, &value);
	if (ret < 0)
		return ret;

	return sprintf(buf, "0x%02x\n", value);
}

struct device_attribute dev_attr_cpld1_board_id = __ATTR(cpld1_board_id, 0400, cpld1_board_id_show, NULL);

static ssize_t cpld1_id_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t value;
	int ret;

	ret = read_cpld_register(REG_CPLD_ID, &value);
	if (ret < 0)
		return ret;

	return sprintf(buf, "0x%02x\n", value);
}
struct device_attribute dev_attr_cpld1_id = __ATTR(cpld1_id, 0400, cpld1_id_show, NULL);

static ssize_t cpld1_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t value;
	int ret;

	ret = read_cpld_register(REG_CPLD_VER, &value);
	if (ret < 0)
		return ret;

	return sprintf(buf, "0x%02x\n", value);
}

struct device_attribute dev_attr_cpld1_version = __ATTR(cpld1_version, 0400, cpld1_version_show, NULL);

// CPLD2
static ssize_t cpld2_board_id_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t value;
	int ret;

	ret = read_cpld_register(REG_CPLD2_BOARD_ID, &value);
	if (ret < 0)
		return ret;

	return sprintf(buf, "0x%02x\n", value);
}
struct device_attribute dev_attr_cpld2_board_id = __ATTR(cpld2_board_id, 0400, cpld2_board_id_show, NULL);

static ssize_t cpld2_id_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t value;
	int ret;

	ret = read_cpld_register(REG_CPLD2_CPLD_ID, &value);
	if (ret < 0)
		return ret;

	return sprintf(buf, "0x%02x\n", value);
}
struct device_attribute dev_attr_cpld2_id = __ATTR(cpld2_id, 0400, cpld2_id_show, NULL);

static ssize_t cpld2_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t value;
	int ret;

	ret = read_cpld_register(REG_CPLD2_CPLD_VER, &value);
	if (ret < 0)
		return ret;

	return sprintf(buf, "0x%02x\n", value);
}
struct device_attribute dev_attr_cpld2_version = __ATTR(cpld2_version, 0400, cpld2_version_show, NULL);

static ssize_t cpld2_intr_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t value;
	int ret;

	ret = read_cpld_register(REG_CPLD2_INTR, &value);
	if (ret < 0)
		return ret;

	return sprintf(buf, "0x%02x\n", value);
}
struct device_attribute dev_attr_cpld2_intr = __ATTR(cpld2_intr, 0400, cpld2_intr_show, NULL);

static ssize_t cpld2_event_flag_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t value;
	int ret;

	ret = read_cpld_register(REG_CPLD2_EVENT_FLAG, &value);
	if (ret < 0)
		return ret;

	return sprintf(buf, "0x%02x\n", value);
}
struct device_attribute dev_attr_cpld2_event_flag = __ATTR(cpld2_event_flag, 0400, cpld2_event_flag_show, NULL);

static ssize_t cpld2_event_clr_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t value;
	int ret;

	ret = read_cpld_register(REG_CPLD2_EVENT_CLR, &value);
	if (ret < 0)
		return ret;

	return sprintf(buf, "0x%02x\n", value);
}

static ssize_t cpld2_event_clr_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long value;
	int err;

	err = kstrtoul(buf, 0, &value);
	if (err)
		return err;

	mutex_lock(&cpld_dev_data->lock);
	err = write_cpld_register(REG_CPLD2_EVENT_CLR, (u8)value);
	mutex_unlock(&cpld_dev_data->lock);

	return (err == 0) ? count : -EIO;
}
struct device_attribute dev_attr_cpld2_event_clr = __ATTR(cpld2_event_clr, 0600, cpld2_event_clr_show, cpld2_event_clr_store);

static ssize_t cpld2_pfr_rev_id1_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t value;
	int ret;

	ret = read_cpld_register(REG_CPLD2_PFR_REV_ID1, &value);
	if (ret < 0)
		return ret;

	return sprintf(buf, "0x%02x\n", value);
}
struct device_attribute dev_attr_cpld2_pfr_rev_id1 = __ATTR(cpld2_pfr_rev_id1, 0400, cpld2_pfr_rev_id1_show, NULL);

static ssize_t cpld2_pfr_rev_id2_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t value;
	int ret;

	ret = read_cpld_register(REG_CPLD2_PFR_REV_ID2, &value);
	if (ret < 0)
		return ret;

	return sprintf(buf, "0x%02x\n", value);
}
struct device_attribute dev_attr_cpld2_pfr_rev_id2 = __ATTR(cpld2_pfr_rev_id2, 0400, cpld2_pfr_rev_id2_show, NULL);

static ssize_t cpld2_pfr_rev_id3_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t value;
	int ret;

	ret = read_cpld_register(REG_CPLD2_PFR_REV_ID3, &value);
	if (ret < 0)
		return ret;

	return sprintf(buf, "0x%02x\n", value);
}
struct device_attribute dev_attr_cpld2_pfr_rev_id3 = __ATTR(cpld2_pfr_rev_id3, 0400, cpld2_pfr_rev_id3_show, NULL);
// CPLD3
static ssize_t cpld3_board_id_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t value;
	int ret;

	ret = read_cpld_register(REG_CPLD3_BOARD_ID, &value);
	if (ret < 0)
		return ret;

	return sprintf(buf, "0x%02x\n", value);
}
struct device_attribute dev_attr_cpld3_board_id = __ATTR(cpld3_board_id, 0400, cpld3_board_id_show, NULL);

static ssize_t cpld3_id_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t value;
	int ret;

	ret = read_cpld_register(REG_CPLD3_CPLD_ID, &value);
	if (ret < 0)
		return ret;

	return sprintf(buf, "0x%02x\n", value);
}
struct device_attribute dev_attr_cpld3_id = __ATTR(cpld3_id, 0400, cpld3_id_show, NULL);

static ssize_t cpld3_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t value;
	int ret;

	ret = read_cpld_register(REG_CPLD3_CPLD_VER, &value);
	if (ret < 0)
		return ret;

	return sprintf(buf, "0x%02x\n", value);
}
struct device_attribute dev_attr_cpld3_version = __ATTR(cpld3_version, 0400, cpld3_version_show, NULL);

static ssize_t cpld3_intr_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t value;
	int ret;

	ret = read_cpld_register(REG_CPLD3_INTR, &value);
	if (ret < 0)
		return ret;

	return sprintf(buf, "0x%02x\n", value);
}
struct device_attribute dev_attr_cpld3_intr = __ATTR(cpld3_intr, 0400, cpld3_intr_show, NULL);

static ssize_t cpld3_event_flag_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t value;
	int ret;

	ret = read_cpld_register(REG_CPLD3_EVENT_FLAG, &value);
	if (ret < 0)
		return ret;

	return sprintf(buf, "0x%02x\n", value);
}
struct device_attribute dev_attr_cpld3_event_flag = __ATTR(cpld3_event_flag, 0400, cpld3_event_flag_show, NULL);

static ssize_t cpld3_event_clr_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t value;
	int ret;

	ret = read_cpld_register(REG_CPLD3_EVENT_CLR, &value);
	if (ret < 0)
		return ret;

	return sprintf(buf, "0x%02x\n", value);
}

static ssize_t cpld3_event_clr_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long value;
	int err;

	err = kstrtoul(buf, 0, &value);
	if (err)
		return err;

	mutex_lock(&cpld_dev_data->lock);
	err = write_cpld_register(REG_CPLD3_EVENT_CLR, (u8)value);
	mutex_unlock(&cpld_dev_data->lock);

	return (err == 0) ? count : -EIO;
}
struct device_attribute dev_attr_cpld3_event_clr = __ATTR(cpld3_event_clr, 0600, cpld3_event_clr_show, cpld3_event_clr_store);

static ssize_t cpld3_pfr_rev_id1_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t value;
	int ret;

	ret = read_cpld_register(REG_CPLD3_PFR_REV_ID1, &value);
	if (ret < 0)
		return ret;

	return sprintf(buf, "0x%02x\n", value);
}
struct device_attribute dev_attr_cpld3_pfr_rev_id1 = __ATTR(cpld3_pfr_rev_id1, 0400, cpld3_pfr_rev_id1_show, NULL);

static ssize_t cpld3_pfr_rev_id2_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t value;
	int ret;

	ret = read_cpld_register(REG_CPLD3_PFR_REV_ID2, &value);
	if (ret < 0)
		return ret;

	return sprintf(buf, "0x%02x\n", value);
}
struct device_attribute dev_attr_cpld3_pfr_rev_id2 = __ATTR(cpld3_pfr_rev_id2, 0400, cpld3_pfr_rev_id2_show, NULL);

static ssize_t cpld3_pfr_rev_id3_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t value;
	int ret;

	ret = read_cpld_register(REG_CPLD3_PFR_REV_ID3, &value);
	if (ret < 0)
		return ret;

	return sprintf(buf, "0x%02x\n", value);
}
struct device_attribute dev_attr_cpld3_pfr_rev_id3 = __ATTR(cpld3_pfr_rev_id3, 0400, cpld3_pfr_rev_id3_show, NULL);
// JED FW release date
static ssize_t jed_rel_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint16_t date = 0;
	uint8_t month, day;
	int ret;

	ret = read_cpld_register(REG_CPLD_MOT, &month);
	if (ret < 0)
		return ret;

	ret = read_cpld_register(REG_CPLD_DAY, &day);
	if (ret < 0)
		return ret;

	date = (uint16_t)day | ((uint16_t)month << 8);
	return sprintf(buf, "%4.4hx\n", date);
}
struct device_attribute dev_attr_jed_rel = __ATTR(jed_rel, 0400, jed_rel_show, NULL);

// LED debug mode
static ssize_t led_debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t value;
	int ret;

	mutex_lock(&cpld_dev_data->lock);
	ret = read_cpld_register(REG_LED_DEBUG, &value);
	mutex_unlock(&cpld_dev_data->lock);

	if (ret < 0)
		return ret;

	return sprintf(buf, "0x%02x\n", value);
}

static ssize_t led_debug_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long value;
	int err;

	err = kstrtoul(buf, 0, &value);
	if (err)
		return err;

	mutex_lock(&cpld_dev_data->lock);
	err = write_cpld_register(REG_LED_DEBUG, (u8)value);
	mutex_unlock(&cpld_dev_data->lock);

	return (err == 0) ? count : -EIO;
}

struct device_attribute dev_attr_led_debug = __ATTR(led_debug, 0600, led_debug_show, led_debug_store);

// Switch Model ID
static ssize_t model_id_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t sw_id;
	int ret;

	ret = read_cpld_register(REG_MODEL_ID, &sw_id);
	if (ret < 0)
		return ret;

	return sprintf(buf, "0x%02x\n", sw_id);
}
struct device_attribute dev_attr_model_id = __ATTR(model_id, 0400, model_id_show, NULL);

// OSFP Efuse

static ssize_t osfp_efuse_fault_n_1_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t value;
	int ret;

	ret = read_cpld_register(REG_EFUSE_FAULT_N_1, &value);
	if (ret < 0)
		return ret;

	return sprintf(buf, "0x%02x\n", value);
}
struct device_attribute dev_attr_osfp_efuse_fault_n_1 = __ATTR(osfp_efuse_fault_n_1, 0400, osfp_efuse_fault_n_1_show, NULL);

static ssize_t osfp_efuse_fault_n_2_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t value;
	int ret;

	ret = read_cpld_register(REG_EFUSE_FAULT_N_2, &value);
	if (ret < 0)
		return ret;

	return sprintf(buf, "0x%02x\n", value);
}
struct device_attribute dev_attr_osfp_efuse_fault_n_2 = __ATTR(osfp_efuse_fault_n_2, 0400, osfp_efuse_fault_n_2_show, NULL);

static ssize_t osfp_efuse_powergood_1_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t value;
	int ret;

	ret = read_cpld_register(REG_EFUSE_PWRGOOD_1, &value);
	if (ret < 0)
		return ret;

	return sprintf(buf, "0x%02x\n", value);
}
struct device_attribute dev_attr_osfp_efuse_powergood_1 = __ATTR(osfp_efuse_powergood_1, 0400, osfp_efuse_powergood_1_show, NULL);

static ssize_t osfp_efuse_powergood_2_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t value;
	int ret;

	ret = read_cpld_register(REG_EFUSE_PWRGOOD_2, &value);
	if (ret < 0)
		return ret;

	return sprintf(buf, "0x%02x\n", value);
}
struct device_attribute dev_attr_osfp_efuse_powergood_2 = __ATTR(osfp_efuse_powergood_2, 0400, osfp_efuse_powergood_2_show, NULL);

// OSFP enable LED flag
static ssize_t osfp_en_led_0_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	uint8_t value;
	int ret;

	ret = read_cpld_register(REG_OSFP_EN_LED_0, &value);

	if (ret < 0)
		return ret;

	return sprintf(buf, "0x%02x\n", value);
}

static ssize_t osfp_en_led_0_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long value;
	int err;

	err = kstrtoul(buf, 0, &value);
	if (err)
		return err;

	err = write_cpld_register(REG_OSFP_EN_LED_0, (uint8_t)value);

	return (err == 0) ? count : -EIO;
}

static DEVICE_ATTR_RW(osfp_en_led_0);

static ssize_t osfp_en_led_1_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	uint8_t value;
	int ret;

	ret = read_cpld_register(REG_OSFP_EN_LED_1, &value);

	if (ret < 0)
		return ret;

	return sprintf(buf, "0x%02x\n", value);
}

static ssize_t osfp_en_led_1_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long value;
	int err;

	err = kstrtoul(buf, 0, &value);
	if (err)
		return err;

	err = write_cpld_register(REG_OSFP_EN_LED_1, (uint8_t)value);

	return (err == 0) ? count : -EIO;
}

static DEVICE_ATTR_RW(osfp_en_led_1);

static ssize_t osfp_en_led_2_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	uint8_t value;
	int ret;

	ret = read_cpld_register(REG_OSFP_EN_LED_2, &value);

	if (ret < 0)
		return ret;

	return sprintf(buf, "0x%02x\n", value);
}

static ssize_t osfp_en_led_2_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long value;
	int err;

	err = kstrtoul(buf, 0, &value);
	if (err)
		return err;

	err = write_cpld_register(REG_OSFP_EN_LED_2, (uint8_t)value);

	return (err == 0) ? count : -EIO;
}

static DEVICE_ATTR_RW(osfp_en_led_2);

static ssize_t osfp_en_led_3_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	uint8_t value;
	int ret;

	ret = read_cpld_register(REG_OSFP_EN_LED_3, &value);

	if (ret < 0)
		return ret;

	return sprintf(buf, "0x%02x\n", value);
}

static ssize_t osfp_en_led_3_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long value;
	int err;

	err = kstrtoul(buf, 0, &value);
	if (err)
		return err;

	err = write_cpld_register(REG_OSFP_EN_LED_3, (uint8_t)value);

	return (err == 0) ? count : -EIO;
}

static DEVICE_ATTR_RW(osfp_en_led_3);

static ssize_t osfp_en_led_4_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	uint8_t value;
	int ret;

	ret = read_cpld_register(REG_OSFP_EN_LED_4, &value);

	if (ret < 0)
		return ret;

	return sprintf(buf, "0x%02x\n", value);
}

static ssize_t osfp_en_led_4_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long value;
	int err;

	err = kstrtoul(buf, 0, &value);
	if (err)
		return err;

	err = write_cpld_register(REG_OSFP_EN_LED_4, (uint8_t)value);

	return (err == 0) ? count : -EIO;
}

static DEVICE_ATTR_RW(osfp_en_led_4);

static ssize_t osfp_en_led_5_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	uint8_t value;
	int ret;

	ret = read_cpld_register(REG_OSFP_EN_LED_5, &value);

	if (ret < 0)
		return ret;

	return sprintf(buf, "0x%02x\n", value);
}

static ssize_t osfp_en_led_5_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long value;
	int err;

	err = kstrtoul(buf, 0, &value);
	if (err)
		return err;

	err = write_cpld_register(REG_OSFP_EN_LED_5, (uint8_t)value);

	return (err == 0) ? count : -EIO;
}

static DEVICE_ATTR_RW(osfp_en_led_5);

static ssize_t osfp_en_led_6_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	uint8_t value;
	int ret;

	ret = read_cpld_register(REG_OSFP_EN_LED_6, &value);

	if (ret < 0)
		return ret;

	return sprintf(buf, "0x%02x\n", value);
}

static ssize_t osfp_en_led_6_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long value;
	int err;

	err = kstrtoul(buf, 0, &value);
	if (err)
		return err;

	err = write_cpld_register(REG_OSFP_EN_LED_6, (uint8_t)value);

	return (err == 0) ? count : -EIO;
}

static DEVICE_ATTR_RW(osfp_en_led_6);


static ssize_t osfp_en_led_7_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	uint8_t value;
	int ret;

	ret = read_cpld_register(REG_OSFP_EN_LED_7, &value);

	if (ret < 0)
		return ret;

	return sprintf(buf, "0x%02x\n", value);
}

static ssize_t osfp_en_led_7_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long value;
	int err;

	err = kstrtoul(buf, 0, &value);
	if (err)
		return err;

	err = write_cpld_register(REG_OSFP_EN_LED_7, (uint8_t)value);

	return (err == 0) ? count : -EIO;
}

static DEVICE_ATTR_RW(osfp_en_led_7);

// QSFP present
static ssize_t qsfp_present_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t value;
	int ret;

	ret = read_cpld_register(REG_QSFP_STATUS, &value);

	if (ret < 0)
		return ret;

	return sprintf(buf, "%d\n", (value >> BITPOS_QSFP_PRESENT) & 0x01);
}

struct device_attribute dev_attr_qsfp_present = __ATTR(qsfp_present, 0400, qsfp_present_show, NULL);

/* QSFP enable LED flag */
static ssize_t qsfp_en_led_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t value;
	int ret;

	mutex_lock(&cpld_dev_data->lock);
	ret = read_cpld_register(REG_QSFP_STATUS, &value);
	mutex_unlock(&cpld_dev_data->lock);

	if (ret < 0)
		return ret;

	return sprintf(buf, "%d\n", (value >> BITPOS_QSFP_EN_LED) & 0x01);
}

static ssize_t qsfp_en_led_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long state;
	uint8_t reg;
	int err;

	err = kstrtoul(buf, 0, &state);
	if (err)
		return err;

	mutex_lock(&cpld_dev_data->lock);
	err = read_cpld_register(REG_QSFP_STATUS, &reg);
	if (!err) {
		if (state)
			reg |= (1 << BITPOS_QSFP_EN_LED);
		else
			reg &= ~(1 << BITPOS_QSFP_EN_LED);

		err = write_cpld_register(REG_QSFP_STATUS, reg);
	}
	mutex_unlock(&cpld_dev_data->lock);

	return (err == 0) ? count : -EIO;
}
static DEVICE_ATTR(qsfp_en_led, 0600, qsfp_en_led_show, qsfp_en_led_store);

// Reset Switch Asic
static ssize_t reset_switch_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t reg;
	int ret;

	mutex_lock(&cpld_dev_data->lock);
	ret = read_cpld_register(REG_DEV_RST_CTRL, &reg);
	mutex_unlock(&cpld_dev_data->lock);

	if (ret < 0)
		return ret;

	return sprintf(buf, "%d\n", (reg >> BITPOS_RSTN_SWITCH) & 0x01);
}

static ssize_t reset_switch_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long state;
	uint8_t reg;
	int err;

	err = kstrtoul(buf, 0, &state);
	if (err)
		return err;

	mutex_lock(&cpld_dev_data->lock);
	err = read_cpld_register(REG_DEV_RST_CTRL, &reg);
	if (!err) {
		if (state)
			reg |= (1 << BITPOS_RSTN_SWITCH);
		else
			reg &= ~(1 << BITPOS_RSTN_SWITCH);

		err = write_cpld_register(REG_DEV_RST_CTRL, reg);
	}
	mutex_unlock(&cpld_dev_data->lock);

	return (err == 0) ? count : -EIO;
}

struct device_attribute dev_attr_reset_switch = __ATTR(reset_switch, 0600, reset_switch_show, reset_switch_store);

// Switch Asic Powerdown
static ssize_t switch_powerdown_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t reg;
	int ret;

	mutex_lock(&cpld_dev_data->lock);
	ret = read_cpld_register(REG_DEV_RST_CTRL, &reg);
	mutex_unlock(&cpld_dev_data->lock);

	if (ret < 0)
		return ret;

	return sprintf(buf, "%d\n", (reg >> BITPOS_SWITCH_PWRDOWN_N) & 0x01);
}

static ssize_t switch_powerdown_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long state;
	uint8_t reg;
	int err;

	err = kstrtoul(buf, 0, &state);
	if (err)
		return err;

	mutex_lock(&cpld_dev_data->lock);
	err = read_cpld_register(REG_DEV_RST_CTRL, &reg);
	if (!err) {
		if (state)
			reg |= (1 << BITPOS_SWITCH_PWRDOWN_N);
		else
			reg &= ~(1 << BITPOS_SWITCH_PWRDOWN_N);

		err = write_cpld_register(REG_DEV_RST_CTRL, reg);
	}
	mutex_unlock(&cpld_dev_data->lock);

	return (err == 0) ? count : -EIO;
}

struct device_attribute dev_attr_switch_powerdown = __ATTR(switch_powerdown, 0600, switch_powerdown_show, switch_powerdown_store);

// Switch UART control
static ssize_t switch_uart_control_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t value;
	int ret;

	mutex_lock(&cpld_dev_data->lock);
	ret = read_cpld_register(REG_UART_CONTROL, &value);
	mutex_unlock(&cpld_dev_data->lock);

	if (ret < 0)
		return ret;

	return sprintf(buf, "0x%02x\n", value);
}

static ssize_t switch_uart_control_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long value;
	int err;

	err = kstrtoul(buf, 0, &value);
	if (err)
		return err;

	mutex_lock(&cpld_dev_data->lock);
	err = write_cpld_register(REG_UART_CONTROL, (u8)value);
	mutex_unlock(&cpld_dev_data->lock);

	return (err == 0) ? count : -EIO;
}

struct device_attribute dev_attr_switch_uart_control = __ATTR(switch_uart_control, 0600, switch_uart_control_show, switch_uart_control_store);

// Testee
static ssize_t testee_offset_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%02x\n", testee_offset);
}

static ssize_t testee_offset_store(struct device *dev, struct device_attribute *attr, const char *buf,
					size_t count)
{
	ssize_t status = 0;
	uint8_t data_byte;

	status = kstrtou8(buf, 0, &data_byte);
	if (status == 0) {
		testee_offset = data_byte;
		status = count;
	}

	return status;
}
struct device_attribute dev_attr_testee_offset =
	__ATTR(testee_offset, 0600, testee_offset_show, testee_offset_store);

static ssize_t testee_value_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t data_byte;
	int err;

	mutex_lock(&cpld_dev_data->lock);
	err = read_cpld_register(testee_offset, &data_byte);
	mutex_unlock(&cpld_dev_data->lock);

	if (err < 0)
		return err;

	return sprintf(buf, "0x%02x\n", data_byte);
}

static ssize_t testee_value_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	uint8_t data_byte;
	ssize_t status;

	status = kstrtou8(buf, 0, &data_byte);
	if (status == 0) {
		mutex_lock(&cpld_dev_data->lock);
		status = write_cpld_register(testee_offset, data_byte) == 0 ? count : -EIO;
		mutex_unlock(&cpld_dev_data->lock);
	}

	return status;
}

struct device_attribute dev_attr_testee_value =
	__ATTR(testee_value, 0600, testee_value_show, testee_value_store);

static struct attribute *cpld_attrs[] = {
	&dev_attr_cpld1_board_id.attr,
	&dev_attr_cpld1_id.attr,
	&dev_attr_cpld1_version.attr,
	&dev_attr_cpld2_board_id.attr,
	&dev_attr_cpld2_id.attr,
	&dev_attr_cpld2_version.attr,
	&dev_attr_cpld2_event_clr.attr,
	&dev_attr_cpld2_event_flag.attr,
	&dev_attr_cpld2_intr.attr,
	&dev_attr_cpld2_pfr_rev_id1.attr,
	&dev_attr_cpld2_pfr_rev_id2.attr,
	&dev_attr_cpld2_pfr_rev_id3.attr,
	&dev_attr_cpld3_board_id.attr,
	&dev_attr_cpld3_id.attr,
	&dev_attr_cpld3_version.attr,
	&dev_attr_cpld3_event_clr.attr,
	&dev_attr_cpld3_event_flag.attr,
	&dev_attr_cpld3_intr.attr,
	&dev_attr_cpld3_pfr_rev_id1.attr,
	&dev_attr_cpld3_pfr_rev_id2.attr,
	&dev_attr_cpld3_pfr_rev_id3.attr,
	&dev_attr_jed_rel.attr,
	&dev_attr_led_debug.attr,
	&dev_attr_model_id.attr,
	&dev_attr_osfp_efuse_fault_n_1.attr,
	&dev_attr_osfp_efuse_fault_n_2.attr,
	&dev_attr_osfp_efuse_powergood_1.attr,
	&dev_attr_osfp_efuse_powergood_2.attr,
	&dev_attr_osfp_en_led_0.attr,
	&dev_attr_osfp_en_led_1.attr,
	&dev_attr_osfp_en_led_2.attr,
	&dev_attr_osfp_en_led_3.attr,
	&dev_attr_osfp_en_led_4.attr,
	&dev_attr_osfp_en_led_5.attr,
	&dev_attr_osfp_en_led_6.attr,
	&dev_attr_osfp_en_led_7.attr,
	&dev_attr_qsfp_present.attr,
	&dev_attr_qsfp_en_led.attr,
	&dev_attr_reset_switch.attr,
	&dev_attr_switch_powerdown.attr,
	&dev_attr_switch_uart_control.attr,
	&dev_attr_testee_offset.attr,
	&dev_attr_testee_value.attr,
	NULL,
};

static const struct attribute_group cpld_attr_group = {
	.attrs = cpld_attrs,
};

/* SFF device attributes */

static const int osfp_intr_port_lookup[64][2] = {
	{0, 7}, {0, 6}, {0, 5}, {0, 4}, // Port 1, 2, 3, 4
	{0, 3}, {0, 2}, {0, 1}, {0, 0}, // Port 5, 6, 7, 8
	{1, 7}, {1, 6}, {1, 5}, {1, 4}, // Port 9, 10, 11, 12
	{1, 3}, {1, 2}, {1, 1}, {1, 0}, // Port 13, 14, 15, 16
	{2, 7}, {2, 6}, {2, 5}, {2, 4}, // Port 17, 18, 19, 20
	{2, 3}, {2, 2}, {2, 1}, {2, 0}, // Port 21, 22, 23, 24
	{3, 7}, {3, 6}, {3, 5}, {3, 4}, // Port 25, 26, 27, 28
	{3, 3}, {3, 2}, {3, 1}, {3, 0}, // Port 29, 30, 31, 32
	{7, 0}, {7, 1}, {7, 2}, {7, 3}, // Port 33, 34, 35, 36
	{7, 4}, {7, 5}, {7, 6}, {7, 7}, // Port 37, 38, 39, 40
	{6, 0}, {6, 1}, {6, 2}, {6, 3}, // Port 41, 42, 43, 44
	{6, 4}, {6, 5}, {6, 6}, {6, 7}, // Port 45, 46, 47, 48
	{5, 0}, {5, 1}, {5, 2}, {5, 3}, // Port 49, 50, 51, 52
	{5, 4}, {5, 5}, {5, 6}, {5, 7}, // Port 53, 54, 55, 56
	{4, 0}, {4, 1}, {4, 2}, {4, 3}, // Port 57, 58, 59, 60
	{4, 4}, {4, 5}, {4, 6}, {4, 7}  // Port 61, 62, 63, 64
};

static ssize_t osfp_intr_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cpld_port_data *port_data = dev_get_drvdata(dev);
	u8 reg_addr;
	uint8_t value;
	int err, bit_position;

	if (!port_data)
		return -ENODEV;

	reg_addr = REG_OSFP_INTR_0 + osfp_intr_port_lookup[port_data->port_id-1][0];

	err = read_cpld_register(reg_addr, &value);
	if (err < 0)
		return err;

	bit_position = osfp_intr_port_lookup[port_data->port_id-1][1];

	value = (value >> bit_position) & 0x01;

	return sprintf(buf, "%d\n", value);
}
DEVICE_ATTR_RO(osfp_intr);

static const int osfp_lpmode_port_lookup[64][2] = {
	{0, 7}, {0, 6}, {0, 5}, {0, 4}, // Port 1, 2, 3, 4
	{0, 3}, {0, 2}, {0, 1}, {0, 0}, // Port 5, 6, 7, 8
	{1, 7}, {1, 6}, {1, 5}, {1, 4}, // Port 9, 10, 11, 12
	{1, 3}, {1, 2}, {1, 1}, {1, 0}, // Port 13, 14, 15, 16
	{2, 7}, {2, 6}, {2, 5}, {2, 4}, // Port 17, 18, 19, 20
	{2, 3}, {2, 2}, {2, 1}, {2, 0}, // Port 21, 22, 23, 24
	{3, 7}, {3, 6}, {3, 5}, {3, 4}, // Port 25, 26, 27, 28
	{3, 3}, {3, 2}, {3, 1}, {3, 0}, // Port 29, 30, 31, 32
	{7, 0}, {7, 1}, {7, 2}, {7, 3}, // Port 33, 34, 35, 36
	{7, 4}, {7, 5}, {7, 6}, {7, 7}, // Port 37, 38, 39, 40
	{6, 0}, {6, 1}, {6, 2}, {6, 3}, // Port 41, 42, 43, 44
	{6, 4}, {6, 5}, {6, 6}, {6, 7}, // Port 45, 46, 47, 48
	{5, 0}, {5, 1}, {5, 2}, {5, 3}, // Port 49, 50, 51, 52
	{5, 4}, {5, 5}, {5, 6}, {5, 7}, // Port 53, 54, 55, 56
	{4, 0}, {4, 1}, {4, 2}, {4, 3}, // Port 57, 58, 59, 60
	{4, 4}, {4, 5}, {4, 6}, {4, 7}  // Port 61, 62, 63, 64
};

static ssize_t osfp_lpmode_n_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cpld_port_data *port_data = dev_get_drvdata(dev);
	u8 reg_addr;
	uint8_t value;
	int err, bit_position;

	if (!port_data)
		return -ENODEV;

	reg_addr = REG_OSFP_LPW_N_0 + osfp_lpmode_port_lookup[port_data->port_id-1][0];

	err = read_cpld_register(reg_addr, &value);
	if (err < 0)
		return err;

	bit_position = osfp_lpmode_port_lookup[port_data->port_id-1][1];

	value = (value >> bit_position) & 0x01;

	return sprintf(buf, "%d\n", value);
}

static ssize_t osfp_lpmode_n_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct cpld_port_data *port_data = dev_get_drvdata(dev);
	u8 reg_addr;
	uint8_t value;
	int err, bit_position;
	unsigned long set;

	if (!port_data)
		return -ENODEV;

	/* parse user input: only allow 0 or 1 */
	err = kstrtoul(buf, 0, &set);
	if (err)
		return err;

	if (set != 0 && set != 1)
		return -EINVAL;

	reg_addr = REG_OSFP_LPW_N_0 + osfp_lpmode_port_lookup[port_data->port_id - 1][0];
	bit_position = osfp_lpmode_port_lookup[port_data->port_id - 1][1];

	/* read-modify-write */
	mutex_lock(&cpld_dev_data->lock);
	err = read_cpld_register(reg_addr, &value);
	if (err < 0) {
		mutex_unlock(&cpld_dev_data->lock);
		return err;
	}

	if (set)
		value |= (1 << bit_position);
	else
		value &= ~(1 << bit_position);

	err = write_cpld_register(reg_addr, value);
	mutex_unlock(&cpld_dev_data->lock);

	if (err < 0)
		return err;

	return count;
}

DEVICE_ATTR_RW(osfp_lpmode_n);

static const int osfp_modprs_port_lookup[64][2] = {
	{0, 7}, {0, 6}, {0, 5}, {0, 4}, // Port 1, 2, 3, 4
	{0, 3}, {0, 2}, {0, 1}, {0, 0}, // Port 5, 6, 7, 8
	{1, 7}, {1, 6}, {1, 5}, {1, 4}, // Port 9, 10, 11, 12
	{1, 3}, {1, 2}, {1, 1}, {1, 0}, // Port 13, 14, 15, 16
	{2, 7}, {2, 6}, {2, 5}, {2, 4}, // Port 17, 18, 19, 20
	{2, 3}, {2, 2}, {2, 1}, {2, 0}, // Port 21, 22, 23, 24
	{3, 7}, {3, 6}, {3, 5}, {3, 4}, // Port 25, 26, 27, 28
	{3, 3}, {3, 2}, {3, 1}, {3, 0}, // Port 29, 30, 31, 32
	{7, 0}, {7, 1}, {7, 2}, {7, 3}, // Port 33, 34, 35, 36
	{7, 4}, {7, 5}, {7, 6}, {7, 7}, // Port 37, 38, 39, 40
	{6, 0}, {6, 1}, {6, 2}, {6, 3}, // Port 41, 42, 43, 44
	{6, 4}, {6, 5}, {6, 6}, {6, 7}, // Port 45, 46, 47, 48
	{5, 0}, {5, 1}, {5, 2}, {5, 3}, // Port 49, 50, 51, 52
	{5, 4}, {5, 5}, {5, 6}, {5, 7}, // Port 53, 54, 55, 56
	{4, 0}, {4, 1}, {4, 2}, {4, 3}, // Port 57, 58, 59, 60
	{4, 4}, {4, 5}, {4, 6}, {4, 7}  // Port 61, 62, 63, 64
};

static ssize_t osfp_modprs_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cpld_port_data *port_data = dev_get_drvdata(dev);
	u8 reg_addr;
	uint8_t value;
	int err, bit_position;

	if (!port_data)
		return -ENODEV;

	reg_addr = REG_OSFP_PRESENT_0 + osfp_modprs_port_lookup[port_data->port_id-1][0];

	err = read_cpld_register(reg_addr, &value);
	if (err < 0)
		return err;

	bit_position = osfp_modprs_port_lookup[port_data->port_id-1][1];

	value = (value >> bit_position) & 0x01;

	return sprintf(buf, "%d\n", value);
}
DEVICE_ATTR_RO(osfp_modprs);

static const int osfp_reset_port_lookup[64][2] = {
	{0, 7}, {0, 6}, {0, 5}, {0, 4}, // Port 1, 2, 3, 4
	{0, 3}, {0, 2}, {0, 1}, {0, 0}, // Port 5, 6, 7, 8
	{1, 7}, {1, 6}, {1, 5}, {1, 4}, // Port 9, 10, 11, 12
	{1, 3}, {1, 2}, {1, 1}, {1, 0}, // Port 13, 14, 15, 16
	{2, 7}, {2, 6}, {2, 5}, {2, 4}, // Port 17, 18, 19, 20
	{2, 3}, {2, 2}, {2, 1}, {2, 0}, // Port 21, 22, 23, 24
	{3, 7}, {3, 6}, {3, 5}, {3, 4}, // Port 25, 26, 27, 28
	{3, 3}, {3, 2}, {3, 1}, {3, 0}, // Port 29, 30, 31, 32
	{7, 0}, {7, 1}, {7, 2}, {7, 3}, // Port 33, 34, 35, 36
	{7, 4}, {7, 5}, {7, 6}, {7, 7}, // Port 37, 38, 39, 40
	{6, 0}, {6, 1}, {6, 2}, {6, 3}, // Port 41, 42, 43, 44
	{6, 4}, {6, 5}, {6, 6}, {6, 7}, // Port 45, 46, 47, 48
	{5, 0}, {5, 1}, {5, 2}, {5, 3}, // Port 49, 50, 51, 52
	{5, 4}, {5, 5}, {5, 6}, {5, 7}, // Port 53, 54, 55, 56
	{4, 0}, {4, 1}, {4, 2}, {4, 3}, // Port 57, 58, 59, 60
	{4, 4}, {4, 5}, {4, 6}, {4, 7}  // Port 61, 62, 63, 64
};

static ssize_t osfp_reset_n_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct cpld_port_data *port_data = dev_get_drvdata(dev);
	u8 reg_addr;
	uint8_t value;
	int err, bit_position;

	if (!port_data)
		return -ENODEV;

	reg_addr = REG_OSFP_RST_N_0 +
		   osfp_reset_port_lookup[port_data->port_id - 1][0];

	err = read_cpld_register(reg_addr, &value);
	if (err < 0)
		return err;

	bit_position = osfp_reset_port_lookup[port_data->port_id - 1][1];
	value = (value >> bit_position) & 0x01;

	return sprintf(buf, "%d\n", value);
}

static ssize_t osfp_reset_n_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct cpld_port_data *port_data = dev_get_drvdata(dev);
	u8 reg_addr;
	uint8_t value;
	int err, bit_position;
	unsigned long set;

	if (!port_data)
		return -ENODEV;

	err = kstrtoul(buf, 0, &set);
	if (err)
		return err;

	if (set != 0 && set != 1)
		return -EINVAL;

	reg_addr = REG_OSFP_RST_N_0 +
		   osfp_reset_port_lookup[port_data->port_id - 1][0];
	bit_position = osfp_reset_port_lookup[port_data->port_id - 1][1];

	mutex_lock(&cpld_dev_data->lock);
	err = read_cpld_register(reg_addr, &value);
	if (err < 0) {
		mutex_unlock(&cpld_dev_data->lock);
		return err;
	}

	if (set)
		value |= (1 << bit_position);   /* deassert reset */
	else
		value &= ~(1 << bit_position);  /* assert reset */

	err = write_cpld_register(reg_addr, value);
	mutex_unlock(&cpld_dev_data->lock);

	if (err < 0)
		return err;

	return count;
}

DEVICE_ATTR_RW(osfp_reset_n);

static struct attribute *sff_osfp_attrs[] = {
	&dev_attr_osfp_intr.attr,
	&dev_attr_osfp_lpmode_n.attr,
	&dev_attr_osfp_modprs.attr,
	&dev_attr_osfp_reset_n.attr,
	NULL,
};
static struct attribute_group sff_osfp_attr_grp = {
	.attrs = sff_osfp_attrs,
};
static const struct attribute_group *sff_osfp_attr_grps[] = { &sff_osfp_attr_grp, NULL };

static int cpld_open(struct inode *inode, struct file *file)
{
	file->private_data = cpld_dev_data;
	return 0;
}

static int cpld_release(struct inode *inode, struct file *file)
{
	return 0;
}

static struct file_operations cpld_fops = {
	.owner = THIS_MODULE,
	.open = cpld_open,
	.release = cpld_release,
};

static struct device *cpld_sff_init(int port_id, int port_type, struct cpld_data *parent)
{
	struct cpld_port_data *new_data;
	struct device *new_device;
	char tmpStr[20];

	new_data = kzalloc(sizeof(*new_data), GFP_KERNEL);
	if (!new_data)
		return NULL;
	new_data->port_id = port_id;
	new_data->parent = parent;

	if (!sff_dev || IS_ERR(sff_dev)) {
		printk(KERN_ERR "%s: SFF device is not created properly.\n", __FUNCTION__);
		kfree(new_data);
		return NULL;
	}

	if (port_type == OSFP) {
		sprintf(tmpStr, "OSFP%d", new_data->port_id);
		new_device = device_create_with_groups(cpld_class, sff_dev, MKDEV(0, 0), new_data, sff_osfp_attr_grps,
							   "%s", tmpStr);
	} else {
		printk(KERN_ERR "%s: Invalid port type %d.\n", __FUNCTION__, port_type);
	}

	if (IS_ERR(new_device)) {
		printk(KERN_ERR "%s: Failed to create device for port %d.\n", __FUNCTION__, port_id);
		kfree(new_data);
		return NULL;
	}

	return new_device;
}

static void cpld_sff_deinit(int port_id, struct cpld_data *data)
{
	struct cpld_port_data *dev_data;

	dev_data = dev_get_drvdata(data->sff_devices[port_id]);
	device_unregister(data->sff_devices[port_id]);
	kfree(dev_data);
}

static int cpld_wdt_start(struct watchdog_device *wdt_dev)
{
	uint8_t val;

	mutex_lock(&cpld_dev_data->lock);
	if (read_cpld_register(REG_DEV_RST_CTRL, &val) != 0) {
		mutex_unlock(&cpld_dev_data->lock);
		return -EIO;
	}
	/* Enable watchdog */
	val |= (1 << BITPOS_WDT_EN);
	if (write_cpld_register(REG_DEV_RST_CTRL, val) != 0) {
		mutex_unlock(&cpld_dev_data->lock);
		return -EIO;
	}
	mutex_unlock(&cpld_dev_data->lock);
	return 0;
}

static int cpld_wdt_stop(struct watchdog_device *wdt_dev)
{
	uint8_t val;

	mutex_lock(&cpld_dev_data->lock);
	if (read_cpld_register(REG_DEV_RST_CTRL, &val) != 0) {
		mutex_unlock(&cpld_dev_data->lock);
		return -EIO;
	}
	val &= ~(1 << BITPOS_WDT_EN);
	if (write_cpld_register(REG_DEV_RST_CTRL, val) != 0) {
		mutex_unlock(&cpld_dev_data->lock);
		return -EIO;
	}
	mutex_unlock(&cpld_dev_data->lock);
	return 0;
}

static int cpld_wdt_set_timeout(struct watchdog_device *wdt_dev, unsigned int timeout)
{
	uint8_t val;
	int is_enabled = 0;

	mutex_lock(&cpld_dev_data->lock);
	if (read_cpld_register(REG_DEV_RST_CTRL, &val) != 0) {
		mutex_unlock(&cpld_dev_data->lock);
		return -EIO;
	}
	if (val & (1 << BITPOS_WDT_EN)) {
		is_enabled = 1;
		/* save and stop */
		if (write_cpld_register(REG_DEV_RST_CTRL, val & ~(1 << BITPOS_WDT_EN)) != 0) {
			mutex_unlock(&cpld_dev_data->lock);
			return -EIO;
		}
	}
	if (write_cpld_register(REG_WDT_MAXL, timeout & 0xFF) != 0 ||
		write_cpld_register(REG_WDT_MAXM, (timeout >> 8) & 0xFF) != 0) {
		mutex_unlock(&cpld_dev_data->lock);
		return -EIO;
	}
	/* restore */
	if (is_enabled) {
		if (write_cpld_register(REG_DEV_RST_CTRL, val) != 0) {
			mutex_unlock(&cpld_dev_data->lock);
			return -EIO;
		}
	}
	wdt_dev->timeout = timeout;
	mutex_unlock(&cpld_dev_data->lock);
	return 0;
}

static int cpld_wdt_ping(struct watchdog_device *wdt_dev)
{
	uint8_t val;

	mutex_lock(&cpld_dev_data->lock);

	if (read_cpld_register(REG_DEV_RST_CTRL, &val) != 0) {
		mutex_unlock(&cpld_dev_data->lock);
		return -EIO;
	}
	val |= (1 << BITPOS_WDT_CLR);
	if (write_cpld_register(REG_DEV_RST_CTRL, val) != 0) {
		mutex_unlock(&cpld_dev_data->lock);
		return -EIO;
	}

	mutex_unlock(&cpld_dev_data->lock);
	return 0;
}

static unsigned int cpld_wdt_get_bootstatus(struct cpld_data *data)
{
	uint8_t val = 0;
	unsigned int bootstatus;

	mutex_lock(&cpld_dev_data->lock);
	if (read_cpld_register(REG_WDT_REC, &val) != 0) {
		mutex_unlock(&cpld_dev_data->lock);
		return -EIO;
	}

	bootstatus = (val & (1 << BITPOS_WDT_REC)) ? WDIOF_CARDRESET : 0;

	// Write-clear reboot record
	val = (1 << BITPOS_WDT_REC_CFG);
	if (write_cpld_register(REG_WDT_REC, val) != 0) {
		mutex_unlock(&cpld_dev_data->lock);
		return -EIO;
	}
	mutex_unlock(&cpld_dev_data->lock);
	return bootstatus;
}

static unsigned int cpld_wdt_get_timeleft(struct watchdog_device *wdt_dev)
{
	uint8_t cntl, cntm, maxl, maxm;
	unsigned int current_count, max_timeout;

	mutex_lock(&cpld_dev_data->lock);

	if (read_cpld_register(REG_WDT_CNTL, &cntl) != 0 || read_cpld_register(REG_WDT_CNTM, &cntm) != 0) {
		mutex_unlock(&cpld_dev_data->lock);
		return -EIO;
	}
	if (read_cpld_register(REG_WDT_MAXL, &maxl) != 0 || read_cpld_register(REG_WDT_MAXM, &maxm) != 0) {
		mutex_unlock(&cpld_dev_data->lock);
		return -EIO;
	}

	mutex_unlock(&cpld_dev_data->lock);

	current_count = ((unsigned int)cntm << 8) | cntl;
	max_timeout = ((unsigned int)maxm << 8) | maxl;

	if (current_count >= max_timeout) {
		return 0;
	} else {
		return max_timeout - current_count;
	}
}

#define WATCHDOG_TIMEOUT 30 /* 30 sec default heartbeat */

static struct watchdog_device *pwddev;

static const struct watchdog_info cpld_wdt_info = {
	.options = WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING | WDIOF_MAGICCLOSE,
	.identity = "CPLD1 Watchdog",
};

static const struct watchdog_ops cpld_wdt_ops = {
	.owner = THIS_MODULE,
	.start = cpld_wdt_start,
	.stop = cpld_wdt_stop,
	.ping = cpld_wdt_ping,
	.set_timeout = cpld_wdt_set_timeout,
	.get_timeleft = cpld_wdt_get_timeleft,
};

static int __init cpld_init(void)
{
	int port_count = 0;
	int ret;
	dev_t dev;

	cpld_dev_data = kzalloc(sizeof(*cpld_dev_data), GFP_KERNEL);
	if (!cpld_dev_data)
		return -ENOMEM;

	mutex_init(&cpld_dev_data->lock);

	if ((ret = alloc_chrdev_region(&dev, 0, 1, "cpld_device")) < 0)
		goto fail_alloc_chrdev;

	cpld_major = MAJOR(dev);
	cdev_init(&cpld_dev_data->cdev, &cpld_fops);
	cpld_dev_data->cdev.owner = THIS_MODULE;
	if ((ret = cdev_add(&cpld_dev_data->cdev, dev, 1)) < 0)
		goto fail_cdev_add;

	cpld_class = class_create("CPLD1");
	if (IS_ERR(cpld_class)) {
		ret = PTR_ERR(cpld_class);
		goto fail_class_create;
	}

	cpld_dev_data->cpld_device = device_create(cpld_class, NULL, dev, NULL, "cpld_device");
	if (IS_ERR(cpld_dev_data->cpld_device)) {
		ret = PTR_ERR(cpld_dev_data->cpld_device);
		goto fail_device_create;
	}

	ret = sysfs_create_group(&cpld_dev_data->cpld_device->kobj, &cpld_attr_group);
	if (ret)
		goto fail_sysfs_create_group;

	/* SFF */
	sff_dev = device_create(cpld_class, NULL, MKDEV(0, 0), NULL, "sff_device");
	if (IS_ERR(sff_dev)) {
		ret = PTR_ERR(sff_dev);
		printk(KERN_ERR "%s: Failed to create SFF device: %d\n", __FUNCTION__, ret);
		goto fail_sff_dev_create;
	}

	for (port_count = 0; port_count < OSFP_PORT_TOTAL; port_count++) {
		cpld_dev_data->sff_devices[port_count] = cpld_sff_init(port_count + 1, OSFP, cpld_dev_data);
		if (!cpld_dev_data->sff_devices[port_count]) {
			ret = -ENODEV;
			goto fail_sff_init;
		}
	}

	/* Watchdog */
	pwddev = kzalloc(sizeof(*pwddev), GFP_KERNEL);
	if (!pwddev) {
	pr_err("Failed to allocate memory for watchdog device\n");
	return -ENOMEM;
	}

	pwddev->info = &cpld_wdt_info;
	pwddev->ops = &cpld_wdt_ops;
	pwddev->bootstatus = cpld_wdt_get_bootstatus(cpld_dev_data);
	pwddev->timeout = WATCHDOG_TIMEOUT;
	pwddev->parent = cpld_dev_data->cpld_device;
	pwddev->min_timeout = 1;
	pwddev->max_timeout = 0xffff;

	ret = watchdog_register_device(pwddev);
	if (ret) {
		dev_err(cpld_dev_data->cpld_device, "Cannot register watchdog device (err=%d)\n", ret);
		kfree(pwddev);
		pwddev = NULL;
		goto fail_sff_init;
	} else {
		cpld_wdt_stop(pwddev);
		cpld_wdt_set_timeout(pwddev, WATCHDOG_TIMEOUT);
		dev_info(cpld_dev_data->cpld_device, "Watchdog initialized. heartbeat=%d sec (nowayout=%d)\n",
			pwddev->timeout, 0);
	}

	return 0;

fail_sff_init:
	while (port_count-- > 0)
		cpld_sff_deinit(port_count, cpld_dev_data);

	device_destroy(cpld_class, sff_dev->devt);

fail_sff_dev_create:
	sysfs_remove_group(&cpld_dev_data->cpld_device->kobj, &cpld_attr_group);

fail_sysfs_create_group:
	device_destroy(cpld_class, cpld_dev_data->cpld_device->devt);

fail_device_create:
	class_destroy(cpld_class);

fail_class_create:
	cdev_del(&cpld_dev_data->cdev);

fail_cdev_add:
	unregister_chrdev_region(dev, 1);

fail_alloc_chrdev:
	mutex_destroy(&cpld_dev_data->lock);
	kfree(cpld_dev_data);
	return ret;
}

static void __exit cpld_exit(void)
{
	int port_count;
	dev_t dev = MKDEV(cpld_major, 0);

	if (pwddev) {
		watchdog_unregister_device(pwddev);
		pr_err("%s: Watchdog device unregistered\n", KBUILD_MODNAME);
		kfree(pwddev);
		pwddev = NULL;
	}

	if (sff_dev) {
		for (port_count = 0; port_count < OSFP_PORT_TOTAL; port_count++) {
			if (cpld_dev_data->sff_devices[port_count])
				cpld_sff_deinit(port_count, cpld_dev_data);
		}
		device_destroy(cpld_class, sff_dev->devt);
	}

	if (cpld_dev_data->cpld_device) {
		sysfs_remove_group(&cpld_dev_data->cpld_device->kobj, &cpld_attr_group);
		device_destroy(cpld_class, cpld_dev_data->cpld_device->devt);
	}

	if (cpld_class)
		class_destroy(cpld_class);

	if (cpld_dev_data) {
		cdev_del(&cpld_dev_data->cdev);
		mutex_destroy(&cpld_dev_data->lock);
		kfree(cpld_dev_data);
	}

	unregister_chrdev_region(dev, 1);
}

module_init(cpld_init);
module_exit(cpld_exit);

MODULE_DESCRIPTION("SuperMicro T8264 Switchboard CPLD1 Driver");
MODULE_LICENSE("GPL");

