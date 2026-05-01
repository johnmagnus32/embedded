/*
 * dbg_main.c — sim-dbg entry point
 *
 * GDB-like CLI debugger. Connects to sim-core's debug stub,
 * loads ELF for symbols/DWARF, provides interactive REPL.
 *
 * Usage: sim-dbg --connect host:port --elf firmware.elf
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dbg_client.h"
#include "dbg_cmd.h"
#include "elf_sym.h"

#ifdef HAVE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

static void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s --connect host:port --elf firmware.elf\n", prog);
    exit(1);
}

int main(int argc, char **argv)
{
    const char *connect_str = NULL;
    const char *elf_path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--connect") == 0 && i + 1 < argc)
            connect_str = argv[++i];
        else if (strcmp(argv[i], "--elf") == 0 && i + 1 < argc)
            elf_path = argv[++i];
        else
            usage(argv[0]);
    }

    if (!connect_str || !elf_path) usage(argv[0]);

    /* Parse host:port */
    char host[256] = "127.0.0.1";
    int port = 9001;
    char *colon = strrchr(connect_str, ':');
    if (colon) {
        int hlen = (int)(colon - connect_str);
        if (hlen > 0 && hlen < (int)sizeof(host)) {
            memcpy(host, connect_str, hlen);
            host[hlen] = '\0';
        }
        port = atoi(colon + 1);
    } else {
        port = atoi(connect_str);
    }

    /* Load ELF for symbols/DWARF (into dummy buffers — we don't run the CPU) */
    static uint8_t dummy_flash[1024*1024];
    static uint8_t dummy_ram[128*1024];
    if (elf_load(elf_path, dummy_flash, dummy_ram) != 0) {
        fprintf(stderr, "Failed to load ELF: %s\n", elf_path);
        return 1;
    }
    fprintf(stderr, "Loaded symbols from %s\n", elf_path);

    /* Connect to debug stub */
    struct dbg_client client;
    if (dbg_connect(&client, host, port) < 0) {
        fprintf(stderr, "Cannot connect to %s:%d\n", host, port);
        return 1;
    }
    fprintf(stderr, "Connected to %s:%d\n", host, port);

    /* Read initial stop info */
    char *init = dbg_recv(&client);
    if (init) {
        printf("Stopped: ");
        dbg_show_stop(init);
    }

    /* REPL */
#ifdef HAVE_READLINE
    while (1) {
        char *line = readline("(dbg) ");
        if (!line) break;
        if (*line) add_history(line);
        if (dbg_handle_command(&client, line) < 0) { free(line); break; }
        free(line);
    }
#else
    char line[1024];
    while (1) {
        printf("(dbg) ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;
        /* Strip newline */
        int len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
        if (dbg_handle_command(&client, line) < 0) break;
    }
#endif

    printf("Disconnected.\n");
    dbg_close(&client);
    return 0;
}
