// SPDX-License-Identifier: GPL-2.0-only
/*
 * Analog Devices Generic AXI DAC IP core
 * Link: https://wiki.analog.com/resources/fpga/docs/axi_dac_ip
 *
 * Copyright 2016-2024 Analog Devices Inc.
 */

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/regmap.h>

#include <linux/fpga/adi-axi-common.h>
#include <linux/iio/backend.h>
#include <linux/iio/buffer-dmaengine.h>
#include <linux/iio/buffer.h>
#include <linux/iio/iio.h>

/*
 * Register definitions:
 *   https://wiki.analog.com/resources/fpga/docs/axi_dac_ip#register_map
 */

 /* DAC controls */

#define AXI_REG_RSTN			0x0040
#define   AXI_REG_RSTN_CE_N		BIT(2)
#define   AXI_REG_RSTN_MMCM_RSTN	BIT(1)
#define   AXI_REG_RSTN_RSTN		BIT(0)

/* DAC Channel controls */
#define AXI_REG_CHAN_CNTRL_7(c)		(0x0418 + (c) * 0x40)
#define   AXI_DAC_DATA_SEL_MASK		GENMASK(3, 0)

enum {
	AXI_DAC_DATA_INTERNAL_TONE,
	AXI_DAC_DATA_DMA = 2,
};

struct axi_dac_state {
	struct regmap *regmap;
	struct device *dev;
};

static int axi_dac_enable(struct iio_backend *back)
{
	struct axi_dac_state *st = iio_backend_get_priv(back);
	int ret;

	ret = regmap_set_bits(st->regmap, AXI_REG_RSTN, AXI_REG_RSTN_MMCM_RSTN);
	if (ret)
		return ret;

	fsleep(10);
	return regmap_set_bits(st->regmap, AXI_REG_RSTN,
			       AXI_REG_RSTN_RSTN | AXI_REG_RSTN_MMCM_RSTN);
}

static void axi_dac_disable(struct iio_backend *back)
{
	struct axi_dac_state *st = iio_backend_get_priv(back);

	regmap_write(st->regmap, AXI_REG_RSTN, 0);
}

static struct iio_buffer *axi_dac_request_buffer(struct iio_backend *back,
						 struct iio_dev *indio_dev)
{
	struct axi_dac_state *st = iio_backend_get_priv(back);
	struct iio_buffer *buffer;
	const char *dma_name;
	int ret;

	if (device_property_read_string(st->dev, "dma-names", &dma_name))
		dma_name = "tx";

	buffer = iio_dmaengine_buffer_alloc(st->dev, dma_name);
	if (IS_ERR(buffer)) {
		dev_err(st->dev, "Could not get DMA buffer, %ld\n",
			PTR_ERR(buffer));
		return ERR_CAST(buffer);
	}

	indio_dev->modes |= INDIO_BUFFER_HARDWARE;
	iio_buffer_set_dir(buffer, IIO_BUFFER_DIRECTION_OUT);

	ret = iio_device_attach_buffer(indio_dev, buffer);
	if (ret)
		return ERR_PTR(ret);

	return buffer;
}

static void axi_dac_free_buffer(struct iio_backend *back,
				struct iio_buffer *buffer)
{
	iio_dmaengine_buffer_free(buffer);
}

static int ad9739a_read_raw(struct iio_backend *back,
			    struct iio_chan_spec const *chan,
			    int *val, int *val2, long mask)
{
	struct axi_dac_state *st = iio_backend_get_priv(back);

	switch (mask) {
	case IIO_CHAN_INFO_SCALE:
		return 0;
	case IIO_CHAN_INFO_FREQUENCY:
		return 0;
	case IIO_CHAN_INFO_PHASE:
		return 0;
	default:
		return -EINVAL;
	};
}

static int ad9739a_write_raw(struct iio_backend *back,
			     struct iio_chan_spec const *chan,
			     int val, int val2, long mask)
{
	struct axi_dac_state *st = iio_backend_get_priv(back);

	switch (mask) {
	case IIO_CHAN_INFO_SCALE:
		return 0;
	case IIO_CHAN_INFO_FREQUENCY:
		return 0;
	case IIO_CHAN_INFO_PHASE:
		return 0;
	default:
		return -EINVAL;
	};
}

static int axi_dac_data_source_set(struct iio_backend *back, unsigned int chan,
				   enum iio_backend_data_source data)
{
	struct axi_dac_state *st = iio_backend_get_priv(back);

	switch (data) {
	case IIO_BACKEND_INTERNAL_CW:
		return regmap_update_bits(st->regmap, AXI_REG_CHAN_CNTRL_7(chan),
					  AXI_DAC_DATA_SEL_MASK,
					  AXI_DAC_DATA_INTERNAL_TONE);
	case IIO_BACKEND_EXTERNAL:
		return regmap_update_bits(st->regmap, AXI_REG_CHAN_CNTRL_7(chan),
					  AXI_DAC_DATA_SEL_MASK,
					  AXI_DAC_DATA_DMA);
	default:
		return -EINVAL;
	}
}

static const struct iio_backend_ops axi_dac_generic = {
	.enable = axi_dac_enable,
	.disable = axi_dac_disable,
	.request_buffer = axi_dac_request_buffer,
	.free_buffer = axi_dac_free_buffer,
	.read_raw = axi_dac_read_raw,
	.write_raw = axi_dac_write_raw,
	.data_source_set = axi_dac_data_source_set,
};

static const struct regmap_config axi_dac_regmap_config = {
	.val_bits = 32,
	.reg_bits = 32,
	.reg_stride = 4,
	.max_register = 0x0800,
};

static int axi_dac_probe(struct platform_device *pdev)
{
	unsigned int ver, *expected_ver;
	struct axi_dac_state *st;
	void __iomem *base;
	struct clk *clk;
	int ret;

	st = devm_kzalloc(&pdev->dev, sizeof(*st), GFP_KERNEL);
	if (!st)
		return -ENOMEM;

	expected_ver = (unsigned int *)device_get_match_data(&pdev->dev);
	if (!expected_ver)
		return -ENODEV;

	clk = devm_clk_get_enabled(&pdev->dev, NULL);
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(base))
		return PTR_ERR(base);

	st->dev = &pdev->dev;
	st->regmap = devm_regmap_init_mmio(&pdev->dev, base,
					   &axi_dac_regmap_config);
	if (IS_ERR(st->regmap))
		return PTR_ERR(st->regmap);

	/*
	 * Force disable the core. Up to the frontend to enable us. And we can
	 * still read/write registers...
	 */
	ret = regmap_write(st->regmap, AXI_REG_RSTN, 0);
	if (ret)
		return ret;

	ret = regmap_read(st->regmap, ADI_AXI_REG_VERSION, &ver);
	if (ret)
		return ret;

	if (ADI_AXI_PCORE_VER_MAJOR(ver) != ADI_AXI_PCORE_VER_MAJOR(*expected_ver)) {
		dev_err(&pdev->dev,
			"Major version mismatch. Expected %d.%.2d.%c, Reported %d.%.2d.%c\n",
			ADI_AXI_PCORE_VER_MAJOR(*expected_ver),
			ADI_AXI_PCORE_VER_MINOR(*expected_ver),
			ADI_AXI_PCORE_VER_PATCH(*expected_ver),
			ADI_AXI_PCORE_VER_MAJOR(ver),
			ADI_AXI_PCORE_VER_MINOR(ver),
			ADI_AXI_PCORE_VER_PATCH(ver));
		return -ENODEV;
	}

	ret = devm_iio_backend_register(&pdev->dev, &axi_dac_generic, st);
	if (ret)
		return ret;

	dev_info(&pdev->dev, "AXI DAC IP core (%d.%.2d.%c) probed\n",
		 ADI_AXI_PCORE_VER_MAJOR(ver),
		 ADI_AXI_PCORE_VER_MINOR(ver),
		 ADI_AXI_PCORE_VER_PATCH(ver));

	return 0;
}

static unsigned int axi_dac_9_1_b_info = ADI_AXI_PCORE_VER(9, 1, 'b');

static const struct of_device_id axi_dac_of_match[] = {
	{ .compatible = "adi,axi-dac-9.1.b", .data = &axi_dac_9_1_b_info },
	{}
};
MODULE_DEVICE_TABLE(of, axi_dac_of_match);

static struct platform_driver axi_dac_driver = {
	.driver = {
		.name = "adi-axi-dac",
		.of_match_table = axi_dac_of_match,
	},
	.probe = axi_dac_probe,
};
module_platform_driver(axi_dac_driver);

MODULE_AUTHOR("Michael Hennerich <michael.hennerich@analog.com>");
MODULE_AUTHOR("Nuno Sa <nuno.sa@analog.com>");
MODULE_DESCRIPTION("Analog Devices Generic AXI DAC IP core driver");
MODULE_LICENSE("GPL v2");
MODULE_IMPORT_NS(IIO_DMAENGINE_BUFFER);
MODULE_IMPORT_NS(IIO_BACKEND);
