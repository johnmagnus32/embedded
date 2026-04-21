/*
 * shell.c — Interactive UART shell
 *
 * Runs as a scheduler task. Reads characters from UART, builds a
 * line buffer, parses into argc/argv, dispatches to registered commands.
 *
 * Built-in commands: help, threads, devices
 */

#include "shell.h"
#include "config.h"
#include "devicetree.h"
#include "device.h"
#include "drivers/uart.h"
#ifdef CONFIG_SCHED
#include "sched.h"
#endif
#ifdef CONFIG_SYNC
#include "sync.h"
extern struct semaphore uart_rx_sem;
#endif
#ifdef CONFIG_ELFLOADER
#include "elfloader.h"
#include "fs.h"
#endif
#include <stdint.h>

DEVICE_DT_DECLARE(DT_CHOSEN_CONSOLE);

static struct shell_cmd cmds[SHELL_MAX_CMDS];
static int num_cmds;

static const struct device *con(void)
{
    return DEVICE_DT_GET(DT_CHOSEN_CONSOLE);
}

static void print(const char *s)
{
    uart_puts(con(), s);
}

void shell_register(const char *name, shell_cmd_fn handler, const char *help)
{
    if (num_cmds >= SHELL_MAX_CMDS) return;
    cmds[num_cmds].name = name;
    cmds[num_cmds].handler = handler;
    cmds[num_cmds].help = help;
    num_cmds++;
}

/* Simple string compare */
static int streq(const char *a, const char *b)
{
    while (*a && *b) { if (*a++ != *b++) return 0; }
    return *a == *b;
}

/* Parse line into argc/argv */
static int parse_args(char *line, char **argv, int max_args)
{
    int argc = 0;
    while (*line && argc < max_args) {
        while (*line == ' ') line++;
        if (!*line) break;
        argv[argc++] = line;
        while (*line && *line != ' ') line++;
        if (*line) *line++ = '\0';
    }
    return argc;
}

/* ---- Built-in commands ---- */

static int cmd_help(int argc, char **argv)
{
    (void)argc; (void)argv;
    print("Available commands:\n");
    for (int i = 0; i < num_cmds; i++) {
        print("  ");
        print(cmds[i].name);
        if (cmds[i].help) {
            print(" - ");
            print(cmds[i].help);
        }
        print("\n");
    }
    return 0;
}

#ifdef CONFIG_SCHED
static int cmd_threads(int argc, char **argv)
{
    (void)argc; (void)argv;
    /* sched doesn't expose thread list yet — just show current */
    print("Current task: ");
    print(sched_current_name());
    print("\n");
    return 0;
}
#endif

#ifdef CONFIG_ELFLOADER
/*
 * "run <filename>" — load an ELF from the filesystem and run it as a task.
 * Like Linux's execve() but simpler: doesn't replace the current task,
 * creates a new one.
 */
extern struct fs_mount mnt;  /* from main.c */

static int cmd_run(int argc, char **argv)
{
    if (argc < 2) {
        print("Usage: run <filename>\n");
        return -1;
    }

    /* Read ELF from filesystem into a buffer */
    struct fs_file f;
    if (fs_open(&f, &mnt, argv[1]) < 0) {
        print("File not found: ");
        print(argv[1]);
        print("\n");
        return -1;
    }

    /* Allocate buffer and read the whole file */
    uint8_t elf_buf[2048];  /* max program size */
    int size = fs_read(&f, elf_buf, sizeof(elf_buf));
    fs_close(&f);

    if (size <= 0) {
        print("Empty file\n");
        return -1;
    }

    print("Loading ");
    print(argv[1]);
    print("...\n");

    /* Load and run */
    int rc = elf_load_and_run(elf_buf, (uint32_t)size, argv[1], 4);
    if (rc < 0) {
        print("Failed to load ELF\n");
        return -1;
    }

    print("Started task: ");
    print(argv[1]);
    print("\n");
    return 0;
}
#endif

/* ---- Shell task ---- */

static void prompt(void)
{
    print("shell> ");
}

void shell_task(void)
{
    char line[SHELL_MAX_LINE];
    int pos = 0;

    /* Register built-in commands */
    shell_register("help", cmd_help, "Show available commands");
#ifdef CONFIG_SCHED
    shell_register("threads", cmd_threads, "Show thread info");
#endif
#ifdef CONFIG_ELFLOADER
    shell_register("run", cmd_run, "Load and run ELF: run <filename>");
#endif

    print("\n\nsimple-stm32 shell\nType 'help' for commands.\n\n");
    prompt();

    while (1) {
        /* Block until UART ISR signals data available */
#ifdef CONFIG_SYNC
        sem_take(&uart_rx_sem);
#endif
        char c;
        while (uart_poll_in(con(), &c) == 0) {
            if (c == '\r' || c == '\n') {
                uart_poll_out(con(), '\r');
                uart_poll_out(con(), '\n');
                line[pos] = '\0';

                if (pos > 0) {
                    /* Parse and dispatch */
                    char *argv[SHELL_MAX_ARGS];
                    int argc = parse_args(line, argv, SHELL_MAX_ARGS);

                    if (argc > 0) {
                        int found = 0;
                        for (int i = 0; i < num_cmds; i++) {
                            if (streq(argv[0], cmds[i].name)) {
                                cmds[i].handler(argc, argv);
                                found = 1;
                                break;
                            }
                        }
                        if (!found) {
                            print("Unknown command: ");
                            print(argv[0]);
                            print("\n");
                        }
                    }
                }
                pos = 0;
                prompt();
            } else if (c == 127 || c == '\b') {
                /* Backspace */
                if (pos > 0) {
                    pos--;
                    print("\b \b");
                }
            } else if (pos < SHELL_MAX_LINE - 1) {
                line[pos++] = c;
                uart_poll_out(con(), c);  /* echo */
            }
        }
    }
}
