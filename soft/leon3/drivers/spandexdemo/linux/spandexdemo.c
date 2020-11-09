#include <linux/of_device.h>
#include <linux/mm.h>

#include <asm/io.h>

#include <esp_accelerator.h>
#include <esp.h>

#include "spandexdemo.h"

#define DRV_NAME	"spandexdemo"

/* <<--regs-->> */
#define SPANDEXDEMO_BASE_ADDR_REG 0x5c
#define SPANDEXDEMO_OWNER_REG 0x58
#define SPANDEXDEMO_OWNER_PRED_REG 0x54
#define SPANDEXDEMO_STRIDE_SIZE_REG 0x50
#define SPANDEXDEMO_COH_MSG_REG 0x4c
#define SPANDEXDEMO_ARRAY_LENGTH_REG 0x48
#define SPANDEXDEMO_REQ_TYPE_REG 0x44
#define SPANDEXDEMO_ELEMENT_SIZE_REG 0x40

struct spandexdemo_device {
	struct esp_device esp;
};

static struct esp_driver spandexdemo_driver;

static struct of_device_id spandexdemo_device_ids[] = {
	{
		.name = "SLD_SPANDEXDEMO",
	},
	{
		.name = "eb_058",
	},
	{
		.compatible = "sld,spandexdemo",
	},
	{ },
};

static int spandexdemo_devs;

static inline struct spandexdemo_device *to_spandexdemo(struct esp_device *esp)
{
	return container_of(esp, struct spandexdemo_device, esp);
}

static void spandexdemo_prep_xfer(struct esp_device *esp, void *arg)
{
	struct spandexdemo_access *a = arg;

	/* <<--regs-config-->> */
	iowrite32be(a->base_addr, esp->iomem + SPANDEXDEMO_BASE_ADDR_REG);
	iowrite32be(a->owner, esp->iomem + SPANDEXDEMO_OWNER_REG);
	iowrite32be(a->owner_pred, esp->iomem + SPANDEXDEMO_OWNER_PRED_REG);
	iowrite32be(a->stride_size, esp->iomem + SPANDEXDEMO_STRIDE_SIZE_REG);
	iowrite32be(a->coh_msg, esp->iomem + SPANDEXDEMO_COH_MSG_REG);
	iowrite32be(a->array_length, esp->iomem + SPANDEXDEMO_ARRAY_LENGTH_REG);
	iowrite32be(a->req_type, esp->iomem + SPANDEXDEMO_REQ_TYPE_REG);
	iowrite32be(a->element_size, esp->iomem + SPANDEXDEMO_ELEMENT_SIZE_REG);
	iowrite32be(a->src_offset, esp->iomem + SRC_OFFSET_REG);
	iowrite32be(a->dst_offset, esp->iomem + DST_OFFSET_REG);

}

static bool spandexdemo_xfer_input_ok(struct esp_device *esp, void *arg)
{
	/* struct spandexdemo_device *spandexdemo = to_spandexdemo(esp); */
	/* struct spandexdemo_access *a = arg; */

	return true;
}

static int spandexdemo_probe(struct platform_device *pdev)
{
	struct spandexdemo_device *spandexdemo;
	struct esp_device *esp;
	int rc;

	spandexdemo = kzalloc(sizeof(*spandexdemo), GFP_KERNEL);
	if (spandexdemo == NULL)
		return -ENOMEM;
	esp = &spandexdemo->esp;
	esp->module = THIS_MODULE;
	esp->number = spandexdemo_devs;
	esp->driver = &spandexdemo_driver;
	rc = esp_device_register(esp, pdev);
	if (rc)
		goto err;

	spandexdemo_devs++;
	return 0;
 err:
	kfree(spandexdemo);
	return rc;
}

static int __exit spandexdemo_remove(struct platform_device *pdev)
{
	struct esp_device *esp = platform_get_drvdata(pdev);
	struct spandexdemo_device *spandexdemo = to_spandexdemo(esp);

	esp_device_unregister(esp);
	kfree(spandexdemo);
	return 0;
}

static struct esp_driver spandexdemo_driver = {
	.plat = {
		.probe		= spandexdemo_probe,
		.remove		= spandexdemo_remove,
		.driver		= {
			.name = DRV_NAME,
			.owner = THIS_MODULE,
			.of_match_table = spandexdemo_device_ids,
		},
	},
	.xfer_input_ok	= spandexdemo_xfer_input_ok,
	.prep_xfer	= spandexdemo_prep_xfer,
	.ioctl_cm	= SPANDEXDEMO_IOC_ACCESS,
	.arg_size	= sizeof(struct spandexdemo_access),
};

static int __init spandexdemo_init(void)
{
	return esp_driver_register(&spandexdemo_driver);
}

static void __exit spandexdemo_exit(void)
{
	esp_driver_unregister(&spandexdemo_driver);
}

module_init(spandexdemo_init)
module_exit(spandexdemo_exit)

MODULE_DEVICE_TABLE(of, spandexdemo_device_ids);

MODULE_AUTHOR("Emilio G. Cota <cota@braap.org>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("spandexdemo driver");
