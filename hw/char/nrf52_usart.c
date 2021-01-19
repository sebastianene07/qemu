#include "qemu/osdep.h"
#include "hw/char/nrf52_usart.h"
#include "hw/irq.h"
#include "hw/qdev-properties-system.h"
#include "qemu/log.h"
#include "qemu/module.h"

static uint64_t nrf52_usart_read(void *opaque, hwaddr addr,
                                 unsigned int size);

static void nrf52_usart_write(void *opaque, hwaddr addr,
                              uint64_t val64, unsigned int size);

static const MemoryRegionOps nrf52_usart_ops = {
    .read  = nrf52_usart_read,
    .write = nrf52_usart_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static Property nrf52_usart_properties[] = {
    DEFINE_PROP_CHR("chardev", NRF52UsartState, chr),
    DEFINE_PROP_END_OF_LIST(),
};

static void nrf52_usart_init(Object *obj)
{
    NRF52UsartState *s = NRF52_USART(obj);

    sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->irq);

    memory_region_init_io(&s->mmio,
                          obj,
                          &nrf52_usart_ops,
                          s,
                          TYPE_NRF52_USART,
                          0x400);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->mmio);
}

static uint64_t nrf52_usart_read(void *opaque, hwaddr addr,
                                 unsigned int size)
{
    return 0;
}

static void nrf52_usart_write(void *opaque, hwaddr addr,
                              uint64_t val64, unsigned int size)
{
}


static int nrf52_usart_can_receive(void *opaque)
{
    NRF52UsartState *s = opaque;

    if (!(s->usart_isr & USART_ISR_RXNE)) {
        return 1;
    }

    return 0;
}

static void nrf52_usart_receive(void *opaque, const uint8_t *buf, int size)
{
}

static void nrf52_usart_realize(DeviceState *dev, Error **errp)
{
    NRF52UsartState *s = NRF52_USART(dev);

    qemu_chr_fe_set_handlers(&s->chr,
                             nrf52_usart_can_receive,
                             nrf52_usart_receive,
                             NULL,
                             NULL,
                             s,
                             NULL,
                             true);
}

static void nrf52_usart_reset(DeviceState *dev)
{
    NRF52UsartState *s = NRF52_USART(dev);

    s->usart_isr = USART_ISR_RESET;

    qemu_set_irq(s->irq, 0);
}

static void nrf52_usart_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = nrf52_usart_reset;
    device_class_set_props(dc, nrf52_usart_properties);
    dc->realize = nrf52_usart_realize;
}

static const TypeInfo nrf52_usart_info = {
    .name          = TYPE_NRF52_USART,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(NRF52UsartState),
    .instance_init = nrf52_usart_init,
    .class_init    = nrf52_usart_class_init,
};

static void nrf52_usart_register_types(void)
{
    type_register_static(&nrf52_usart_info);
}

type_init(nrf52_usart_register_types)
