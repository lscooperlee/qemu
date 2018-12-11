#ifndef LARM_H
#define LARM_H

#include "hw/char/serial.h"
#include "hw/arm/boot.h"
#include "hw/ptimer.h"
#include "cpu.h"

typedef struct LarmInterrupt {
    qemu_irq irqs[8];
    MemoryRegion controller;
} LarmInterrupt;

typedef struct LarmTimer {
    ptimer_state *ptimer[1];
    MemoryRegion controller;
} LarmTimer;

#define TYPE_LARM "larm"
#define LARM_SOC(obj) OBJECT_CHECK(LarmState, (obj), TYPE_LARM)

typedef struct LarmState {
    /*< private >*/
    DeviceState parent_obj;
    /*< public >*/

    ARMCPU cpu;
    LarmInterrupt intc;
    LarmTimer timer;
} LarmState;

#endif
