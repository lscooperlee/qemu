#include "qemu/osdep.h"
#include "qemu-common.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "hw/hw.h"
#include "hw/irq.h"
#include "hw/boards.h"
#include "hw/ptimer.h"
#include "hw/arm/boot.h"
#include "chardev/char-fe.h"
#include "sysemu/sysemu.h"
#include "exec/address-spaces.h"
#include "cpu.h"
#include "hw/sysbus.h"
#include "hw/char/serial.h"

#include "hw/arm/larm_one.h"

typedef struct LarmInterrupt
{
    qemu_irq irq;
    uint8_t isEnabled;
    uint8_t irqSource;
    MemoryRegion controller;
} LarmInterrupt;

typedef struct LarmUart
{
    CharBackend backend;
    MemoryRegion controller;
    uint8_t rxData;
    uint32_t rxReady;
    uint32_t txReady;
} LarmUart;

typedef struct LarmTimer
{
    ptimer_state *ptimer[1];
    MemoryRegion controller;
} LarmTimer;

typedef struct LarmState
{
    ARMCPU *cpu;
    LarmUart uart;
    LarmTimer timer;
    LarmInterrupt intc;
} LarmState;

typedef struct LarmMachineState
{
    LarmState soc;
    MemoryRegion ram;
} LarmMachineState;

static void larm_set_irq(LarmState *socstate, int n)
{
    socstate->intc.irqSource |= n;
    qemu_set_irq(socstate->intc.irq, 1);
}

static void larm_reset_irq(LarmState *socstate)
{
    qemu_set_irq(socstate->intc.irq, 0);
}

static void larm_interrupt_callback(void *opaque, int n, int level)
{
    LarmState *socstate = (LarmState *)opaque;
    if (level)
    {
        cpu_interrupt(CPU(socstate->cpu), CPU_INTERRUPT_HARD);
    }
    else
    {
        cpu_reset_interrupt(CPU(socstate->cpu), CPU_INTERRUPT_HARD);
    }
}

static uint64_t larm_ic_read(void *opaque, hwaddr offset, unsigned size)
{
    LarmState *socstate = (LarmState *)opaque;
    uint64_t ret;

    switch (offset)
    {
    case LARM_INTERRUPT_OFFSET_SOURCE:
        ret = socstate->intc.irqSource;
        break;
    case LARM_INTERRUPT_OFFSET_ENABLE:
        ret = socstate->intc.isEnabled;
        break;
    default:
        break;
    }
    return ret;
}

static void larm_ic_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    LarmState *socstate = (LarmState *)opaque;

    switch (offset)
    {
    case LARM_INTERRUPT_OFFSET_SOURCE:
        socstate->intc.irqSource = (uint8_t)value;
        larm_reset_irq(socstate);
        break;
    case LARM_INTERRUPT_OFFSET_ENABLE:
        socstate->intc.isEnabled = (uint8_t)value;
        break;
    default:
        break;
    }
}

static const MemoryRegionOps larm_interrupt_controller_ops = {
    .read = larm_ic_read,
    .write = larm_ic_write,
};

static void larm_interrupte_init(LarmState *socstate)
{

    memory_region_init_io(&socstate->intc.controller, NULL,
                          &larm_interrupt_controller_ops, socstate, "interrupt", 0x400);
    memory_region_add_subregion(get_system_memory(), LARM_INTERRUPT_REG_BASE, &socstate->intc.controller);

    socstate->intc.irq = qemu_allocate_irq(larm_interrupt_callback, socstate, 0);
    socstate->intc.isEnabled = 0;
    socstate->intc.irqSource = 0;
}

static uint64_t larm_uart_read(void *opaque, hwaddr offset, unsigned size)
{
    LarmState *socstate = (LarmState *)opaque;
    uint64_t ret = 0;

    switch (offset)
    {
    case LARM_UART_OFFSET_RTX:
        ret = socstate->uart.rxData;
        socstate->uart.rxReady = 0;
        break;
    case LARM_UART_OFFSET_RX_READY:
        ret = socstate->uart.rxReady;
        break;
    case LARM_UART_REG_TX_READY:
        ret = socstate->uart.txReady;
        break;
    default:
        break;
    }
    return ret;
}

static void larm_uart_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    LarmState *socstate = (LarmState *)opaque;
    uint8_t buf;

    switch (offset)
    {
    case LARM_UART_OFFSET_RTX:
        buf = (uint8_t)value;
        if (socstate->uart.txReady == 1)
        {
            socstate->uart.txReady = 0;
            qemu_chr_fe_write_all(&socstate->uart.backend, &buf, 1);
            socstate->uart.txReady = 1;
        }
        break;
    case LARM_UART_OFFSET_BAUDRATE:
        break;
    default:
        break;
    }
}

static const MemoryRegionOps larm_uart_controller_ops = {
    .read = larm_uart_read,
    .write = larm_uart_write,
};

static int larm_uart_can_receive(void *opaque)
{
    return 1;
}

static void larm_uart_receive(void *opaque, const uint8_t *buf, int size)
{
    LarmState *socstate = (LarmState *)opaque;
    if (socstate->uart.rxReady == 0)
    {
        socstate->uart.rxData = buf[0];
        if (socstate->intc.isEnabled & LARM_INTERRUPT_UART)
        {
            larm_set_irq(socstate, LARM_INTERRUPT_UART);
        }
        socstate->uart.rxReady = 1;
    }
}
static void larm_uart_init(LarmState *socstate)
{

    qemu_chr_fe_init(&socstate->uart.backend, serial_hd(0), &error_abort);
    qemu_chr_fe_set_handlers(&socstate->uart.backend, larm_uart_can_receive, larm_uart_receive,
                             NULL, NULL, socstate, NULL, true);

    memory_region_init_io(&socstate->uart.controller, NULL,
                          &larm_uart_controller_ops, socstate, "larmone.uart", 0x400);
    memory_region_add_subregion(get_system_memory(), LARM_UART_REG_BASE, &socstate->uart.controller);

    socstate->uart.rxReady = 0;
    socstate->uart.txReady = 1;
}

static void larm_timer_callback(void *opaque)
{
    LarmState *socstate = (LarmState *)opaque;

    if (socstate->intc.isEnabled & LARM_INTERRUPT_TIMER)
    {
        larm_set_irq(socstate, LARM_INTERRUPT_TIMER);
    }
}

static uint64_t larm_timer_read(void *opaque, hwaddr offset, unsigned size)
{
    LarmState *socstate = (LarmState *)opaque;
    uint64_t value;

    switch (offset)
    {
    case LARM_TIMER_OFFSET_COUNTER:
        value = ptimer_get_count(socstate->timer.ptimer[0]);
        break;
    default:
        break;
    }
    return value;
}

static void larm_timer_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    LarmState *socstate = (LarmState *)opaque;

    ptimer_transaction_begin(socstate->timer.ptimer[0]);

    switch (offset)
    {
    case LARM_TIMER_OFFSET_ENABLE:
        ptimer_run(socstate->timer.ptimer[0], 0);
        break;
    case LARM_TIMER_OFFSET_FREQ:
        ptimer_set_freq(socstate->timer.ptimer[0], value);
        break;
    case LARM_TIMER_OFFSET_COUNTER:
        ptimer_set_count(socstate->timer.ptimer[0], value);
        ptimer_set_limit(socstate->timer.ptimer[0], value, 1);
        break;
    default:
        break;
    }

    ptimer_transaction_commit(socstate->timer.ptimer[0]);
}

static const MemoryRegionOps larm_timer_controller_ops = {
    .read = larm_timer_read,
    .write = larm_timer_write,
};

static void larm_timer_init(LarmState *socstate)
{

    memory_region_init_io(&socstate->timer.controller, NULL,
                          &larm_timer_controller_ops, socstate, "timer", 0x400);
    memory_region_add_subregion(get_system_memory(), LARM_TIMER_REG_BASE, &socstate->timer.controller);
    socstate->timer.ptimer[0] = ptimer_init(larm_timer_callback, socstate, PTIMER_POLICY_DEFAULT);
}

static void larm_state_init(LarmState *socstate)
{

    socstate->cpu = ARM_CPU(cpu_create(ARM_CPU_TYPE_NAME("cortex-a7")));

    // uart
    larm_uart_init(socstate);

    // timer
    larm_timer_init(socstate);

    // interrupt
    larm_interrupte_init(socstate);
}

static struct arm_boot_info larm_machine_boot_info = {
    //.loader_start = 0x007F8000, //for no mmu if phys addr is 0x00800000, then entry will be 0x00808000, //
    //for mmu without Patch physical to virtual translations at runtime,
    //    .loader_start = 0x00FF8000, //for mmu with Patch physical to virtual translations at runtime, it has to be 16MB aligned. entry is 0x01008000
    //.loader_start = 0x01000000,
    .board_id = 0x1f41,
};

static void larm_machine_state_init(MachineState *machine)
{
    LarmMachineState *mstate = malloc(sizeof(LarmMachineState));

    larm_state_init(&mstate->soc);

    //ram
    memory_region_init_ram(&mstate->ram, NULL, "ram", machine->ram_size, NULL);
    memory_region_add_subregion(get_system_memory(), 0, &mstate->ram);

    if (machine->kernel_filename)
    {
        larm_machine_boot_info.ram_size = machine->ram_size;
        larm_machine_boot_info.kernel_filename = machine->kernel_filename;
        larm_machine_boot_info.kernel_cmdline = machine->kernel_cmdline;
        larm_machine_boot_info.initrd_filename = machine->initrd_filename;
        arm_load_kernel(mstate->soc.cpu, machine, &larm_machine_boot_info);
    }
}

static void larm_machine_init(MachineClass *mc)
{
    mc->desc = "LS board";
    mc->init = larm_machine_state_init;
}

DEFINE_MACHINE("larm_one", larm_machine_init)
