/*
 * stm32_spi.c — STM32 SPI peripheral register interface
 */
#include <string.h>
#include "stm32_spi.h"

#define SPI_CR1  0x00
#define SPI_CR2  0x04
#define SPI_SR   0x08
#define SPI_DR   0x0C

#define SR_TXE   (1 << 1)
#define SR_RXNE  (1 << 0)
#define SR_BSY   (1 << 7)

void stm32_spi_init(struct stm32_spi *s)
{
    s->cr1 = 0;
    s->cr2 = 0;
    s->sr = SR_TXE;
    memset(&s->bus, 0, sizeof(s->bus));
}

uint32_t stm32_spi_read(void *opaque, uint32_t offset)
{
    struct stm32_spi *s = (struct stm32_spi *)opaque;
    switch (offset) {
    case SPI_CR1: return s->cr1;
    case SPI_CR2: return s->cr2;
    case SPI_SR:  return s->sr;
    case SPI_DR:  s->sr &= ~SR_RXNE; return 0;
    default:      return 0;
    }
}

void stm32_spi_write(void *opaque, uint32_t offset, uint32_t val)
{
    struct stm32_spi *s = (struct stm32_spi *)opaque;
    switch (offset) {
    case SPI_CR1: s->cr1 = val; break;
    case SPI_CR2: s->cr2 = val; break;
    case SPI_DR:
        spi_bus_transfer(&s->bus, (uint8_t)val);
        s->sr |= SR_TXE | SR_RXNE;
        s->sr &= ~SR_BSY;
        break;
    }
}
