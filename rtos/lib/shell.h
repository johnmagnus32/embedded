/*
 * shell.h — Interactive command shell
 *
 * Like Zephyr's subsys/shell:
 *   - Reads lines from UART
 *   - Parses command + arguments
 *   - Dispatches to registered command handlers
 *   - SHELL_CMD_REGISTER() adds commands from any file
 */

#ifndef SHELL_H
#define SHELL_H

#define SHELL_MAX_CMDS  16
#define SHELL_MAX_LINE  64
#define SHELL_MAX_ARGS  4

typedef int (*shell_cmd_fn)(int argc, char **argv);

struct shell_cmd {
    const char *name;
    const char *help;
    shell_cmd_fn handler;
};

/* Register a command (call before shell_task starts) */
void shell_register(const char *name, shell_cmd_fn handler, const char *help);

/* Shell task entry point (runs as a scheduler task) */
void shell_task(void);

/*
 * Static command registration macro.
 * In Zephyr this uses ELF sections (like DEVICE_DT_DEFINE).
 * We use a simpler init-time registration.
 */
#define SHELL_CMD_REGISTER(name, fn, help_str) \
    __attribute__((constructor)) \
    static void __shell_reg_##name(void) { shell_register(#name, fn, help_str); }

#endif
