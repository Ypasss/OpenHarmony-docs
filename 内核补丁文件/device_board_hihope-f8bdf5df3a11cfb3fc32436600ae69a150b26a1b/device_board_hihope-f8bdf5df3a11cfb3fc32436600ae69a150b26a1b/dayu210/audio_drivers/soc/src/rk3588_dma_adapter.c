/*
 * Copyright (C) 2022 HiHope Open Source Organization .
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */
 
#include <linux/slab.h>
#include "gpio_if.h"
#include "audio_core.h"
#include "audio_platform_base.h"
#include "rk3588_dma_ops.h"
#include "osal_io.h"
#include "osal_mem.h"
#include "audio_driver_log.h"

#define HDF_LOG_TAG rk3588_platform_adapter

struct AudioDmaOps g_dmaDeviceOps = {
    .DmaBufAlloc = Rk3588DmaBufAlloc,
    .DmaBufFree = Rk3588DmaBufFree,
    .DmaRequestChannel = Rk3588DmaRequestChannel,
    .DmaConfigChannel = Rk3588DmaConfigChannel,
    .DmaPrep = Rk3588DmaPrep,
    .DmaSubmit = Rk3588DmaSubmit,
    .DmaPending = Rk3588DmaPending,
    .DmaPause = Rk3588DmaPause,
    .DmaResume = Rk3588DmaResume,
    .DmaPointer = Rk3588PcmPointer,
};

struct PlatformData g_platformData = {
    .PlatformInit = AudioDmaDeviceInit,
    .ops = &g_dmaDeviceOps,
};

/* HdfDriverEntry implementations */
static int32_t PlatformDriverBind(struct HdfDeviceObject *device)
{
    struct PlatformHost *platformHost = NULL;

    if (device == NULL) {
        AUDIO_DEVICE_LOG_ERR("input para is NULL.");
        return HDF_FAILURE;
    }

    platformHost = (struct PlatformHost *)OsalMemCalloc(sizeof(*platformHost));
    if (platformHost == NULL) {
        AUDIO_DEVICE_LOG_ERR("malloc host fail!");
        return HDF_FAILURE;
    }

    platformHost->device = device;
    device->service = &platformHost->service;

    AUDIO_DEVICE_LOG_DEBUG("success!");
    return HDF_SUCCESS;
}

static int32_t PlatformGetServiceName(const struct HdfDeviceObject *device)
{
    const struct DeviceResourceNode *node = NULL;
    struct DeviceResourceIface *drsOps = NULL;
    int32_t ret;

    if (device == NULL) {
        AUDIO_DEVICE_LOG_ERR("para is NULL.");
        return HDF_FAILURE;
    }

    node = device->property;
    if (node == NULL) {
        AUDIO_DEVICE_LOG_ERR("node is NULL.");
        return HDF_FAILURE;
    }

    drsOps = DeviceResourceGetIfaceInstance(HDF_CONFIG_SOURCE);
    if (drsOps == NULL || drsOps->GetString == NULL) {
        AUDIO_DEVICE_LOG_ERR("get drsops object instance fail!");
        return HDF_FAILURE;
    }

    ret = drsOps->GetString(node, "serviceName", &g_platformData.drvPlatformName, 0);
    if (ret != HDF_SUCCESS) {
        AUDIO_DEVICE_LOG_ERR("read serviceName fail!");
        return ret;
    }
    AUDIO_DEVICE_LOG_DEBUG("success!");

    return HDF_SUCCESS;
}

static int32_t PlatformDriverInit(struct HdfDeviceObject *device)
{
    int32_t ret;

    if (device == NULL) {
        AUDIO_DEVICE_LOG_ERR("device is NULL.");
        return HDF_ERR_INVALID_OBJECT;
    }

    ret = PlatformGetServiceName(device);
    if (ret !=  HDF_SUCCESS) {
        AUDIO_DEVICE_LOG_ERR("get service name fail.");
        return ret;
    }

    OsalMutexInit(&g_platformData.renderBufInfo.buffMutex);
    OsalMutexInit(&g_platformData.captureBufInfo.buffMutex);
    ret = AudioSocRegisterPlatform(device, &g_platformData);
    if (ret !=  HDF_SUCCESS) {
        AUDIO_DEVICE_LOG_ERR("register dai fail.");
        return ret;
    }

    AUDIO_DEVICE_LOG_DEBUG("success.\n");
    return HDF_SUCCESS;
}

static void PlatformDriverRelease(struct HdfDeviceObject *device)
{
    struct PlatformHost *platformHost = NULL;
    if (device == NULL) {
        AUDIO_DEVICE_LOG_ERR("device is NULL");
        return;
    }

    platformHost = (struct PlatformHost *)device->service;
    if (platformHost == NULL) {
        AUDIO_DEVICE_LOG_ERR("platformHost is NULL");
        return;
    }

    OsalMutexDestroy(&g_platformData.renderBufInfo.buffMutex);
    OsalMutexDestroy(&g_platformData.captureBufInfo.buffMutex);
    OsalMemFree(platformHost);
    AUDIO_DEVICE_LOG_DEBUG("success.\n");
    return;
}

/* HdfDriverEntry definitions */
struct HdfDriverEntry g_platformDriverEntry = {
    .moduleVersion = 1,
    .moduleName = "DMA_RK3588",
    .Bind = PlatformDriverBind,
    .Init = PlatformDriverInit,
    .Release = PlatformDriverRelease,
};
HDF_INIT(g_platformDriverEntry);
