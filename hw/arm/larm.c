
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu-common.h"
#include "cpu.h"
#include "hw/sysbus.h"
#include "hw/irq.h"
#include "exec/address-spaces.h"
#include "hw/arm/larm.h"

static void larm_interrupt_callback(void *opaque, int n, int level)
{
    printf("interrupt callback\n");
}
//interrupt controller
static uint64_t larm_ic_read(void *opaque, hwaddr offset, unsigned size)
{
    printf("ic read\n");
    return 0;
}

static void larm_ic_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    printf("ic write\n");
}

static const MemoryRegionOps larm_interrupt_controller_ops = {
    .read = larm_ic_read,
    .write = larm_ic_write,
};

/**
 * timer:               Base = 0xFF020000
 * Timer enable Reg:    offset = 0x0
 *                      Value = 1 for start timer, 0 for stop
 * Freqency Reg:        Offset = 0x4
 *                      Value = frequency in HZ
 * IRQ enable Reg:      offset = 0x8
 *                      Value = 1 for enable irq, 0 for disable irq
*/

static void larm_timer_callback(void *opaque) 
{
    LarmState *socstate = LARM_SOC(opaque);

    qemu_set_irq(socstate->intc.irqs[0], 1);

    printf("print timer callback\n");
}

static uint64_t larm_timer_read(void *opaque, hwaddr offset, unsigned size)
{
    printf("timer read\n");
    return 0;
}

static void larm_timer_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    LarmState *socstate = LARM_SOC(opaque);

    switch(offset) {
        case 0:
            printf("timer read 0\n");
            ptimer_run(socstate->timer.ptimer[0], 0);
            break;
        case 4:
            printf("timer read 4\n");
            ptimer_set_freq(socstate->timer.ptimer[0], value);
            ptimer_set_count(socstate->timer.ptimer[0], 1);
            ptimer_set_limit(socstate->timer.ptimer[0], 1, 1);
            break;
        case 8:
            printf("timer read 8\n");
            break;
        default:
            break;
    }
}

static const MemoryRegionOps larm_timer_controller_ops = {
    .read = larm_timer_read,
    .write = larm_timer_write,
};


static void larm_instance_init(Object *obj)
{
    LarmState *socstate = LARM_SOC(obj);

    /*object_initialize_child(obj, "cpu", &socstate->cpu, sizeof(socstate->cpu),*/
                            /*ARM_CPU_TYPE_NAME("cortex-a7"), &error_abort, NULL);*/
    object_initialize_child(obj, "cpu", &socstate->cpu, ARM_CPU_TYPE_NAME("cortex-a7"));

}
static void larm_realize(DeviceState *dev, Error **errp)
{
    LarmState *socstate = LARM_SOC(dev);
    Error *err = NULL;

    object_property_set_bool(OBJECT(&socstate->cpu), true, "realized", &err);
    if (err != NULL) {
        error_propagate(errp, err);
        return;
    }

    // interrupt
    memory_region_init_io(&socstate->intc.controller, OBJECT(socstate), 
            &larm_interrupt_controller_ops, socstate, "test", 0x400);
    memory_region_add_subregion(get_system_memory(), 0xFF0F0000, &socstate->intc.controller);
    int i;
    for(i=0; i<8; ++i){
        socstate->intc.irqs[i] = qemu_allocate_irq(larm_interrupt_callback, NULL, 1);
    }

    // timer
    memory_region_init_io(&socstate->timer.controller, OBJECT(socstate), 
            &larm_timer_controller_ops, socstate, "test", 0x400);
    memory_region_add_subregion(get_system_memory(), 0xFF020000, &socstate->timer.controller);
    socstate->timer.ptimer[0] = ptimer_init(larm_timer_callback, socstate, PTIMER_POLICY_DEFAULT);
}

static void larm_class_init(ObjectClass *cls, void *data)
{
    DeviceClass *deviceClass = DEVICE_CLASS(cls);

    deviceClass->realize = larm_realize;
}

static const TypeInfo larm_info = {
    .name          = TYPE_LARM,
    .parent        = TYPE_ARM_CPU,
    .instance_size = sizeof(LarmState),
    .instance_init = larm_instance_init,
    .class_init    = larm_class_init,
};

static void larm_register_types(void)
{
    type_register_static(&larm_info);
}

type_init(larm_register_types)
