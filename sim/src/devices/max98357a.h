#ifndef MAX98357A_H
#define MAX98357A_H

#include <stdint.h>
#include <time.h>
#include "chardev.h"

struct stm32_dma;
struct stm32_dma_stream;
struct membus;
struct armv7m_nvic;

struct max98357a {
    struct chardev          *cd;
    int                      sample_rate;
    struct stm32_dma        *dma;
    int                      dma_stream_idx;
    struct stm32_dma_stream *dma_stream;
    struct membus           *bus;
    struct timespec          last_pull;
    int                      started;
};

void max98357a_init(struct max98357a *d);
void max98357a_tick(struct max98357a *d);

#endif
