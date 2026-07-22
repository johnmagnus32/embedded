#include <stdio.h>
#include <string.h>
#include "machine.h"

extern const struct machine_desc gameboy_machine;
extern const struct machine_desc gameboy_v2_machine;
extern const struct machine_desc fpga_test_machine;

static const struct machine_desc *machines[] = {
    &gameboy_machine,
    &gameboy_v2_machine,
    &fpga_test_machine,
    NULL
};

const struct machine_desc *machine_find(const char *name)
{
    for (int i = 0; machines[i]; i++)
        if (strcmp(machines[i]->name, name) == 0)
            return machines[i];
    return NULL;
}

