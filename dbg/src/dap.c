/*
 * dap.c — Debug Adapter Protocol message framing
 *
 * DAP uses Content-Length headers over stdin/stdout (same as LSP).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dap.h"

char *dap_read(void)
{
    /* Read "Content-Length: N\r\n\r\n" */
    char header[256];
    int content_length = 0;
    while (fgets(header, sizeof(header), stdin)) {
        if (strncmp(header, "Content-Length:", 15) == 0) {
            content_length = atoi(header + 15);
        }
        /* Empty line (just \r\n) marks end of headers */
        if (strcmp(header, "\r\n") == 0 || strcmp(header, "\n") == 0)
            break;
    }
    if (content_length <= 0) return NULL;

    char *body = malloc(content_length + 1);
    if (!body) return NULL;
    int n = fread(body, 1, content_length, stdin);
    body[n] = '\0';
    return body;
}

void dap_write(const char *json)
{
    int len = strlen(json);
    fprintf(stdout, "Content-Length: %d\r\n\r\n%s", len, json);
    fflush(stdout);
}
