/*
 * Copyright (C) 2022 HiHope Open Source Organization .
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */
#include <sound/memalloc.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_dma.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/suspend.h>

#include "audio_platform_base.h"
#include "osal_io.h"
#include "osal_uaccess.h"
#include "audio_driver_log.h"
#include "rk3588_dma_ops.h"
#include "rk3588_dai_linux.h"

#define HDF_LOG_TAG rk3588_platform_ops

#define I2S0_ADDR 0xfe470000


#define DMA_RX_CHANNEL 0
#define DMA_TX_CHANNEL 1
#define DMA_CHANNEL_MAX 2

struct HdfDeviceObject *g_device;

struct DmaRuntimeData {
    struct dma_chan *dmaChn[DMA_CHANNEL_MAX];
    dma_cookie_t cookie[DMA_CHANNEL_MAX];
    uint32_t streamType;
};

static struct dma_chan *g_dmaChn[DMA_CHANNEL_MAX];
static dma_cookie_t g_cookie[DMA_CHANNEL_MAX];
static struct device *g_dmaDev;
static const char *g_i2s1DtsTreePath = "/i2s@fe470000";

struct device *getDmaDevice(void)
{
    struct device_node    *dmaOfNode;
    struct platform_device *platformdev;

    dmaOfNode = of_find_node_by_path(g_i2s1DtsTreePath);
    if (dmaOfNode == NULL) {
        AUDIO_DEVICE_LOG_ERR("get device node failed.");
        return NULL;
    }
    platformdev = of_find_device_by_node(dmaOfNode);
    if (platformdev == NULL) {
        AUDIO_DEVICE_LOG_ERR("get platformdev failed.");
        return NULL;
    }
    g_dmaDev = &platformdev->dev;
    return g_dmaDev;
}

int32_t AudioDmaDeviceInit(const struct AudioCard *card, const struct PlatformDevice *platform)
{
    struct PlatformData *data = NULL;
    static const char * const dmaChannelNames[] = {
        [DMA_RX_CHANNEL] = "rx",
        [DMA_TX_CHANNEL] = "tx",
    };
    uint32_t i = 0;
    struct device *dmaDevice = getDmaDevice();

    if (card == NULL) {
        AUDIO_DEVICE_LOG_ERR("card is null.");
        return HDF_FAILURE;
    }
    g_device = card->device;

    data = PlatformDataFromCard(card);
    if (data == NULL) {
        AUDIO_DEVICE_LOG_ERR("PlatformDataFromCard failed.");
        return HDF_FAILURE;
    }
    if (data->platformInitFlag == true) {
        AUDIO_DRIVER_LOG_DEBUG("platform init complete!");
        return HDF_SUCCESS;
    }
    data->platformInitFlag = true;
    for (i = 0; i <= 1; i++) {
        g_dmaChn[i] = dma_request_slave_channel(dmaDevice,
            dmaChannelNames[i]);
        if (g_dmaChn[i] == NULL) {
            AUDIO_DEVICE_LOG_ERR("dma_request_slave_channel streamType=%d failed", i);
            return HDF_FAILURE;
        }
    }
    AUDIO_DEVICE_LOG_DEBUG("success.");
    return HDF_SUCCESS;
}


static int32_t DmaRtdMemAlloc(struct PlatformData *data, enum AudioStreamType streamType)
{
    struct DmaRuntimeData *dmaRtd = NULL;

    if (data == NULL) {
        AUDIO_DEVICE_LOG_ERR("data is null.");
        return HDF_FAILURE;
    }

    dmaRtd = kzalloc(sizeof(*dmaRtd), GFP_KERNEL);
    if (!dmaRtd) {
        AUDIO_DEVICE_LOG_ERR("AudioRenderBuffInit: fail.");
        return HDF_FAILURE;
    }
    data->dmaPrv = dmaRtd;
    dmaRtd->streamType = streamType;
    AUDIO_DEVICE_LOG_DEBUG("streamType = %d", streamType);
    dmaRtd->dmaChn[streamType] = g_dmaChn[streamType];
    AUDIO_DEVICE_LOG_DEBUG("success.");
    return HDF_SUCCESS;
}

int32_t Rk3588DmaBufAlloc(struct PlatformData *data, const enum AudioStreamType streamType)
{
    uint32_t preallocBufSize;
    struct device *dmaDevice = getDmaDevice();

    if (data == NULL) {
        AUDIO_DEVICE_LOG_ERR("data is null");
        return HDF_FAILURE;
    }

    if (dmaDevice == NULL) {
        AUDIO_DEVICE_LOG_ERR("dmaDevice is null");
        return HDF_FAILURE;
    }

    if (streamType == AUDIO_CAPTURE_STREAM) {
        if (data->captureBufInfo.virtAddr == NULL) {
            preallocBufSize = data->captureBufInfo.cirBufMax;
            dmaDevice->coherent_dma_mask = 0xffffffffUL;
            AUDIO_DEVICE_LOG_DEBUG("AUDIO_CAPTURE_STREAM");
            data->captureBufInfo.virtAddr = dma_alloc_wc(dmaDevice, preallocBufSize,
                (dma_addr_t *)&data->captureBufInfo.phyAddr, GFP_DMA | GFP_KERNEL);
        }
    } else if (streamType == AUDIO_RENDER_STREAM) {
        if (data->renderBufInfo.virtAddr == NULL) {
            preallocBufSize = data->renderBufInfo.cirBufMax;
            dmaDevice->coherent_dma_mask = 0xffffffffUL;
            AUDIO_DEVICE_LOG_DEBUG("AUDIO_RENDER_STREAM");
            data->renderBufInfo.virtAddr = dma_alloc_wc(dmaDevice, preallocBufSize,
                (dma_addr_t *)&data->renderBufInfo.phyAddr, GFP_DMA | GFP_KERNEL);
        }
    } else {
        AUDIO_DEVICE_LOG_ERR("stream Type is invalude.");
        return HDF_FAILURE;
    }
    
    AUDIO_DEVICE_LOG_DEBUG("success.");
    return HDF_SUCCESS;
}

int32_t Rk3588DmaBufFree(struct PlatformData *data, const enum AudioStreamType streamType)
{
    struct device *dmaDevice = getDmaDevice();

    if (data == NULL) {
        AUDIO_DEVICE_LOG_ERR("data is null");
        return HDF_FAILURE;
    }

    if (streamType == AUDIO_CAPTURE_STREAM) {
        AUDIO_DEVICE_LOG_DEBUG("AUDIO_CAPTURE_STREAM");
        dma_free_wc(dmaDevice, data->captureBufInfo.cirBufMax, data->captureBufInfo.virtAddr,
                    data->captureBufInfo.phyAddr);
    } else if (streamType == AUDIO_RENDER_STREAM) {
        AUDIO_DEVICE_LOG_DEBUG("AUDIO_RENDER_STREAM");
        dma_free_wc(dmaDevice, data->renderBufInfo.cirBufMax, data->renderBufInfo.virtAddr,
                    data->renderBufInfo.phyAddr);
    } else {
        AUDIO_DEVICE_LOG_ERR("stream Type is invalude.");
        return HDF_FAILURE;
    }

    AUDIO_DEVICE_LOG_DEBUG("success");
    return HDF_SUCCESS;
}

int32_t  Rk3588DmaRequestChannel(const struct PlatformData *data, const enum AudioStreamType streamType)
{
    (void)data;
    AUDIO_DEVICE_LOG_DEBUG("sucess");
    return HDF_SUCCESS;
}

int32_t Rk3588DmaConfigChannel(const struct PlatformData *data, const enum AudioStreamType streamType)
{
    struct dma_chan *dmaChan;
    struct dma_slave_config slaveConfig;
    int32_t ret = 0;

    (void)memset_s(&slaveConfig, sizeof(slaveConfig), 0, sizeof(slaveConfig));
    if (streamType == AUDIO_RENDER_STREAM) {
        dmaChan = (struct dma_chan *)g_dmaChn[DMA_TX_CHANNEL];   // tx
        slaveConfig.direction = DMA_MEM_TO_DEV;
        slaveConfig.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
        slaveConfig.dst_addr = I2S0_ADDR + I2S_TXDR;
        slaveConfig.dst_maxburst = 8; // Max Transimit 8 Byte
    } else {
        dmaChan = (struct dma_chan *)g_dmaChn[DMA_RX_CHANNEL];
        slaveConfig.direction = DMA_DEV_TO_MEM;
        slaveConfig.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
        slaveConfig.src_addr = I2S0_ADDR + I2S_RXDR;
        slaveConfig.src_maxburst = 8; // Max Transimit 8 Byte
    }
    slaveConfig.device_fc = 0;
    slaveConfig.slave_id = 0;

    if (dmaChan == NULL) {
        AUDIO_DEVICE_LOG_ERR("dmaChan is null");
        return HDF_FAILURE;
    }
    ret = dmaengine_slave_config(dmaChan, &slaveConfig);
    if (ret != 0) {
        AUDIO_DEVICE_LOG_ERR("dmaengine_slave_config failed");
        return HDF_FAILURE;
    }
    AUDIO_DEVICE_LOG_DEBUG("success");
    return HDF_SUCCESS;
}

static int32_t BytesToFrames(uint32_t frameBits, uint32_t size, uint32_t *pointer)
{
    if (pointer == NULL || frameBits == 0) {
        AUDIO_DEVICE_LOG_ERR("input para is error.");
        return HDF_FAILURE;
    }
    *pointer = size / frameBits;
    return HDF_SUCCESS;
}

int32_t Rk3588PcmPointer(struct PlatformData *data, const enum AudioStreamType streamType, uint32_t *pointer)
{
    uint32_t bufSize;
    struct dma_chan *dmaChn;
    struct dma_tx_state dmaState;
    uint32_t currentPointer;
    int ret;

    if (data == NULL) {
        AUDIO_DEVICE_LOG_ERR("input para is null.");
        return HDF_FAILURE;
    }

    if (streamType == AUDIO_RENDER_STREAM) {
        dmaChn = g_dmaChn[DMA_TX_CHANNEL];
        bufSize = data->renderBufInfo.cirBufSize;
        if (dmaChn == NULL) {
            AUDIO_DEVICE_LOG_ERR("dmaChan is null");
            return HDF_FAILURE;
        }
        dmaengine_tx_status(dmaChn, g_cookie[DMA_TX_CHANNEL], &dmaState);

        if (dmaState.residue) {
            currentPointer = bufSize - dmaState.residue;
            ret = BytesToFrames(data->renderPcmInfo.frameSize, currentPointer, pointer);
            if (ret != HDF_SUCCESS) {
                AUDIO_DEVICE_LOG_ERR("BytesToFrames is failed.");
                return HDF_FAILURE;
            }
        } else {
            *pointer = 0;
        }
    } else {
        dmaChn = g_dmaChn[DMA_RX_CHANNEL];
        bufSize = data->captureBufInfo.cirBufSize;
        if (dmaChn == NULL) {
            AUDIO_DEVICE_LOG_ERR("dmaChan is null");
            return HDF_FAILURE;
        }
        dmaengine_tx_status(dmaChn, g_cookie[DMA_RX_CHANNEL], &dmaState);

        if (dmaState.residue) {
            currentPointer = bufSize - dmaState.residue;
            ret = BytesToFrames(data->capturePcmInfo.frameSize, currentPointer, pointer);
            if (ret != HDF_SUCCESS) {
                AUDIO_DEVICE_LOG_ERR("BytesToFrames is failed.");
                return HDF_FAILURE;
            }
        } else {
            *pointer = 0;
        }
    }

    return HDF_SUCCESS;
}

int32_t Rk3588DmaPrep(const struct PlatformData *data, const enum AudioStreamType streamType)
{
    (void)data;
    return HDF_SUCCESS;
}



int32_t Rk3588DmaSubmit(const struct PlatformData *data, const enum AudioStreamType streamType)
{
    struct dma_async_tx_descriptor *desc;
    enum dma_transfer_direction direction;
    unsigned long flags = 3;
    struct dma_chan *dmaChan = NULL;

    if (data == NULL) {
        AUDIO_DEVICE_LOG_ERR("input para is null.");
        return HDF_FAILURE;
    }

    if (streamType == AUDIO_RENDER_STREAM) {
        direction = DMA_MEM_TO_DEV;
        dmaChan = g_dmaChn[DMA_TX_CHANNEL];
        if (dmaChan == NULL) {
            AUDIO_DEVICE_LOG_ERR("dmaChan is null");
            return HDF_FAILURE;
        }
        desc = dmaengine_prep_dma_cyclic(dmaChan,
            data->renderBufInfo.phyAddr,
            data->renderBufInfo.cirBufSize,
            data->renderBufInfo.periodSize, direction, flags);
        if (!desc) {
            AUDIO_DEVICE_LOG_ERR("DMA_TX_CHANNEL desc create failed");
            return -ENOMEM;
        }
        g_cookie[DMA_TX_CHANNEL] = dmaengine_submit(desc);
    } else {
        direction = DMA_DEV_TO_MEM;
        dmaChan = g_dmaChn[DMA_RX_CHANNEL];
        if (dmaChan == NULL) {
            AUDIO_DEVICE_LOG_ERR("dmaChan is null");
            return HDF_FAILURE;
        }

        desc = dmaengine_prep_dma_cyclic(dmaChan,
            data->captureBufInfo.phyAddr,
            data->captureBufInfo.cirBufSize,
            data->captureBufInfo.periodSize, direction, flags);
        if (!desc) {
            AUDIO_DEVICE_LOG_ERR("DMA_RX_CHANNEL desc create failed");
            return -ENOMEM;
        }

        g_cookie[DMA_RX_CHANNEL] = dmaengine_submit(desc);
    }

    AUDIO_DEVICE_LOG_DEBUG("success");
    return 0;
}

int32_t Rk3588DmaPending(struct PlatformData *data, const enum AudioStreamType streamType)
{
    struct dma_chan *dmaChan = NULL;

    AUDIO_DEVICE_LOG_DEBUG("streamType = %d", streamType);
    if (streamType == AUDIO_RENDER_STREAM) {
        dmaChan = g_dmaChn[DMA_TX_CHANNEL];
    } else {
        dmaChan = g_dmaChn[DMA_RX_CHANNEL];
    }
    if (dmaChan == NULL) {
        AUDIO_DEVICE_LOG_ERR("dmaChan is null");
        return HDF_FAILURE;
    }

    dma_async_issue_pending(dmaChan);
    AUDIO_DEVICE_LOG_DEBUG("dmaChan chan_id = %d.", dmaChan->chan_id);

    AUDIO_DEVICE_LOG_DEBUG("success");
    return HDF_SUCCESS;
}

int32_t Rk3588DmaPause(struct PlatformData *data, const enum AudioStreamType streamType)
{
    struct dma_chan *dmaChan;

    if (streamType == AUDIO_RENDER_STREAM) {
        dmaChan = g_dmaChn[DMA_TX_CHANNEL];
    } else {
        dmaChan = g_dmaChn[DMA_RX_CHANNEL];
    }
    // can not use dmaengine_pause function
    if (dmaChan == NULL) {
        AUDIO_DEVICE_LOG_ERR("dmaChan is null");
        return HDF_FAILURE;
    }
    dmaengine_terminate_async(dmaChan);

    AUDIO_DEVICE_LOG_DEBUG("success");
    return HDF_SUCCESS;
}
int32_t Rk3588DmaResume(const struct PlatformData *data, const enum AudioStreamType streamType)
{
    int ret;
    struct dma_chan *dmaChan;
    if (data == NULL) {
        AUDIO_DEVICE_LOG_ERR("data is null");
        return HDF_FAILURE;
    }

    if (streamType == AUDIO_RENDER_STREAM) {
        dmaChan = g_dmaChn[DMA_TX_CHANNEL];
    } else {
        dmaChan = g_dmaChn[DMA_RX_CHANNEL];
    }
    if (dmaChan == NULL) {
        AUDIO_DEVICE_LOG_ERR("dmaChan is null");
        return HDF_FAILURE;
    }

    // use start Operation function
    ret = Rk3588DmaSubmit(data, streamType);
    if (ret != HDF_SUCCESS) {
        AUDIO_DEVICE_LOG_ERR("call Rk3588DmaSubmit failed");
        return HDF_FAILURE;
    }
    dma_async_issue_pending(dmaChan);

    AUDIO_DEVICE_LOG_DEBUG("success");
    return HDF_SUCCESS;
}
