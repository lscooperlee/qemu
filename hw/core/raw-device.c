#include "qemu/osdep.h"
#include "hw/core/cpu.h"
#include "hw/sysbus.h"
#include "sysemu/dma.h"
#include "sysemu/reset.h"
#include "hw/loader.h"
#include "hw/qdev-properties.h"
#include "qapi/error.h"
#include "qemu/module.h"
#include "hw/qdev-core.h"

#include <dlfcn.h>
#include <libgen.h>
#include <stdio.h>

typedef struct RawDeviceState
{
    /* <private> */
    DeviceState parent_obj;

    /* <public> */
    CPUState *cpu;
    MemoryRegion mem_region;

    uint64_t addr;
    uint64_t size;
    char *file;
    char *arg;

    struct
    {
        void *handle;
        int (*init)(const char *);
        uint64_t (*read)(uint64_t offset, uint32_t size);
        void (*write)(uint64_t offset, uint64_t value, uint32_t size);
    } plugin;

} RawDeviceState;

#define TYPE_DEVICE_RAW "raw"
#define RAW_DEVICE(obj) OBJECT_CHECK(RawDeviceState, (obj), \
                                     TYPE_DEVICE_RAW)

static uint64_t raw_device_read(void *opaque, hwaddr offset, unsigned size)
{
    RawDeviceState *rawstate = (RawDeviceState *)opaque;
    return rawstate->plugin.read(offset, size);
}

static void raw_device_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    RawDeviceState *rawstate = (RawDeviceState *)opaque;
    rawstate->plugin.write(offset, value, size);
}

static const MemoryRegionOps raw_device_ops = {
    .read = raw_device_read,
    .write = raw_device_write,
};

static void raw_device_realize(DeviceState *dev, Error **errp)
{
    RawDeviceState *rawstate = RAW_DEVICE(dev);
    printf("file %s, addr %lx, size %lu\n", rawstate->file, rawstate->addr, rawstate->size);

    char *error = NULL;
    rawstate->plugin.handle = dlopen(rawstate->file, RTLD_NOW);
    if (!rawstate->plugin.handle)
    {
        if ((error = dlerror()) != NULL)
        {
            printf("loading plugin error: %s\n", error);
        }

        exit(EXIT_FAILURE);
    }

    *(void **)(&rawstate->plugin.init) = dlsym(rawstate->plugin.handle, "plugin_init");
    *(void **)(&rawstate->plugin.read) = dlsym(rawstate->plugin.handle, "plugin_read");
    *(void **)(&rawstate->plugin.write) = dlsym(rawstate->plugin.handle, "plugin_write");

    char *filename = basename(rawstate->file);
    char buf[1024];

    snprintf(buf, sizeof(buf), "%s --addr %#lX --size %#lX %s", filename, rawstate->addr, rawstate->size, rawstate->arg);
    printf("%s\n", buf);

    if (rawstate->plugin.init(buf))
    {
        printf("plugin init failed\n");
        exit(EXIT_FAILURE);
    }

    memory_region_init_io(&rawstate->mem_region, NULL,
                          &raw_device_ops, rawstate, "raw.device", rawstate->size);
    memory_region_add_subregion(get_system_memory(), rawstate->addr, &rawstate->mem_region);

}

static void raw_device_unrealize(DeviceState *dev)
{
}

static Property raw_device_props[] = {
    DEFINE_PROP_UINT64("addr", RawDeviceState, addr, 0),
    DEFINE_PROP_UINT64("size", RawDeviceState, size, 0),
    DEFINE_PROP_STRING("file", RawDeviceState, file),
    DEFINE_PROP_STRING("arg", RawDeviceState, arg),
    DEFINE_PROP_END_OF_LIST(),
};

static void raw_device_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = raw_device_realize;
    dc->unrealize = raw_device_unrealize;
    device_class_set_props(dc, raw_device_props);
    dc->desc = "Raw Device";
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
}

static TypeInfo raw_device_info = {
    .name = TYPE_DEVICE_RAW,
    .parent = TYPE_DEVICE,
    .instance_size = sizeof(RawDeviceState),
    .class_init = raw_device_class_init,
};

static void raw_device_register_type(void)
{
    type_register_static(&raw_device_info);
}

type_init(raw_device_register_type)
