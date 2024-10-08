/*
 * Copyright (C) 2022 HiHope Open Source Organization .
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */
#include <sound/pcm_params.h>
#include <sound/dmaengine_pcm.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/clk.h>
#include <linux/clk/rockchip.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/spinlock.h>

#include "audio_host.h"
#include "audio_control.h"
#include "audio_dai_if.h"
#include "audio_dai_base.h"
#include "audio_driver_log.h"
#include "audio_stream_dispatch.h"
#include "osal_io.h"
#include "rk3588_audio_common.h"
#include "audio_platform_base.h"
#include "rk3588_dai_ops.h"
#include "rk3588_dai_linux.h"

#define HDF_LOG_TAG rk3588_dai_linux

#define DRV_NAME "rockchip-i2s-tdm"

static bool rockchip_i2s_tdm_wr_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case I2S_TXCR:
	case I2S_RXCR:
	case I2S_CKR:
	case I2S_DMACR:
	case I2S_INTCR:
	case I2S_XFER:
	case I2S_CLR:
	case I2S_TXDR:
	case I2S_TDM_TXCR:
	case I2S_TDM_RXCR:
	case I2S_CLKDIV:
		return true;
	default:
		return false;
	}
}

static bool rockchip_i2s_tdm_rd_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case I2S_TXCR:
	case I2S_RXCR:
	case I2S_CKR:
	case I2S_DMACR:
	case I2S_INTCR:
	case I2S_XFER:
	case I2S_CLR:
	case I2S_TXDR:
	case I2S_RXDR:
	case I2S_TXFIFOLR:
	case I2S_INTSR:
	case I2S_RXFIFOLR:
	case I2S_TDM_TXCR:
	case I2S_TDM_RXCR:
	case I2S_CLKDIV:
		return true;
	default:
		return false;
	}
}

static bool rockchip_i2s_tdm_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case I2S_TXFIFOLR:
	case I2S_INTSR:
	case I2S_CLR:
	case I2S_TXDR:
	case I2S_RXDR:
	case I2S_RXFIFOLR:
		return true;
	default:
		return false;
	}
}

static bool rockchip_i2s_tdm_precious_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case I2S_RXDR:
		return true;
	default:
		return false;
	}
}

static const struct reg_default rockchip_i2s_tdm_reg_defaults[] = {
	{0x00, 0x7200000f},
	{0x04, 0x01c8000f},
	{0x08, 0x00001f1f},
	{0x10, 0x001f0000},
	{0x14, 0x01f00000},
	{0x30, 0x00003eff},
	{0x34, 0x00003eff},
	{0x38, 0x00000707},
};

static const struct regmap_config rockchip_i2s_tdm_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = I2S_CLKDIV,
	.reg_defaults = rockchip_i2s_tdm_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(rockchip_i2s_tdm_reg_defaults),
	.writeable_reg = rockchip_i2s_tdm_wr_reg,
	.readable_reg = rockchip_i2s_tdm_rd_reg,
	.volatile_reg = rockchip_i2s_tdm_volatile_reg,
	.precious_reg = rockchip_i2s_tdm_precious_reg,
	.cache_type = REGCACHE_FLAT,
};

static const struct of_device_id rockchip_i2s_tdm_match[] = {
	{ .compatible = "rockchip,rk3588-i2s-tdm", },
	{},
};

#if 0
static int of_i2s_resetid_get(struct device_node *node,
			      const char *id)
{
	struct of_phandle_args args;
	int index = 0;
	int ret;

	if (id)
		index = of_property_match_string(node,
						 "reset-names", id);
	ret = of_parse_phandle_with_args(node, "resets", "#reset-cells",
					 index, &args);
	if (ret)
		return ret;

	return args.args[0];
}
#endif

static int i2s_tdm_runtime_suspend(struct rk3588_i2s_tdm_dev *i2s_tdm)
{
    regcache_cache_only(i2s_tdm->regmap, true);
    if (!IS_ERR(i2s_tdm->mclk_tx))
        clk_disable_unprepare(i2s_tdm->mclk_tx);
    if (!IS_ERR(i2s_tdm->mclk_rx))
        clk_disable_unprepare(i2s_tdm->mclk_rx);
    return 0;
}
static int rockchip_i2s_tdm_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	//struct device_node *cru_node;
	const struct of_device_id *of_id;
	struct rk3588_i2s_tdm_dev *i2s_tdm;
	//struct snd_soc_dai_driver *soc_dai;
	struct resource *res;
	void __iomem *regs;
	//bool sync;
	int ret;
	int val;
    struct device *temp_i2s_dev;


    temp_i2s_dev = &pdev->dev;
    if (strcmp(dev_name(temp_i2s_dev), "fe470000.i2s") != 0) {
        AUDIO_DRIVER_LOG_INFO("failed dmaDevice->name %s ", dev_name(temp_i2s_dev));
        return 0;
    }
    AUDIO_DEVICE_LOG_DEBUG("match dmaDevice->name %s ", dev_name(temp_i2s_dev));
	i2s_tdm = devm_kzalloc(&pdev->dev, sizeof(*i2s_tdm), GFP_KERNEL);
	if (!i2s_tdm)
		return -ENOMEM;

	i2s_tdm->dev = &pdev->dev;
	of_id = of_match_device(rockchip_i2s_tdm_match, &pdev->dev);
	if (!of_id)
		return -EINVAL;

	spin_lock_init(&i2s_tdm->lock);
	i2s_tdm->soc_data = (const struct rk_i2s_soc_data *)of_id->data;

	i2s_tdm->bclk_fs = 64;
	if (!of_property_read_u32(node, "rockchip,bclk-fs", &val)) {
		if ((val >= 32) && (val % 2 == 0))
			i2s_tdm->bclk_fs = val;
	}

	i2s_tdm->clk_trcm = I2S_CKR_TRCM_TXRX;
	if (!of_property_read_u32(node, "rockchip,clk-trcm", &val)) {
		if (val >= 0 && val <= 2) {
			i2s_tdm->clk_trcm = val << I2S_CKR_TRCM_SHIFT;
			// if (i2s_tdm->clk_trcm)
			// 	soc_dai->symmetric_rates = 1;
		}
	}

	i2s_tdm->tdm_fsync_half_frame =
		of_property_read_bool(node, "rockchip,tdm-fsync-half-frame");


	i2s_tdm->grf = syscon_regmap_lookup_by_phandle(node, "rockchip,grf");

	// sync = of_device_is_compatible(node, "rockchip,px30-i2s-tdm") ||
	//        of_device_is_compatible(node, "rockchip,rk1808-i2s-tdm") ||
	//        of_device_is_compatible(node, "rockchip,rk3308-i2s-tdm");

	// if (i2s_tdm->clk_trcm && sync) {
	// 	cru_node = of_parse_phandle(node, "rockchip,cru", 0);
	// 	i2s_tdm->cru_base = of_iomap(cru_node, 0);
	// 	if (!i2s_tdm->cru_base)
	// 		return -ENOENT;

	// 	i2s_tdm->tx_reset_id = of_i2s_resetid_get(node, "tx-m");
	// 	i2s_tdm->rx_reset_id = of_i2s_resetid_get(node, "rx-m");
	// }

	i2s_tdm->tx_reset = devm_reset_control_get(&pdev->dev, "tx-m");
	if (IS_ERR(i2s_tdm->tx_reset)) {
		ret = PTR_ERR(i2s_tdm->tx_reset);
		if (ret != -ENOENT)
			return ret;
	}

	i2s_tdm->rx_reset = devm_reset_control_get(&pdev->dev, "rx-m");
	if (IS_ERR(i2s_tdm->rx_reset)) {
		ret = PTR_ERR(i2s_tdm->rx_reset);
		if (ret != -ENOENT)
			return ret;
	}

	i2s_tdm->hclk = devm_clk_get(&pdev->dev, "hclk");
	if (IS_ERR(i2s_tdm->hclk))
		return PTR_ERR(i2s_tdm->hclk);

	ret = clk_prepare_enable(i2s_tdm->hclk);
	if (ret)
		return ret;

	i2s_tdm->mclk_tx = devm_clk_get(&pdev->dev, "mclk_tx");
	if (IS_ERR(i2s_tdm->mclk_tx))
		return PTR_ERR(i2s_tdm->mclk_tx);

	i2s_tdm->mclk_rx = devm_clk_get(&pdev->dev, "mclk_rx");
	if (IS_ERR(i2s_tdm->mclk_rx))
		return PTR_ERR(i2s_tdm->mclk_rx);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(regs))
		return PTR_ERR(regs);

	i2s_tdm->regmap = devm_regmap_init_mmio(&pdev->dev, regs,
					    &rockchip_i2s_tdm_regmap_config);
	if (IS_ERR(i2s_tdm->regmap))
		return PTR_ERR(i2s_tdm->regmap);

	atomic_set(&i2s_tdm->refcount, 0);
	dev_set_drvdata(&pdev->dev, i2s_tdm);

	regmap_update_bits(i2s_tdm->regmap, I2S_DMACR, I2S_DMACR_TDL_MASK,
			   I2S_DMACR_TDL(16));
	regmap_update_bits(i2s_tdm->regmap, I2S_DMACR, I2S_DMACR_RDL_MASK,
			   I2S_DMACR_RDL(16));
	regmap_update_bits(i2s_tdm->regmap, I2S_CKR,
			   I2S_CKR_TRCM_MASK, i2s_tdm->clk_trcm);

	return 0;
}

static int rockchip_i2s_tdm_remove(struct platform_device *pdev)
{
	struct rk3588_i2s_tdm_dev *i2s_tdm = dev_get_drvdata(&pdev->dev);
	
	i2s_tdm_runtime_suspend(i2s_tdm);

	if (!IS_ERR(i2s_tdm->mclk_tx))
		clk_prepare_enable(i2s_tdm->mclk_tx);
	if (!IS_ERR(i2s_tdm->mclk_rx))
		clk_prepare_enable(i2s_tdm->mclk_rx);
	if (!IS_ERR(i2s_tdm->hclk))
		clk_disable_unprepare(i2s_tdm->hclk);

	return 0;
}



static struct platform_driver rockchip_i2s_tdm_driver = {
	.probe = rockchip_i2s_tdm_probe,
	.remove = rockchip_i2s_tdm_remove,
	.driver = {
		.name = DRV_NAME,
		.of_match_table = of_match_ptr(rockchip_i2s_tdm_match),
		.pm = NULL,
	},
};
module_platform_driver(rockchip_i2s_tdm_driver);