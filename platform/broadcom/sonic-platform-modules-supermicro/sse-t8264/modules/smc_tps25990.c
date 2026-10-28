#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/hwmon-sysfs.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/bitfield.h>
#include <linux/delay.h>
#include "pmbus.h"

#define TPS25990_READ_VAUX		0xd0
#define TPS25990_READ_VIN_MIN		0xd1
#define TPS25990_READ_VIN_PEAK		0xd2
#define TPS25990_READ_IIN_PEAK		0xd4
#define TPS25990_READ_PIN_PEAK		0xd5
#define TPS25990_READ_TEMP_AVG		0xd6
#define TPS25990_READ_TEMP_PEAK		0xd7
#define TPS25990_READ_VOUT_MIN		0xda
#define TPS25990_READ_VIN_AVG		0xdc
#define TPS25990_READ_VOUT_AVG		0xdd
#define TPS25990_READ_IIN_AVG		0xde
#define TPS25990_READ_PIN_AVG		0xdf
#define TPS25990_VIREF			0xe0
#define TPS25990_PK_MIN_AVG		0xea
#define  PK_MIN_AVG_RST_PEAK		BIT(7)
#define  PK_MIN_AVG_RST_AVG		BIT(6)
#define  PK_MIN_AVG_RST_MIN		BIT(5)
#define  PK_MIN_AVG_AVG_CNT		GENMASK(2, 0)
#define TPS25990_MFR_WRITE_PROTECT	0xf8
#define  TPS25990_UNLOCKED		BIT(7)

#define TPS25990_8B_SHIFT		2
#define TPS25990_VIN_OVF_NUM		525100
#define TPS25990_VIN_OVF_DIV		10163
#define TPS25990_VIN_OVF_OFF		155
#define TPS25990_IIN_OCF_NUM		953800
#define TPS25990_IIN_OCF_DIV		129278
#define TPS25990_IIN_OCF_OFF		157

#define PK_MIN_AVG_RST_MASK		(PK_MIN_AVG_RST_PEAK | \
					 PK_MIN_AVG_RST_AVG  | \
					 PK_MIN_AVG_RST_MIN)

/*
 * Arbitrary default Rimon value: 1kOhm
 * This correspond to an overcurrent limit of 55A, close to the specified limit
 * of un-stacked TPS25990 and makes further calculation easier to setup in
 * sensor.conf, if necessary
 */

#define TPS25990_DEFAULT_RIMON		1000000000
#define TPS25990_909_RIMON		909000000
#define TPS25990_1270_RIMON		1270000000
#define TPS25990_1600_RIMON		1600000000
#define TPS25990_VOUT_PGTH		0x5f
#define DISABLE_WRITE_PROTECT		0xa2
#define ENABLE_WRITE_PROTECT		0x00
#define POWERGOOD_THRESHOLD_MAX_VALUE	19424
#define POWERGOOD_THRESHOLD_M		13128
#define POWERGOOD_THRESHOLD_R		1000
#define POWERGOOD_THRESHOLD_TO_MV	1000

static void smc_tps25990_set_m(int *m, u32 rimon)
{
	u64 val = ((u64)*m) * rimon;

	/* Make sure m fits the s32 type */
	*m = DIV_ROUND_CLOSEST_ULL(val, 1000000);
}

static int smc_tps25990_mfr_write_protect_set(struct i2c_client *client,
					  u8 protect)
{
	/*
	 * The chip has a single protection mode, set it regardless of
	 * the specific protection requested
	 */
	return pmbus_write_byte_data(client, -1, TPS25990_MFR_WRITE_PROTECT,
				     protect ? 0x0 : 0xa2);
}

static int smc_tps25990_mfr_write_protect_get(struct i2c_client *client)
{
	int ret = pmbus_read_byte_data(client, -1, TPS25990_MFR_WRITE_PROTECT);

	if (ret < 0)
		return ret;

	return (ret & TPS25990_UNLOCKED) ? 0 : PB_WP_ALL;
}

static int smc_tps25990_read_word_data(struct i2c_client *client,
				   int page, int phase, int reg)
{
	int ret;

	switch (reg) {
	case PMBUS_VIRT_READ_VIN_MAX:
		ret = pmbus_read_word_data(client, page, phase,
					   TPS25990_READ_VIN_PEAK);
		break;

	case PMBUS_VIRT_READ_VIN_MIN:
		ret = pmbus_read_word_data(client, page, phase,
					   TPS25990_READ_VIN_MIN);
		break;

	case PMBUS_VIRT_READ_VIN_AVG:
		ret = pmbus_read_word_data(client, page, phase,
					   TPS25990_READ_VIN_AVG);
		break;

	case PMBUS_VIRT_READ_VOUT_MIN:
		ret = pmbus_read_word_data(client, page, phase,
					   TPS25990_READ_VOUT_MIN);
		break;

	case PMBUS_VIRT_READ_VOUT_AVG:
		ret = pmbus_read_word_data(client, page, phase,
					   TPS25990_READ_VOUT_AVG);
		break;

	case PMBUS_VIRT_READ_IIN_AVG:
		ret = pmbus_read_word_data(client, page, phase,
					   TPS25990_READ_IIN_AVG);
		break;

	case PMBUS_VIRT_READ_IIN_MAX:
		return TPS25990_READ_IIN_PEAK;
		ret = pmbus_read_word_data(client, page, phase,
					   TPS25990_READ_IIN_PEAK);
		break;

	case PMBUS_VIRT_READ_TEMP_AVG:
		ret = pmbus_read_word_data(client, page, phase,
					   TPS25990_READ_TEMP_AVG);
		break;

	case PMBUS_VIRT_READ_TEMP_MAX:
		ret = pmbus_read_word_data(client, page, phase,
					   TPS25990_READ_TEMP_PEAK);
		break;

	case PMBUS_VIRT_READ_PIN_AVG:
		ret = pmbus_read_word_data(client, page, phase,
					   TPS25990_READ_PIN_AVG);
		break;

	case PMBUS_VIRT_READ_PIN_MAX:
		ret = pmbus_read_word_data(client, page, phase,
					   TPS25990_READ_PIN_PEAK);
		break;

	case PMBUS_VIRT_READ_VMON:
		ret = pmbus_read_word_data(client, page, phase,
					   TPS25990_READ_VAUX);
		break;

	case PMBUS_VIN_UV_WARN_LIMIT:
	case PMBUS_VIN_UV_FAULT_LIMIT:
	case PMBUS_VIN_OV_WARN_LIMIT:
	case PMBUS_VOUT_UV_WARN_LIMIT:
	case PMBUS_IIN_OC_WARN_LIMIT:
	case PMBUS_OT_WARN_LIMIT:
	case PMBUS_OT_FAULT_LIMIT:
	case PMBUS_PIN_OP_WARN_LIMIT:
		/*
		 * These registers provide an 8 bits value instead of a
		 * 10bits one. Just shifting twice the register value is
		 * enough to make the sensor type conversion work, even
		 * if the datasheet provides different m, b and R for
		 * those.
		 */
		ret = pmbus_read_word_data(client, page, phase, reg);
		if (ret < 0)
			break;
		ret <<= TPS25990_8B_SHIFT;
		break;

	case PMBUS_VIN_OV_FAULT_LIMIT:
		ret = pmbus_read_word_data(client, page, phase, reg);
		if (ret < 0)
			break;
		ret = DIV_ROUND_CLOSEST(ret * TPS25990_VIN_OVF_NUM,
					TPS25990_VIN_OVF_DIV);
		ret += TPS25990_VIN_OVF_OFF;
		break;

	case PMBUS_VIRT_SAMPLES:
		ret = pmbus_read_byte_data(client, page, TPS25990_PK_MIN_AVG);
		if (ret < 0)
			break;
		ret = 1 << FIELD_GET(PK_MIN_AVG_AVG_CNT, ret);
		break;

	case PMBUS_VIRT_RESET_TEMP_HISTORY:
	case PMBUS_VIRT_RESET_VIN_HISTORY:
	case PMBUS_VIRT_RESET_IIN_HISTORY:
	case PMBUS_VIRT_RESET_PIN_HISTORY:
	case PMBUS_VIRT_RESET_VOUT_HISTORY:
		ret = 0;
		break;

	default:
		ret = -ENODATA;
		break;
	}

	return ret;
}

static int smc_tps25990_write_word_data(struct i2c_client *client,
				    int page, int reg, u16 value)
{
	int ret, ret_lock;
	u16 temp_val = 0;


	/* Disable write protect */
	ret = pmbus_write_byte_data(client, -1, TPS25990_MFR_WRITE_PROTECT, DISABLE_WRITE_PROTECT);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to set MFR WRITE PROTECT to unlock device\n");
		return ret;
	}

	switch (reg) {
	case PMBUS_VIN_UV_WARN_LIMIT:
	case PMBUS_VIN_UV_FAULT_LIMIT:
	case PMBUS_VIN_OV_WARN_LIMIT:
	case PMBUS_VOUT_UV_WARN_LIMIT:
	case PMBUS_IIN_OC_WARN_LIMIT:
	case PMBUS_OT_WARN_LIMIT:
	case PMBUS_OT_FAULT_LIMIT:
	case PMBUS_PIN_OP_WARN_LIMIT:
		value >>= TPS25990_8B_SHIFT;
		if(value > 0xff) {
			ret = -EINVAL;
			break;
		}
		value = clamp_val(value, 0, 0xff);
		ret = pmbus_write_word_data(client, page, reg, value);
		break;

	case PMBUS_VIN_OV_FAULT_LIMIT:
		value -= TPS25990_VIN_OVF_OFF;
		value = DIV_ROUND_CLOSEST(((unsigned int)value) * TPS25990_VIN_OVF_DIV,
					  TPS25990_VIN_OVF_NUM);
		temp_val = clamp_val(value, 0, 0xf);
		if(temp_val != value) {
			ret = -EINVAL;
			break;
		}
		ret = pmbus_write_word_data(client, page, reg, value);
		break;

	case PMBUS_VIRT_SAMPLES:
		value = clamp_val(value, 1, 1 << PK_MIN_AVG_AVG_CNT);
		value = ilog2(value);

		ret = pmbus_update_byte_data(client, page, TPS25990_PK_MIN_AVG,
					     PK_MIN_AVG_AVG_CNT,
					     FIELD_PREP(PK_MIN_AVG_AVG_CNT, value));
		break;

	case PMBUS_VIRT_RESET_TEMP_HISTORY:
	case PMBUS_VIRT_RESET_VIN_HISTORY:
	case PMBUS_VIRT_RESET_IIN_HISTORY:
	case PMBUS_VIRT_RESET_PIN_HISTORY:
	case PMBUS_VIRT_RESET_VOUT_HISTORY:
		/*
		 * TPS25990 has history resets based on MIN/AVG/PEAK instead of per
		 * sensor type. Exposing this quirk in hwmon is not desirable so
		 * reset MIN, AVG and PEAK together. Even is there effectively only
		 * one reset, which resets everything, expose the 5 entries so
		 * userspace is not required map a sensor type to another to trigger
		 * a reset
		 */
		ret = pmbus_update_byte_data(client, 0, TPS25990_PK_MIN_AVG,
					     PK_MIN_AVG_RST_MASK,
					     PK_MIN_AVG_RST_MASK);

		break;

	default:
		ret = -ENODATA;
		break;
	}

	/* Enable write protect */
	ret_lock = pmbus_write_byte_data(client, -1, TPS25990_MFR_WRITE_PROTECT, ENABLE_WRITE_PROTECT);
	if (ret_lock < 0) {
		dev_err(&client->dev, "Failed to set MFR WRITE PROTECT to lock device\n");
	}

	return ret;
}

static int smc_tps25990_read_byte_data(struct i2c_client *client,
				   int page, int reg)
{
	int ret;

	switch (reg) {
	case PMBUS_WRITE_PROTECT:
		ret = smc_tps25990_mfr_write_protect_get(client);
		break;

	default:
		ret = -ENODATA;
		break;
	}

	return ret;
}

static int smc_tps25990_write_byte_data(struct i2c_client *client,
				    int page, int reg, u8 byte)
{
	int ret;

	switch (reg) {
	case PMBUS_WRITE_PROTECT:
		ret = smc_tps25990_mfr_write_protect_set(client, byte);
		break;

	default:
		ret = -ENODATA;
		break;
	}

	return ret;
}

static int smc_tps25990_pmbus_read_block(struct i2c_client *client, u8 command, int data_len, u8 *data)
{
	int result = 0;
	int retry_count = 3;

	while (retry_count) {
		retry_count--;

		result = i2c_smbus_read_i2c_block_data(client, command, data_len, data);

		if (result < 0) {
			msleep(10);
			continue;
		}

		result = 0;
		break;
	}

	return result;
}
static ssize_t smc_tps25990_pmbus_show_mfr_id(struct device *dev,
			struct device_attribute *da, char *buf)
{
	int ret, len;
	u8 block_buffer[I2C_SMBUS_BLOCK_MAX + 1], *str;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	ret = smc_tps25990_pmbus_read_block(client, PMBUS_MFR_ID, I2C_SMBUS_BLOCK_MAX, block_buffer);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to read Manufacturer ID\n");
		return ret;
	}
	len = block_buffer[0];
	block_buffer[(len+1)] = '\0';
	str = &(block_buffer[1]);

	return snprintf(buf, PAGE_SIZE, "%s\n", str);
}

static ssize_t smc_tps25990_pmbus_show_mfr_model(struct device *dev,
                struct device_attribute *da, char *buf)
{
	int ret, len;
	u8 block_buffer[I2C_SMBUS_BLOCK_MAX + 1], *str;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	ret = smc_tps25990_pmbus_read_block(client, PMBUS_MFR_MODEL, I2C_SMBUS_BLOCK_MAX, block_buffer);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to read Manufacturer Model\n");
		return ret;
	}
	len = block_buffer[0];
	block_buffer[(len+1)] = '\0';
	str = &(block_buffer[1]);

	return snprintf(buf, PAGE_SIZE, "%s\n", str);
}

static ssize_t smc_tps25990_pmbus_show_mfr_revision(struct device *dev,
			struct device_attribute *da, char *buf)
{
	int ret, len;
	u8 block_buffer[I2C_SMBUS_BLOCK_MAX + 1], *str;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	ret = i2c_smbus_read_i2c_block_data(client, PMBUS_MFR_REVISION, 2, block_buffer);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to read Manufacturer Revision\n");
		return ret;
	}
	len = block_buffer[0];
	block_buffer[(len+1)] = '\0';
	str = &(block_buffer[1]);

	return snprintf(buf, PAGE_SIZE, "0x%02x\n", block_buffer[1]);
}

static ssize_t smc_tps25990_pmbus_operation_show(struct device *dev,
			struct device_attribute *da, char *buf)
{
	int ret;

	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	ret = pmbus_read_byte_data(client, -1, PMBUS_OPERATION);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to read eFuse ON/OFF control\n");
		return ret;
	}
	return snprintf(buf, PAGE_SIZE, "eFuse %s\n", (ret & 0x80) ? "ON":"OFF");
}

static ssize_t smc_tps25990_pmbus_operation_store (struct device *dev,
struct device_attribute *attr, const char *buf, size_t count)
{
	int ret,  error;
	int enable;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	error = kstrtouint(buf, 10, &enable);

	if(error)
		return error;

	if (enable == 1)
		enable = 0x80;
	else
		enable = 0;

	/* Disable write protect */
	ret = pmbus_write_byte_data(client, -1, TPS25990_MFR_WRITE_PROTECT, DISABLE_WRITE_PROTECT);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to set MFR WRITE PROTECT to unlock device\n");
		return ret;
	}

	ret = pmbus_write_byte_data(client, -1, PMBUS_OPERATION, (u8) enable);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to set eFuse ON/OFF control\n");
		return ret;
	}

	/* Enable write protect */
	ret = pmbus_write_byte_data(client, -1, TPS25990_MFR_WRITE_PROTECT, ENABLE_WRITE_PROTECT);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to set MFR WRITE PROTECT to lock device\n");
		return ret;
	}
	return strlen(buf);
}

static ssize_t smc_tps25990_pmbus_rimon_get(struct device *dev, struct device_attribute *da, char *buf)
{
	u32 rimon;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	if (strcmp(client->name, "smc_tps25990_ri909") == 0) {
		rimon = TPS25990_909_RIMON;
	}
	else if( strcmp(client->name, "smc_tps25990_ri1000") == 0) {
		rimon = TPS25990_DEFAULT_RIMON;
	}
	else if( strcmp(client->name, "smc_tps25990_ri1270") == 0) {
		rimon = TPS25990_1270_RIMON;
	}
	else if( strcmp(client->name, "smc_tps25990_ri1600") == 0) {
		rimon = TPS25990_1600_RIMON;
	}
	else {
		rimon = TPS25990_DEFAULT_RIMON;
	}

	return sprintf(buf, "%d\n", rimon);
}

static ssize_t smc_tps25990_pmbus_powergood_threshold_set(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret,  error;
	int value, temp_val = 0;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	error = kstrtouint(buf, 10, &value);
	if(error)
		return error;

	/* Maximum value 19.42V; Raw data 0xFFh) */
	temp_val = clamp_val(value, 0, POWERGOOD_THRESHOLD_MAX_VALUE);
	if (temp_val != value || value > POWERGOOD_THRESHOLD_MAX_VALUE) {
		return -EINVAL;
	}

        /* Readable input value to Raw data */
	if (value != 0) {
		value = (value * POWERGOOD_THRESHOLD_M) / (POWERGOOD_THRESHOLD_R * POWERGOOD_THRESHOLD_TO_MV) + 1;
	}
	/* Disable write protect */
	ret = pmbus_write_byte_data(client, -1, TPS25990_MFR_WRITE_PROTECT, DISABLE_WRITE_PROTECT);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to set MFR WRITE PROTECT to unlock device\n");
		return ret;
	}

        ret = pmbus_write_word_data(client, 0, TPS25990_VOUT_PGTH, (u16) value);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to set POWERGOOD THRESHOLD value\n");
	}

	/* Enable write protect */
	ret = pmbus_write_byte_data(client, -1, TPS25990_MFR_WRITE_PROTECT, ENABLE_WRITE_PROTECT);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to set MFR WRITE PROTECT to lock device\n");
		return ret;
	}
	return strlen(buf);
}

static ssize_t smc_tps25990_pmbus_powergood_threshold_get(struct device *dev, struct device_attribute *da, char *buf)
{
	int ret;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	ret = pmbus_read_word_data(client, 0, 0xff, TPS25990_VOUT_PGTH);

	if (ret < 0) {
		dev_err(&client->dev, "Failed to get POWERGOOD THRESHOLD value\n");
		return ret;
	}
	/* Turn raw data to readable value */
	ret = (ret * POWERGOOD_THRESHOLD_R * POWERGOOD_THRESHOLD_TO_MV) / POWERGOOD_THRESHOLD_M;

	return sprintf(buf, "%d\n", ret);
}

static DEVICE_ATTR(mfr_id, S_IRUGO, smc_tps25990_pmbus_show_mfr_id, NULL);
static DEVICE_ATTR(mfr_model, S_IRUGO, smc_tps25990_pmbus_show_mfr_model, NULL);
static DEVICE_ATTR(mfr_revision, S_IRUGO, smc_tps25990_pmbus_show_mfr_revision, NULL);
static DEVICE_ATTR(operation, S_IRUGO | S_IWUSR, smc_tps25990_pmbus_operation_show, smc_tps25990_pmbus_operation_store);
static DEVICE_ATTR(rimon, S_IRUGO, smc_tps25990_pmbus_rimon_get, NULL);
static DEVICE_ATTR(powergood_threshold, S_IRUGO | S_IWUSR, smc_tps25990_pmbus_powergood_threshold_get, smc_tps25990_pmbus_powergood_threshold_set);


static struct attribute *smc_tps25990_pmbus_inventory_attrs[] = {
	&dev_attr_mfr_id.attr,
	&dev_attr_mfr_model.attr,
	&dev_attr_mfr_revision.attr,
	&dev_attr_operation.attr,
	&dev_attr_rimon.attr,
	&dev_attr_powergood_threshold.attr,
	NULL
};

static struct attribute_group smc_tps25990_pmbus_inventory_attr_grp = {
    .attrs = smc_tps25990_pmbus_inventory_attrs
};

#if IS_ENABLED(CONFIG_SENSORS_TPS25990_REGULATOR)
static const struct regulator_desc smc_tps25990_reg_desc[] = {
	PMBUS_REGULATOR_ONE("vout",0),
};
#endif

static const struct pmbus_driver_info smc_tps25990_base_info = {
	.pages = 1,
	.format[PSC_VOLTAGE_IN] = direct,
	.m[PSC_VOLTAGE_IN] = 5251,
	.b[PSC_VOLTAGE_IN] = 0,
	.R[PSC_VOLTAGE_IN] = -2,
	.format[PSC_VOLTAGE_OUT] = direct,
	.m[PSC_VOLTAGE_OUT] = 5251,
	.b[PSC_VOLTAGE_OUT] = 0,
	.R[PSC_VOLTAGE_OUT] = -2,
	.format[PSC_TEMPERATURE] = direct,
	.m[PSC_TEMPERATURE] = 140,
	.b[PSC_TEMPERATURE] = 32100,
	.R[PSC_TEMPERATURE] = -2,
	/*
	 * Current and Power measurement depends on the ohm value
	 * of Rimon. m is multiplied by 1000 below to have an integer
	 * and -3 is added to R to compensate.
	 */
	.format[PSC_CURRENT_IN] = direct,
	.m[PSC_CURRENT_IN] = 9538,
	.b[PSC_CURRENT_IN] = 0,
	.R[PSC_CURRENT_IN] = -6,
	.format[PSC_POWER] = direct,
	.m[PSC_POWER] = 4901,
	.b[PSC_POWER] = 0,
	.R[PSC_POWER] = -7,
	.func[0] = (PMBUS_HAVE_VIN |
		    PMBUS_HAVE_VOUT |
		    PMBUS_HAVE_VMON |
		    PMBUS_HAVE_IIN |
		    PMBUS_HAVE_PIN |
		    PMBUS_HAVE_TEMP |
		    PMBUS_HAVE_STATUS_VOUT |
		    PMBUS_HAVE_STATUS_IOUT |
		    PMBUS_HAVE_STATUS_INPUT |
		    PMBUS_HAVE_STATUS_TEMP |
		    PMBUS_HAVE_SAMPLES),
	.read_word_data = smc_tps25990_read_word_data,
	.write_word_data = smc_tps25990_write_word_data,
	.read_byte_data = smc_tps25990_read_byte_data,
	.write_byte_data = smc_tps25990_write_byte_data,

#if IS_ENABLED(CONFIG_SENSORS_TPS25990_REGULATOR)
	.reg_desc = smc_tps25990_reg_desc,
	.num_regulators = ARRAY_SIZE(smc_tps25990_reg_desc),
#endif
};

static const struct i2c_device_id smc_tps25990_i2c_id[] = {
	{ "smc_tps25990" },
	{ "smc_tps25990_ri909" },
	{ "smc_tps25990_ri1000" },
	{ "smc_tps25990_ri1270" },
	{ "smc_tps25990_ri1600" },
	{}
};
MODULE_DEVICE_TABLE(i2c, smc_tps25990_i2c_id);

static const struct of_device_id smc_tps25990_of_match[] = {
	{ .compatible = "ti,smc_tps25990" },
	{ .compatible = "ti,smc_tps25990" },
	{ .compatible = "ti,smc_tps25990" },
	{ .compatible = "ti,smc_tps25990" },
	{ .compatible = "ti,smc_tps25990" },
	{}
};
MODULE_DEVICE_TABLE(of, smc_tps25990_of_match);

static int smc_tps25990_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct pmbus_driver_info *info;
	u32 rimon = TPS25990_DEFAULT_RIMON;
	int ret;

	ret = device_property_read_u32(dev, "ti,rimon-micro-ohms", &rimon);
	if (ret < 0 && ret != -EINVAL)
		return dev_err_probe(dev, ret, "failed to get rimon\n");

	info = devm_kmemdup(dev, &smc_tps25990_base_info, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	/* Adapt the current and power scale for each instance */
	if( strcmp(client->name, "smc_tps25990_ri909") == 0) {
		rimon = TPS25990_909_RIMON;
	}
	else if( strcmp(client->name, "smc_tps25990_ri1270") == 0) {
		rimon = TPS25990_1270_RIMON;
	}
	else if( strcmp(client->name, "smc_tps25990_ri1600") == 0) {
		rimon = TPS25990_1600_RIMON;
	}
	else {
		rimon = TPS25990_DEFAULT_RIMON;
	}

	smc_tps25990_set_m(&info->m[PSC_CURRENT_IN], rimon);
	smc_tps25990_set_m(&info->m[PSC_POWER], rimon);

	/* Register sysfs hooks */
	ret = sysfs_create_group(&dev->kobj, &smc_tps25990_pmbus_inventory_attr_grp);
	if (ret) {
		dev_err(dev, "Failed to create sysfs entries\n");
		return -1;
	}
	return pmbus_do_probe(client, info);
}

static struct i2c_driver smc_tps25990_driver = {
	.driver = {
		.name = "smc_tps25990",
		.of_match_table = smc_tps25990_of_match,
	},
	.probe = smc_tps25990_probe,
	.id_table = smc_tps25990_i2c_id,
};
module_i2c_driver(smc_tps25990_driver);

MODULE_AUTHOR("Jerome Brunet <jbrunet@baylibre.com>");
MODULE_DESCRIPTION("PMBUS driver for TPS25990 eFuse");
MODULE_LICENSE("GPL");
MODULE_IMPORT_NS(PMBUS);
