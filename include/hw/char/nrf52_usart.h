#ifndef __NRF52_USART_H
#define __NRF52_USART_H

#include "hw/sysbus.h"
#include "chardev/char-fe.h"

#define TYPE_NRF52_USART "nrf52-usart"
#define NRF52_USART(obj) \
    OBJECT_CHECK(NRF52UsartState, (obj), TYPE_NRF52_USART)


// TODO - fill this with correct values
//
#define USART_ISR_RESET         (0)
#define USART_ISR_RXNE          (0)

typedef struct {
    uint32_t usart_isr;
    qemu_irq irq;
    MemoryRegion mmio;
    CharBackend chr;
} NRF52UsartState;

#endif /* __NRF52_USART_H */
