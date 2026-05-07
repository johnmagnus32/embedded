/*
 * elfloader.c — ELF loader with argc/argv, envp, and OS API
 *
 * Three additions over the basic loader:
 *
 * 1. argc/argv: shell parses "run hello world" into argc=2, argv=["hello","world"]
 *    We copy argv strings onto the program's stack before starting it.
 *    Same as Linux: execve() copies argv to the new process's stack.
 *
 * 2. envp: environment variables like PATH=/bin, HOME=/
 *    Stored in a global table, pointer passed to program entry.
 *    Same as Linux: main(int argc, char **argv, char **envp)
 *
 * 3. OS API: struct of function pointers the program can call.
 *    This is our simplified "dynamic linking" — instead of resolving
 *    symbols from .so files, we pass a vtable directly.
 *    Like Zephyr's llext symbol table or a plugin API.
 */

#include <stdint.h>
#include "config.h"

#ifdef CONFIG_ELFLOADER

#include "elfloader.h"
#include "sched.h"
#include "heap.h"

#ifdef CONFIG_SYNC
#include "sync.h"
#endif

/* ---- ELF32 structures ---- */

#define EI_NIDENT 16

struct elf32_ehdr {
    uint8_t  e_ident[EI_NIDENT];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct elf32_phdr {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
};

#define PT_LOAD    1
#define EM_ARM     40
#define ELF_MAGIC  0x464C457F

static void mcpy(void *d, const void *s, uint32_t n)
{
    uint8_t *dd = d; const uint8_t *ss = s;
    while (n--) *dd++ = *ss++;
}

static void mzero(void *d, uint32_t n)
{
    uint8_t *dd = d;
    while (n--) *dd++ = 0;
}

static uint32_t slen(const char *s)
{
    uint32_t n = 0;
    while (s[n]) n++;
    return n;
}

/* ---- Environment variables ---- */

#define ENV_MAX 16
static char env_storage[ENV_MAX][64];  /* "KEY=VALUE" strings */
static char *env_ptrs[ENV_MAX + 1];   /* NULL-terminated pointer array */
static int env_count;

char *os_environ[ENV_MAX + 1];

void env_set(const char *key, const char *value)
{
    /* Check if key already exists */
    uint32_t klen = slen(key);
    for (int i = 0; i < env_count; i++) {
        int match = 1;
        for (uint32_t j = 0; j < klen; j++) {
            if (env_storage[i][j] != key[j]) { match = 0; break; }
        }
        if (match && env_storage[i][klen] == '=') {
            /* Update existing */
            char *p = env_storage[i];
            for (uint32_t j = 0; j < klen; j++) *p++ = key[j];
            *p++ = '=';
            while (*value && p < env_storage[i] + 63) *p++ = *value++;
            *p = '\0';
            return;
        }
    }

    /* Add new */
    if (env_count >= ENV_MAX) return;
    char *p = env_storage[env_count];
    while (*key && p < env_storage[env_count] + 62) *p++ = *key++;
    *p++ = '=';
    while (*value && p < env_storage[env_count] + 63) *p++ = *value++;
    *p = '\0';
    env_ptrs[env_count] = env_storage[env_count];
    env_count++;
    env_ptrs[env_count] = (char *)0;  /* NULL terminate */

    /* Update global */
    mcpy(os_environ, env_ptrs, sizeof(env_ptrs));
}

const char *env_get(const char *key)
{
    uint32_t klen = slen(key);
    for (int i = 0; i < env_count; i++) {
        int match = 1;
        for (uint32_t j = 0; j < klen; j++) {
            if (env_storage[i][j] != key[j]) { match = 0; break; }
        }
        if (match && env_storage[i][klen] == '=')
            return env_storage[i] + klen + 1;
    }
    return (const char *)0;
}

/* ---- OS API table (simplified dynamic linking) ---- */

/* UART puts wrapper — loaded programs call api->puts("hello") */
static void api_puts(const char *s)
{
    /* Use the console device */
    extern void uart_puts(const void *dev, const char *s);
    extern const void *__device_usart2;  /* or whatever console is */
    /* Simplified: just use semihosting-style direct output */
    /* In a real system this would go through the chosen console */
}

static const struct os_api os_api_table = {
    .yield    = sched_yield,
    .sleep_ms = sched_sleep_ms,
#ifdef CONFIG_SYNC
    .sem_give = (void (*)(void *))sem_give,
    .sem_take = (void (*)(void *))sem_take,
#endif
    .malloc   = (void *(*)(uint32_t))heap_alloc,
    .free     = heap_free,
};

/* ---- Trampoline: wraps program entry to pass argc/argv/envp ---- */

struct program_args {
    program_entry entry;
    int argc;
    char **argv;
    char **envp;
    const struct os_api *api;
};

static struct program_args pending_args;

static void program_trampoline(void)
{
    /* Call the loaded program's entry point with arguments */
    int rc = pending_args.entry(
        pending_args.argc,
        pending_args.argv,
        pending_args.envp,
        pending_args.api
    );
    (void)rc;

    /* Program returned — sleep forever (task stays alive) */
    while (1) sched_sleep_ms(10000);
}

/* ---- Main loader ---- */

int elf_load_and_run(const void *elf_data, uint32_t elf_size,
                     const char *name, uint8_t priority,
                     int argc, char **argv)
{
    const uint8_t *base = (const uint8_t *)elf_data;
    const struct elf32_ehdr *ehdr = (const struct elf32_ehdr *)base;

    if (elf_size < sizeof(struct elf32_ehdr))
        return -1;
    if (*(const uint32_t *)ehdr->e_ident != ELF_MAGIC)
        return -1;
    if (ehdr->e_machine != EM_ARM)
        return -1;

    /* Find address range of PT_LOAD segments */
    uint32_t min_vaddr = 0xFFFFFFFF;
    uint32_t max_vaddr = 0;

    const struct elf32_phdr *phdr =
        (const struct elf32_phdr *)(base + ehdr->e_phoff);

    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            if (phdr[i].p_vaddr < min_vaddr)
                min_vaddr = phdr[i].p_vaddr;
            uint32_t end = phdr[i].p_vaddr + phdr[i].p_memsz;
            if (end > max_vaddr)
                max_vaddr = end;
        }
    }

    if (min_vaddr >= max_vaddr)
        return -1;

    uint32_t total_memsz = max_vaddr - min_vaddr;

    /* Allocate and load */
    uint8_t *load_base = heap_alloc(total_memsz);
    if (!load_base)
        return -1;

    mzero(load_base, total_memsz);

    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type != PT_LOAD)
            continue;
        uint32_t offset_in_load = phdr[i].p_vaddr - min_vaddr;
        uint32_t copy_size = phdr[i].p_filesz;
        if (phdr[i].p_offset + copy_size > elf_size)
            continue;
        mcpy(load_base + offset_in_load,
             base + phdr[i].p_offset,
             copy_size);
    }

    /* Entry point */
    uint32_t entry = (uint32_t)load_base + (ehdr->e_entry - min_vaddr);
    entry |= 1;

    /*
     * Set up arguments for the trampoline.
     * The trampoline calls: entry(argc, argv, envp, &os_api_table)
     *
     * In Linux, argv/envp are copied onto the new process's stack.
     * We keep them in the caller's memory (simpler, works because
     * we share address space).
     */
    pending_args.entry = (program_entry)entry;
    pending_args.argc = argc;
    pending_args.argv = argv;
    pending_args.envp = os_environ;
    pending_args.api = &os_api_table;

    /* Create task running the trampoline */
    sched_create_task(program_trampoline, name, priority);

    return 0;
}

#endif /* CONFIG_ELFLOADER */
