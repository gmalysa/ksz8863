/*
 * drivers/net/ethernet/micrel/ksz8863.c
 *
 * Minimal driver for the Microchip KSZ8863 3-port switch.
 * Currently expects an SPI interface
 *
 * Copyright (c) 2020 - Timesys Corporation
 *   Greg Malysa <greg.malysa@timesys.com>
 *
 * TODO:
 * 	- Add sysfs interface for most config registers
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/errno.h>
#include <linux/sysfs.h>
#include <linux/spi.h>

#define KSZ8863_DRIVER_NAME "ksz8863"

#define KSZ8863_CHIP_ID_MASK			0xf0ff
#define KSZ8863_CHIP_ID					0x3088
#define KSZ8863_CHIP_REVISION_MASK		0x0e00

#define KSZ8863_SPI_READ_CMD			0x03
#define KSZ8863_SPI_WRITE_CMD			0x02

#define KSZ8863_REG_CHIP_ID_0			0x00
#define KSZ8863_REG_CHIP_ID_1			0x01
#define KSZ8863_REG_GLOBAL_CONTROL0		0x02

/* Users expect to read up to VALUES registers, we need VALUES+2 bytes for spi transfer */
#define KSZ8863_MAX_XFER_VALUES			8
#define KSZ8863_MAX_XFER				(KSZ8863_MAX_XFER_VALUES + 2)

struct ksz8863_data {
	struct spi_device *spi;
};

static int ksz8863_spi_xfer(struct ksz8863_data *data, u8 *tx, u8 *rx, u32 length) {
	struct spi_transfer xfer;
	struct spi_message msg;

	xfer.tx = tx;
	xfer.length = length;
	xfer.rx = rx;

	spi_message_init_with_transfers(&msg, &xfer, 1);
	return spi_sync(data->spi, &msg);
}

/**
 * Read count 8-bit registers starting at addr
 */
static int ksz8863_spi_read(struct ksz8863_data *data, u8 *dst, u8 addr, u32 count) {
	int ret;
	u8 tx[KSZ8863_MAX_XFER];
	u8 rx[KSZ8863_MAX_XFER];

	if (count > KSZ8863_MAX_XFER_VALUES)
		return -EINVAL;

	memset(tx, 0, sizeof(tx));
	tx[0] = KSZ_SPI_READ_CMD;
	tx[1] = addr;

	/* 2 bytes for command and address */
	ret = ksz8863_spi_xfer(data, tx, rx, count + 2);
	if (ret)
		return ret;

	memcpy(dst, &rx[2], count);
	return 0;
}

static int ksz8863_spi_write(struct ksz8863_data *data, u8 *src, u8 addr, u32 count) {
	u8 tx[KSZ8863_MAX_XFER];

	if (count > KSZ8863_MAX_XFER_VALUES)
		return -EINVAL;

	tx[0] = KSZ_SPI_WRITE_CMD;
	tx[1] = addr;
	memcpy(&tx[2], src, count);

	/* 2 bytes for command and address */
	return ksz8863_spi_xfer(data, tx, NULL, count + 2);
}

static u8 ksz8863_spi_read8(struct ksz8863_data *data, u8 addr) {
	int ret;
	u8 rx;

	ret = ksz8863_spi_read(data, &rx, addr, 1);
	if (ret)
		return 0;

	return rx;
}

static int ksz8863_spi_write8(struct ksz8863_data *data, u8 addr, u8 value) {
	return ksz8863_spi_write(data, &value, addr, 1);
}

static u16 ksz8863_get_chipid(struct ksz8863_data *data) {
	u16 chipid;
	u8 reg;

	reg = ksz8863_spi_read8(data, KSZ8863_REG_CHIP_ID_0);
	chipid = reg;

	/* remove the start bit, bit 0 */
	reg = ksz8863_spi_read8(data, KSZ8863_REG_CHIP_ID_1);
	chipid |= (reg & 0xfe) << 8;

	return chipid;
}

static int ksz8863_probe(struct spi_device *spi) {
	struct ksz8863_data *data;
	int ret;
	u16 chipid;

	dev_info(&spi->dev, "ksz8863_probe\n");

	data = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!data) {
		dev_err(&spi->dev, "Could not allocate driver private data\n");
		return -ENOMEM;
	}

	dev_set_drvdata(&spi->dev, data);
	data->spi = spi;

	chipid = ksz8863_get_chipid(data);
	if (KSZ8863_CHIP_ID != (chipid & KSZ8863_CHIP_ID_MASK)) {
		dev_err(&spi->dev, "Invalid chip ID 0x%x found\n", chipid)
		return -ENODEV;
	}

	dev_info(&spi->dev, "Found KSZ8863, revision %d\n", (chipid & KSZ8863_REVISION_MASK));

	// @todo set start switch bit here

	return 0;
}

static int ksz8863_remove(struct spi_device *spi) {
	struct ksz8863_data *data;

	data = dev_get_drvdata(spi);
	if (!data)
		return 0;

	kfree(data);
	return 0;
}

static const struct of_device_id ksz8863_dt_ids[] = {
	{ .compatible = "microchip,ksz8863" },
	{}
};

MODULE_DEVICE_TABLE(of, ksz8863_dt_ids);

static struct spi_driver ksz8863_driver = {
	.driver = {
		.name = KSZ8863_DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(ksz8863_dt_ids),
	},
	.probe = ksz8863_probe,
	.remove = ksz8863_remove,
};

module_spi_driver(&ksz8863_driver);

MODULE_DESCRIPTION("KSZ8863 switch driver with SPI interface");
MODULE_AUTHOR("Greg Malysa <greg.malysa@timesys.com>");
MODULE_LICENSE("GPL");

MODULE_ALIAS("spi:ksz8863");
