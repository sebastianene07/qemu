#ifndef __NRF52_USART_H
#define __NRF52_USART_H

#include "hw/sysbus.h"
#include "chardev/char-fe.h"

#ifndef NRF52_USART_DEBUG
#define NRF52_USART_DEBUG 1
#endif

#define TYPE_NRF52_USART "nrf52-usart"
#define NRF52_USART(obj) \
    OBJECT_CHECK(NRF52UsartState, (obj), TYPE_NRF52_USART)


typedef struct {
    SysBusDevice parent_obj;
    MemoryRegion mmio;
    CharBackend chr;

    uint8_t rx_data;
    bool is_event_txrdy;
    uint32_t usart_isr;
    qemu_irq irq;

} NRF52UsartState;

#endif /* __NRF52_USART_H */
