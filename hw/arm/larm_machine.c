
#include "qemu/osdep.h"
#include "qemu-common.h"
#include "qemu/error-report.h"
#include "hw/hw.h"
#include "hw/boards.h"
#include "exec/address-spaces.h"
#include "sysemu/sysemu.h"
#include "hw/arm/larm.h"

typedef struct LarmMachineState {
    LarmState *soc;
    MemoryRegion ram;
} LarmMachineState;


static void larm_machine_state_init(MachineState *machine)
{
    LarmMachineState *mstate = malloc(sizeof(LarmMachineState));
    Error *err = NULL;

    mstate->soc = LARM_SOC(object_new(TYPE_LARM));
    object_property_set_bool(OBJECT(mstate->soc), true, "realized", &err);

    //ram
    memory_region_init_ram(&mstate->ram, NULL, "ram", machine->ram_size, &err);
    memory_region_add_subregion(get_system_memory(), 0, &mstate->ram);

    if (machine->kernel_filename) {
        struct arm_boot_info larm_machine_boot_info = {
            .loader_start = 0x0,
            .kernel_filename = machine->kernel_filename,
        };
        arm_load_kernel(&mstate->soc->cpu, machine, &larm_machine_boot_info);
    }

    serial_mm_init(get_system_memory(), 0xFF010000, 2, NULL,
                   115200, serial_hd(0), DEVICE_NATIVE_ENDIAN);
}

static void larm_machine_init(MachineClass *mc)
{
    mc->desc = "LS board";
    mc->init = larm_machine_state_init;
//    mc->block_default_type = IF_PFLASH;
//    mc->default_ram_size = 1024 * 1024 * 1024;
}

DEFINE_MACHINE("larm_machine", larm_machine_init)
