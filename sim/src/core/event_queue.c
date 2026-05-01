/*
 * event_queue.c — Cycle-accurate event scheduler
 */
#include <string.h>
#include <stdio.h>
#include "event_queue.h"

void event_queue_init(struct event_queue *eq)
{
    memset(eq, 0, sizeof(*eq));
    eq->next_event = UINT64_MAX;
}

static void update_next(struct event_queue *eq)
{
    eq->next_event = UINT64_MAX;
    for (int i = 0; i < EVENT_QUEUE_MAX; i++)
        if (eq->events[i].active && eq->events[i].fire_at < eq->next_event)
            eq->next_event = eq->events[i].fire_at;
}

void event_schedule(struct event_queue *eq, int id, uint64_t fire_at,
                    event_callback_t callback, void *opaque)
{
    struct event *slot = NULL;
    /* Find existing event with this id, or an empty slot */
    for (int i = 0; i < EVENT_QUEUE_MAX; i++) {
        if (eq->events[i].active && eq->events[i].id == id) {
            slot = &eq->events[i];
            break;
        }
    }
    if (!slot) {
        for (int i = 0; i < EVENT_QUEUE_MAX; i++) {
            if (!eq->events[i].active) { slot = &eq->events[i]; break; }
        }
    }
    if (!slot) return;

    slot->fire_at = fire_at;
    slot->callback = callback;
    slot->opaque = opaque;
    slot->id = id;
    slot->active = 1;

    if (fire_at < eq->next_event)
        eq->next_event = fire_at;
}

void event_cancel(struct event_queue *eq, int id)
{
    for (int i = 0; i < EVENT_QUEUE_MAX; i++) {
        if (eq->events[i].active && eq->events[i].id == id) {
            eq->events[i].active = 0;
            break;
        }
    }
    update_next(eq);
}

void event_process(struct event_queue *eq, uint64_t current_cycle)
{
    for (int i = 0; i < EVENT_QUEUE_MAX; i++) {
        struct event *e = &eq->events[i];
        if (e->active && current_cycle >= e->fire_at) {
            e->active = 0;
            e->callback(e->opaque);
        }
    }
    update_next(eq);
}
