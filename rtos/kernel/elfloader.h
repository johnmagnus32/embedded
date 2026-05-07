/*
 * elfloader.h — Load and execute ELF binaries
 *
 * Supports:
 *   - argc/argv passing (like Linux's execve)
 *   - Environment variables (like Linux's envp)
 *   - Symbol table for calling OS functions (simplified dynamic linking)
 */

#ifndef ELFLOADER_H
#define ELFLOADER_H

#include <stdint.h>

/*
 * Symbol table entry — lets loaded programs call OS functions.
 *
 * This is our simplified version of dynamic linking. Instead of
 * a full .so loader with GOT/PLT, we provide a table of function
 * pointers that the loaded program can index into.
 *
 * Linux: ld.so resolves symbols from .so files at load time
 * Zephyr llext: resolves symbols against the kernel's symbol table
 * Ours: pass a struct of function pointers to the program's entry
 */
struct os_api {
    void (*yield)(void);
    void (*sleep_ms)(uint32_t ms);
    void (*sem_give)(void *sem);
    void (*sem_take)(void *sem);
    void (*puts)(const char *s);
    void *(*malloc)(uint32_t size);
    void (*free)(void *ptr);
};

/* Program entry point signature — receives argc/argv/envp + OS API */
typedef int (*program_entry)(int argc, char **argv, char **envp,
                             const struct os_api *api);

/* Environment variable (key=value pairs, NULL-terminated array) */
extern char *os_environ[];

/* Set an environment variable */
void env_set(const char *key, const char *value);

/* Get an environment variable */
const char *env_get(const char *key);

/* Load an ELF and run it with arguments */
int elf_load_and_run(const void *elf_data, uint32_t elf_size,
                     const char *name, uint8_t priority,
                     int argc, char **argv);

#endif
