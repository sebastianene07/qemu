#include "qemu/osdep.h"
#include "hw/char/nrf52_usart.h"
#include "hw/irq.h"
#include "hw/qdev-properties-system.h"
#include "qemu/log.h"
#include "qemu/module.h"

#define DB_PRINT_L(lvl, fmt, args...) do { \
    if (NRF52_USART_DEBUG >= (lvl)) { \
        qemu_log("%s: " fmt, __func__, ## args); \
    } \
} while (0)

#define DB_PRINT(fmt, args...) DB_PRINT_L(1, fmt, ## args)

// TODO - fill this with correct values
//
#define USART_ISR_RESET         (0)
#define USART_ISR_RXNE          (0)

// Registers definitions below this line UARTE
// Format: Register Offset Description

#define UARTE_TASKS_STARTRX       (0x000) // Start UART receiver
#define UARTE_TASKS_STOPRX        (0x004) // Stop UART receiver
#define UARTE_TASKS_STARTTX       (0x008) // Start UART transmitter
#define UARTE_TASKS_STOPTX        (0x00C) // Stop UART transmitter
#define UARTE_TASKS_FLUSHRX       (0x02C) // Flush RX FIFO into RX buffer

#define UARTE_EVENTS_CTS          (0x100) // CTS is activated (set low). Clear To Send.
#define UARTE_EVENTS_NCTS         (0x104) // CTS is deactivated (set high). Not Clear To Send.
#define UARTE_EVENTS_RXDRDY       (0x108) // Data received in RXD (but potentially not yet transferred to Data RAM)
#define UARTE_EVENTS_ENDRX        (0x110) // Receive buffer is filled up
#define UARTE_EVENTS_TXDRDY       (0x11C) // Data sent from TXD
#define UARTE_EVENTS_ENDTX        (0x120) // Last TX byte transmitted
#define UARTE_EVENTS_ERROR        (0x124) // Error detected
#define UARTE_EVENTS_RXTO         (0x144) // Receiver timeout
#define UARTE_EVENTS_RXSTARTED    (0x14C) // UART receiver has started
#define UARTE_EVENTS_TXSTARTED    (0x150) // UART transmitter has started
#define UARTE_EVENTS_TXSTOPPED    (0x158) // Transmitter stopped

#define UARTE_SHORTS              (0x200) // Shortcuts between local events and tasks
#define UARTE_INTEN               (0x300) // Enable or disable interrupt
#define UARTE_INTENSET            (0x304) // Enable interrupt
#define UARTE_INTENCLR            (0x308) // Disable interrupt
#define UARTE_ERRORSRC            (0x480) // Error source Note : this register is read / write one to clear.
#define UARTE_ENABLE              (0x500) // Enable UART
#define UARTE_PSEL_RTS            (0x508) // Pin select for RTS signal
#define UARTE_PSEL_TXD            (0x50C) // Pin select for TXD signal
#define UARTE_PSEL_CTS            (0x510) // Pin select for CTS signal
#define UARTE_PSEL_RXD            (0x514) // Pin select for RXD signal
#define UARTE_RXD                 (0x518) // RXD register
#define UARTE_TXD                 (0x51C) // TXD register
#define UARTE_BAUDRATE            (0x524) // Baud rate. Accuracy depends on the HFCLK source selected.
#define UARTE_RXD_PTR             (0x534) // Data pointer
#define UARTE_RXD_MAXCNT          (0x538) // Maximum number of bytes in receive buffer
#define UARTE_RXD_AMOUNT          (0x53C) // Number of bytes transferred in the last transaction
#define UARTE_TXD_PTR             (0x544) // Data pointer
#define UARTE_TXD_MAXCNT          (0x548) // Maximum number of bytes in transmit buffer
#define UARTE_TXD_AMOUNT          (0x54C) // Number of bytes transferred in the last transaction
#define UARTE_CONFIG              (0x56C) // Configuration of parity and hardware flow control

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
                          0x58C);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->mmio);
}

static uint64_t nrf52_usart_read(void *opaque, hwaddr addr,
                                 unsigned int size)
{
    NRF52UsartState *s = opaque;

//    DB_PRINT("Read 0x%"HWADDR_PRIx"\n", addr);

    switch (addr)
    {
        case UARTE_EVENTS_TXDRDY:
            return s->is_event_txrdy;
    }
    return 0;
}

static void nrf52_usart_write(void *opaque, hwaddr addr,
                              uint64_t val64, unsigned int size)
{
    NRF52UsartState *s = opaque;
    uint32_t value = val64;
    unsigned char ch;

    DB_PRINT("Write 0x%" PRIx32 ", 0x%"HWADDR_PRIx"\n", value, addr);

    switch (addr)
    {
        case UARTE_TXD:
            ch = val64;
            qemu_chr_fe_write_all(&s->chr, &ch, 1);
            s->is_event_txrdy = true;
            DB_PRINT("Wrote in TXD: %c\n", ch);
            break;

        case UARTE_EVENTS_TXDRDY:
            s->is_event_txrdy = (bool)value;
            DB_PRINT("Disable TX RDY\n");
            break;
    }
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
