#ifndef GPIO_LINE_H
#define GPIO_LINE_H

typedef void (*gpio_handler_fn)(void *opaque, int level);

struct gpio_line {
    gpio_handler_fn handler;
    void *opaque;
};

static inline void gpio_set(struct gpio_line *line, int level)
{
    if (line->handler)
        line->handler(line->opaque, level);
}

#endif
