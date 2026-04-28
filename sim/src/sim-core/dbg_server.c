#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "dbg_server.h"
#include "dbg_cmd.h"
#include "cpu.h"
#include "elf_sym.h"

#define LOG(fmt, ...) fprintf(stderr, "[sim-core] " fmt "\n", ##__VA_ARGS__)

void send_response(int fd, const char *json)
{
    write(fd, json, strlen(json));
    write(fd, "\n", 1);
}

static void send_stop_info(int fd, struct cpu_state *cpu)
{
    uint32_t pc = cpu->r[REG_PC];
    uint32_t off;
    const char *fn = sym_lookup(pc, &off);
    int line;
    const char *file = line_lookup(pc, &line);
    char buf[512];
    snprintf(buf, sizeof(buf),
        "{\"stopped\":true,\"pc\":%u,\"line\":%d,\"func\":\"%s\",\"file\":\"%s\",\"cycles\":%lu}",
        pc, line, fn ? fn : "", file ? file : "", (unsigned long)cpu->cycle_count);
    send_response(fd, buf);
}

void dbg_server_run(struct sim_ctx *ctx, int port)
{
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_port = htons(port),
                                .sin_addr.s_addr = htonl(INADDR_LOOPBACK) };
    if (bind(srv, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG("Failed to bind debug port %d", port);
        exit(1);
    }
    listen(srv, 1);
    LOG("Debug server on port %d", port);

    int client = accept(srv, NULL, NULL);
    LOG("Debug client connected");

    send_stop_info(client, ctx->cpu);

    char buf[4096];
    int buf_len = 0;
    while (1) {
        int n = read(client, buf + buf_len, sizeof(buf) - buf_len - 1);
        if (n <= 0) break;
        buf_len += n;
        buf[buf_len] = '\0';

        char *nl;
        while ((nl = strchr(buf, '\n')) != NULL) {
            *nl = '\0';
            LOG("CMD: %s", buf);
            dbg_dispatch(client, ctx, buf);
            int remaining = buf_len - (nl - buf + 1);
            memmove(buf, nl + 1, remaining);
            buf_len = remaining;
            buf[buf_len] = '\0';
        }
    }

    close(client);
    close(srv);
}
