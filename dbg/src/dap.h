/*
 * dap.h — Debug Adapter Protocol message framing
 */
#ifndef DAP_H
#define DAP_H

/* Read one DAP message from stdin. Returns malloc'd JSON string, or NULL on EOF. */
char *dap_read(void);

/* Write a DAP message to stdout. */
void dap_write(const char *json);

#endif
