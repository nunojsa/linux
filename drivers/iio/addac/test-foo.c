#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

static int axi_foo_probe(struct platform_device *pdev)
{
	int ret;

	dev_info(&pdev->dev, "Geting regulator vcc\n");
	ret = devm_regulator_get_enable(&pdev->dev, "vcc");
	dev_info(&pdev->dev, "done...\n");

	return ret;
}

static const struct of_device_id axi_foo[] = {
	{ .compatible = "adi,foo" },
	{},
};
MODULE_DEVICE_TABLE(of, axi_foo);

static struct platform_driver axi_fan_control_driver = {
	.driver = {
		.name = "axi_foo",
		.of_match_table = axi_foo,
	},
	.probe = axi_foo_probe,
};
module_platform_driver(axi_fan_control_driver);

MODULE_AUTHOR("Nuno Sa <nuno.sa@analog.com>");
MODULE_DESCRIPTION("Analog Devices Fan Control HDL CORE driver");
MODULE_LICENSE("GPL");