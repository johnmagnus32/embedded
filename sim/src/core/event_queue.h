/*
 * event_queue.h — Cycle-accurate event scheduler
 *
 * Devices schedule callbacks at specific cycle counts instead of
 * being polled every tick. One comparison per tick replaces N device checks.
 */
#ifndef EVENT_QUEUE_H
#define EVENT_QUEUE_H

#include <stdint.h>

typedef void (*event_callback_t)(void *opaque);

#define EVENT_QUEUE_MAX 32

enum event_id {
    EVT_SYSTICK = 1,
    EVT_SPI0_TXE,
    EVT_SPI1_TXE,
    EVT_ILI9341_REFRESH,
    EVT_IO_POLL,
};

struct event {
    uint64_t fire_at;
    event_callback_t callback;
    void *opaque;
    int id;
    int active;
};

struct event_queue {
    struct event events[EVENT_QUEUE_MAX];
    int count;
    uint64_t next_event;
};

void event_queue_init(struct event_queue *eq);
void event_schedule(struct event_queue *eq, int id, uint64_t fire_at,
                    event_callback_t callback, void *opaque);
void event_cancel(struct event_queue *eq, int id);
void event_process(struct event_queue *eq, uint64_t current_cycle);

#endif
