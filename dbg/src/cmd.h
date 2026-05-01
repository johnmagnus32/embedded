/*
 * cmd.h — sim-dbg command handlers
 */
#ifndef DEBUGGER_DBG_CMD_H
#define DEBUGGER_DBG_CMD_H

struct dbg_client;

/* Handle one user command. Returns 0 to continue, -1 to quit. */
int dbg_handle_command(struct dbg_client *client, const char *line);

/* Display stop info from a JSON response */
void dbg_show_stop(const char *json);

#endif
