#include "signal_trace.h"
#include <stdlib.h>
#include <string.h>

struct signal_trace *signal_trace_create(const char *path, uint64_t *cycle_ptr)
{
    if (!path) return NULL;
    FILE *fp = fopen(path, "w");
    if (!fp) return NULL;
    fprintf(fp, "cycle,type,value\n");
    struct signal_trace *t = calloc(1, sizeof(*t));
    t->fp = fp;
    t->cycle_ptr = cycle_ptr;
    return t;
}

void signal_trace_close(struct signal_trace *t)
{
    if (!t) return;
    fclose(t->fp);
    free(t);
}
