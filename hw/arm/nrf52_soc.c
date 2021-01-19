#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "qemu/timer.h"

#include "hw/sysbus.h"
#include "hw/ssi/ssi.h"
#include "hw/arm/boot.h"
#include "hw/boards.h"
#include "hw/irq.h"
#include "hw/i2c/i2c.h"
#include "hw/arm/armv7m.h"
#include "hw/misc/unimp.h"
#include "hw/char/nrf52_usart.h"

#include "net/net.h"
#include "exec/address-spaces.h"
#include "sysemu/runstate.h"
#include "sysemu/sysemu.h"
#include "cpu.h"

#define SRAM_SIZE                   (0x20000000)
#define CODE_SIZE                   (0x20000000)

#define NRF52_IRQ_NUM               (64)
#define NRF52_HCLK_FRQ              (16000000)
#define NRF52_UART_PERIPHERAL       (2)

static void nrf52_soc_class_init(ObjectClass *oc, void *data);

static const TypeInfo g_nrf52_soc_type = {
    .name       = MACHINE_TYPE_NAME("nrf52840"),
    .parent     = TYPE_MACHINE,
    .class_init = nrf52_soc_class_init,
};

static const uint32_t g_usart_addr[NRF52_UART_PERIPHERAL] =
{
    0x40002000, 0x40028000
};

static const uint32_t g_usart_irq[NRF52_UART_PERIPHERAL] =
{
    18, 56
};

static void nrf52_soc_do_sys_reset(void *opaque, int n, int level)
{
    if (level) {
        qemu_system_reset_request(SHUTDOWN_CAUSE_GUEST_RESET);
    }
}

static void nrf52_soc_init(MachineState *ms)
{
    MemoryRegion *sram  = g_new(MemoryRegion, 1);
    MemoryRegion *code  = g_new(MemoryRegion, 1);
    MemoryRegion *system_memory = get_system_memory();

    /* board memory layout as described in nrf52840_ops_v0.5.pdf
     *
     *  -------- 0xFFFFFFFF
     *
     *  -------- 0x60000000
     *
     *  Peripheral
     *
     *  -------- 0x40000000
     *
     *  SRAM
     *
     *  -------- 0x20000000
     *
     *  CODE
     *
     *  -------  0x0
     */

    memory_region_init_ram(sram, NULL, "NRF52_SOC.sram", SRAM_SIZE, &error_fatal);
    memory_region_add_subregion(system_memory, 0x20000000, sram);

    memory_region_init_rom(code, NULL, "NRF52_SOC.code", CODE_SIZE, &error_fatal);
    memory_region_add_subregion(system_memory, 0x0, code);

    DeviceState *armv7m = qdev_new(TYPE_ARMV7M);
    qdev_prop_set_uint32(armv7m, "num-irq", NRF52_IRQ_NUM);
    qdev_prop_set_string(armv7m, "cpu-type", ms->cpu_type);
    qdev_prop_set_bit(armv7m, "enable-bitband", true);
    object_property_set_link(OBJECT(armv7m), "memory", OBJECT(get_system_memory()), &error_abort);

    /* Add the reset interrupt */

    sysbus_realize_and_unref(SYS_BUS_DEVICE(armv7m), &error_fatal);
    qdev_connect_gpio_out_named(armv7m, "SYSRESETREQ", 0, qemu_allocate_irq(&nrf52_soc_do_sys_reset, NULL, 0));

    /* Set the system CPU clock */

    system_clock_scale = NANOSECONDS_PER_SECOND / NRF52_HCLK_FRQ;

    /* Initialize the UART peripherals */

    for (int i = 0; i < NRF52_UART_PERIPHERAL; i++) {
        DeviceState *dev = qdev_new(TYPE_NRF52_USART);
        qdev_prop_set_chr(dev, "chardev", serial_hd(0));
        sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
        SysBusDevice *busdev = SYS_BUS_DEVICE(dev);
        sysbus_mmio_map(busdev, 0, g_usart_addr[i]);
        sysbus_connect_irq(busdev, 0, qdev_get_gpio_in(armv7m, g_usart_irq[i]));
    }

    armv7m_load_kernel(ARM_CPU(first_cpu), ms->kernel_filename, CODE_SIZE);
}

static void nrf52_soc_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    mc->desc         = "NRF52840 (ARM Cortex-M4)";
    mc->init         = nrf52_soc_init;
    mc->ignore_memory_transaction_failures = true;
    mc->default_cpu_type                   = ARM_CPU_TYPE_NAME("cortex-m4");
}

static void nrf52_soc_machine_init(void)
{
    type_register_static(&g_nrf52_soc_type);
}

type_init(nrf52_soc_machine_init);
