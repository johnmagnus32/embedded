#ifndef DBG_CMD_H
#define DBG_CMD_H

#include "dbg_server.h"

void dbg_dispatch(int fd, struct sim_ctx *ctx, const char *line);
void dbg_cmd_set_src_dir(const char *dir);

#endif
