/*
 * dl_main.c — the heart of ld.so: parse the startup stack, map libc.so, resolve
 * every relocation, and jump to the real program.
 *
 * Called by _start (dl_entry.S) with `sp` pointing at the kernel's initial stack
 * block: [argc, argv[], NULL, envp[], NULL, auxv[]]. We extract the auxv entries
 * that describe the loaded program (AT_PHDR, AT_PHNUM, AT_ENTRY, AT_BASE), then:
 *   1. Find the program's PT_DYNAMIC and parse it (dl_parse).
 *   2. Open + map libc.so (the sole DT_NEEDED).
 *   3. Resolve the program's JUMP_SLOT/GLOB_DAT against libc.
 *   4. Jump to AT_ENTRY (the program's _start / crt0).
 *
 * Constraints: we run BEFORE the program's libc is usable, so we use only bare
 * syscalls (inline asm) and the pure headers from L2/S1 (no libc dependency).
 */
#include <stdint.h>
#include "elf32.h"
#include "dynamic.h"

/* ---- bare syscalls (no libc — we ARE the libc's loader) -------------------- */
static inline long raw_syscall3(long nr, long a0, long a1, long a2)
{
	register long r7 __asm__("r7") = nr;
	register long r0 __asm__("r0") = a0;
	register long r1 __asm__("r1") = a1;
	register long r2 __asm__("r2") = a2;
	__asm__ volatile("svc 0" : "+r"(r0) : "r"(r7),"r"(r1),"r"(r2) : "memory");
	return r0;
}
static inline long raw_syscall6(long nr, long a0, long a1, long a2,
                                long a3, long a4, long a5)
{
	register long r7 __asm__("r7") = nr;
	register long r0 __asm__("r0") = a0;
	register long r1 __asm__("r1") = a1;
	register long r2 __asm__("r2") = a2;
	register long r3 __asm__("r3") = a3;
	register long r4 __asm__("r4") = a4;
	register long r5 __asm__("r5") = a5;
	__asm__ volatile("svc 0" : "+r"(r0)
	                 : "r"(r7),"r"(r1),"r"(r2),"r"(r3),"r"(r4),"r"(r5) : "memory");
	return r0;
}

#define SYS_exit   1
#define SYS_read   3
#define SYS_write  4
#define SYS_open   5    /* legacy open (simpler than openat for a single file) */
#define SYS_close  6
#define SYS_mprotect 125
#define SYS_mmap2 192

#define PROT_READ  1
#define PROT_WRITE 2
#define PROT_EXEC  4
#define MAP_PRIVATE 2

static void dl_puts(const char *s)
{
	unsigned n = 0; while (s[n]) n++;
	raw_syscall3(SYS_write, 2 /* stderr */, (long)s, (long)n);
}


__attribute__((noreturn))
static void dl_die(const char *msg)
{
	dl_puts("ld.so: "); dl_puts(msg); dl_puts("\n");
	raw_syscall3(SYS_exit, 127, 0, 0);
	__builtin_unreachable();
}

/* ---- auxv tags we care about ----------------------------------------------- */
#define AT_NULL  0
#define AT_PHDR  3
#define AT_PHENT 4
#define AT_PHNUM 5
#define AT_BASE  7
#define AT_ENTRY 9

typedef struct {
	uint32_t a_type, a_val;
} Elf32_auxv_t;

/* ---- inline memset (no libc) ----------------------------------------------- */
static void *dl_memset(void *d, int c, unsigned n)
{
	unsigned char *p = d; while (n--) *p++ = (unsigned char)c; return d;
}

/* ---- mmap wrapper (page-sized, fd+offset in pages for mmap2) --------------- */
static void *dl_mmap(uint32_t addr, uint32_t len, int prot, int flags, int fd, uint32_t pgoff)
{
	long r = raw_syscall6(SYS_mmap2, (long)addr, (long)len, prot, flags, fd, (long)pgoff);
	if (r < 0 && r > -4096) return (void *)-1;  /* MAP_FAILED */
	return (void *)(uintptr_t)r;
}

/* ---- map a .so's LOAD segments into memory --------------------------------- *
 * Returns the load base (= mapped_addr - min_vaddr). Opens and closes the fd. */
/* Record an object's PT_GNU_RELRO: written only while relocating, write-protected after (dl_protect_relro). */
static void dl_find_relro(dso_t *d, const Elf32_Phdr *ph, int phnum, Elf32_Addr base)
{
	for (int i = 0; i < phnum; i++)
		if (ph[i].p_type == PT_GNU_RELRO) { d->relro = ph[i].p_vaddr + base; d->relrosz = ph[i].p_memsz; }
}
/* Make an object's RELRO region read-only: the pages wholly inside it (start rounded down, end rounded down, as
 * glibc does — the linker page-aligns the end, so none of the object's writable data shares a protected page). */
static void dl_protect_relro(const dso_t *d, const char *what)
{
	if (!d->relro) return;
	uint32_t start = d->relro & ~0xFFFu, end = (d->relro + d->relrosz) & ~0xFFFu;
	if (end > start && raw_syscall3(SYS_mprotect, (long)start, (long)(end - start), PROT_READ) != 0)
		dl_die(what);
}

static Elf32_Addr dl_map_so(const char *path, dso_t *d)
{
	long fd = raw_syscall3(SYS_open, (long)path, 0 /*O_RDONLY*/, 0);
	if (fd < 0) { dl_puts("ld.so: cannot open "); dl_die(path); }

	/* Read the ELF header (small fixed read, reuse the stack). */
	unsigned char ehdr_buf[52 + 8*32];       /* ELF header + up to 8 phdrs */
	long n = raw_syscall3(SYS_read, fd, (long)ehdr_buf, (long)sizeof ehdr_buf);
	(void)n;

	/* DANGER: we cannot use readelf-style parsing here because that needs
	 * full file image (we can only read sequentially / small chunks). Luckily the
	 * file is small and the phdr is in the first 52+32*n bytes. Parse the header. */
	const uint8_t *e = ehdr_buf;
	if (!(e[0]==0x7f && e[1]=='E' && e[2]=='L' && e[3]=='F'))
		dl_die("bad ELF magic on libc.so");
	uint32_t phoff   = (uint32_t)e[28] | ((uint32_t)e[29]<<8) | ((uint32_t)e[30]<<16) | ((uint32_t)e[31]<<24);
	/* e_phnum is a 2-byte field; read it byte-wise (our cc has no 2-byte load — a uint16_t* deref would
	 * pull 4 bytes and fold in e_shentsize). e_phentsize is known to be sizeof(Elf32_Phdr)=32, so skip it. */
	uint32_t phnum   = (uint32_t)e[44] | ((uint32_t)e[45]<<8);
	if (phnum > 8) phnum = 8;               /* clamp to our buffer */
	const Elf32_Phdr *ph = (const Elf32_Phdr *)(ehdr_buf + phoff);

	/* Compute the span [min_vaddr, max_end) and mmap the whole region, then lay
	 * in each LOAD segment. (Simple single-mmap; a real loader mmaps per-segment
	 * with proper permissions. For our small libc this is fine.) */
	/* Track the segment span. NB: avoid a 0xffffffff sentinel + unsigned `<` — our cc compares as SIGNED,
	 * so 0xffffffff reads as -1 and `vs < min_va` would never update. A `seen` flag sidesteps it (and is
	 * identical under GCC). Real p_vaddr values here are small + positive, so their signed compares are fine. */
	Elf32_Addr min_va = 0, max_end = 0; int seen = 0;
	for (int i = 0; i < phnum; i++) if (ph[i].p_type == PT_LOAD) {
		Elf32_Addr vs = ph[i].p_vaddr & ~0xFFF;
		Elf32_Addr ve = (ph[i].p_vaddr + ph[i].p_memsz + 0xFFF) & ~0xFFF;
		if (!seen || vs < min_va) min_va = vs;
		if (ve > max_end) max_end = ve;
		seen = 1;
	}
	uint32_t span = max_end - min_va;
	/* Reserve the address range (anon private RW, then we'll file-back each seg) */
	void *map = dl_mmap(0, span, PROT_READ|PROT_WRITE, MAP_PRIVATE | 0x20 /*ANON*/, -1, 0);
	if (map == (void *)-1) dl_die("mmap anon for libc.so");
	Elf32_Addr base = (Elf32_Addr)(uintptr_t)map - min_va;

	/* Now file-back each LOAD segment by re-mmapping at the right place. Actually
	 * simpler for a small object: just pread/lseek+read each segment in. We'll use
	 * mmap per segment for correctness (and it exercises the kernel's file-backed
	 * mmap on mainline). For our kernel (which doesn't have file-backed mmap yet)
	 * we'd fall back to read — but S2/S3 target mainline first per the plan. */
	for (int i = 0; i < phnum; i++) if (ph[i].p_type == PT_LOAD) {
		uint32_t off_pg  = (ph[i].p_offset >> 12);
		uint32_t va_base = (ph[i].p_vaddr & ~0xFFF) + base;
		uint32_t len     = (ph[i].p_vaddr + ph[i].p_memsz + 0xFFF) & ~0xFFF;
		len -= (ph[i].p_vaddr & ~0xFFF);
		int prot = PROT_READ;
		if (ph[i].p_flags & 1) prot |= PROT_EXEC;
		if (ph[i].p_flags & 2) prot |= PROT_WRITE;
		void *seg = dl_mmap(va_base, len, prot, MAP_PRIVATE | 0x10 /*FIXED*/, (int)fd, off_pg);
		if (seg == (void *)-1) dl_die("mmap libc.so segment");
		/* Zero the BSS tail: bytes [p_filesz, p_memsz) must be 0. The file-backed
		 * mmap only places p_filesz bytes of real file data; the rest of the page
		 * may be garbage. This is what the static kernel loader (elf_load) gets for
		 * free from zeroed pmm pages, but file-backed mmap doesn't zero the tail. */
		if (ph[i].p_memsz > ph[i].p_filesz) {
			uint8_t *bss_start = (uint8_t *)(uintptr_t)(ph[i].p_vaddr + ph[i].p_filesz + base);
			uint32_t bss_len = ph[i].p_memsz - ph[i].p_filesz;
			for (uint32_t b = 0; b < bss_len; b++) bss_start[b] = 0;
		}
	}
	raw_syscall3(SYS_close, fd, 0, 0);

	/* Parse the .dynamic of the mapped image. */
	const Elf32_Dyn *dyn = dl_find_dynamic(ph, phnum, base);
	if (!dyn) dl_die("no PT_DYNAMIC in libc.so");
	dl_memset(d, 0, sizeof *d);
	if (!dl_parse(d, dyn, base)) dl_die("dl_parse libc.so failed");
	dl_find_relro(d, ph, phnum, base);
	return base;
}

/* Apply one object's relocation table (REL or JMPREL) against the lookup scope; any unresolved entry is fatal. */
static void relocate_table(const dso_t *d, const Elf32_Rel *rel, Elf32_Word sz, const dso_t *const *scope, const char *what)
{
	if (!rel) return;
	int nrel = (int)(sz / sizeof(Elf32_Rel));
	for (int i = 0; i < nrel; i++)
		if (!reloc_apply(d, scope, 2, &rel[i]))
			dl_die(what);
}

/* ---- the main linker loop -------------------------------------------------- */
/* hidden visibility: _start's `bl _dl_main` resolves PC-relative, NOT through
 * the PLT — so we don't need a resolved GOT entry to call ourselves. This
 * eliminates the one JUMP_SLOT self-reference the linker would otherwise have. */
__attribute__((noreturn, used, visibility("hidden")))
void _dl_main(long *sp)
{
	/* 1. Parse the initial stack to get argc/argv/envp/auxv. */
	long argc = sp[0];
	char **argv = (char **)&sp[1];
	/* char **envp = argv + argc + 1; */
	/* walk past envp to the auxv */
	long *p = (long *)&argv[argc + 1];      /* first envp entry */
	while (*p) p++;                         /* skip to envp NULL */
	p++;                                    /* past the NULL -> auxv */

	Elf32_auxv_t *auxv = (Elf32_auxv_t *)p;
	const Elf32_Phdr *prog_phdr = 0;
	int prog_phnum = 0;
	Elf32_Addr prog_entry = 0;
	Elf32_Addr ld_base = 0;                  /* our own load base (AT_BASE) */
	for (Elf32_auxv_t *a = auxv; a->a_type != AT_NULL; a++) {
		switch (a->a_type) {
		case AT_PHDR:  prog_phdr  = (const Elf32_Phdr *)(uintptr_t)a->a_val; break;
		case AT_PHNUM: prog_phnum = (int)a->a_val; break;
		case AT_ENTRY: prog_entry = a->a_val; break;
		case AT_BASE:  ld_base    = a->a_val; break;
		}
	}
	(void)ld_base;   /* used later for self-lookup; currently no self-relocs */

	if (!prog_phdr || !prog_entry)
		dl_die("missing AT_PHDR or AT_ENTRY");

	dl_puts("ld.so: entry, argc=");
	char nbuf[4] = { '0'+(char)(argc%10), '\n', 0, 0 }; dl_puts(nbuf);

	/* 2. Compute the program's load bias, then parse its .dynamic.
	 *
	 * The bias is what we add to any link-time address in the ELF to get its
	 * real runtime address. The standard, PIE-correct way to find it: the kernel
	 * told us in AT_PHDR where the program headers ACTUALLY are in memory; the
	 * PT_PHDR program header records where they were LINKED to be. Their
	 * difference is the bias:
	 *     base = AT_PHDR (runtime) - PT_PHDR.p_vaddr (link-time)
	 *   - non-PIE ET_EXEC (ours): PT_PHDR.p_vaddr == AT_PHDR  => base = 0.
	 *   - PIE (ET_DYN): PT_PHDR.p_vaddr is a small offset, AT_PHDR is that plus
	 *                   the load address => base = the load address. (Correct,
	 *                   unlike the old code which forced 0 and broke PIE.)
	 * If there's no PT_PHDR (unusual), fall back to bias 0 — right for ET_EXEC. */
	Elf32_Addr prog_base = 0;
	for (int i = 0; i < prog_phnum; i++) {
		if (prog_phdr[i].p_type == PT_PHDR) {
			prog_base = (Elf32_Addr)(uintptr_t)prog_phdr - prog_phdr[i].p_vaddr;
			break;
		}
	}

	dso_t prog;
	{
		const Elf32_Dyn *dyn = dl_find_dynamic(prog_phdr, prog_phnum, prog_base);
		if (!dyn) dl_die("no PT_DYNAMIC in program");
		dl_memset(&prog, 0, sizeof prog);
		if (!dl_parse(&prog, dyn, prog_base)) dl_die("dl_parse program failed");
		dl_find_relro(&prog, prog_phdr, prog_phnum, prog_base);
	}

	/* 3. Map the program's dependency, read from its DT_NEEDED (NOT hardcoded).
	 * dl_parse already collected the NEEDED names into prog.needed[]. We resolve
	 * each against a fixed search dir ("/lib/"), like a real linker's default
	 * path. SCOPE: we support a single dependency (our libc); a program with more
	 * NEEDED entries would need a per-lib dso_t array + transitive resolution. */
	if (prog.nneeded < 1)
		dl_die("program has no DT_NEEDED (nothing to link against)");
	if (prog.nneeded > 1)
		dl_puts("ld.so: warning: >1 DT_NEEDED, only the first is mapped\n");

	char lib_path[128];
	{
		const char *dir = "/lib/";
		const char *name = prog.needed[0];      /* e.g. "libc.so" */
		int k = 0;
		for (const char *c = dir;  *c && k < (int)sizeof lib_path - 1; c++) lib_path[k++] = *c;
		for (const char *c = name; *c && k < (int)sizeof lib_path - 1; c++) lib_path[k++] = *c;
		lib_path[k] = '\0';
	}
	dso_t libc;
	dl_map_so(lib_path, &libc);
	dl_puts("ld.so: mapped "); dl_puts(lib_path); dl_puts("\n");

	/* 4-7. Relocate every object against ONE lookup scope — the program, then libc — so a name binds to the same
	 * definition everywhere: libc's references to `environ` reach the program's copy of it (R_ARM_COPY), a function
	 * pointer the program formed is its PLT entry wherever it's compared, and a program definition interposes on
	 * libc's own (`main`, errno). libc's relocations run first, so its data is final before the program's COPYs
	 * read it. */
	const dso_t *scope[2] = { &prog, &libc };
	relocate_table(&libc, libc.rel, libc.relsz, scope, "unresolved libc REL");
	relocate_table(&libc, libc.jmprel, libc.pltrelsz, scope, "unresolved libc JUMP_SLOT");
	relocate_table(&prog, prog.jmprel, prog.pltrelsz, scope, "unresolved program JUMP_SLOT");
	relocate_table(&prog, prog.rel, prog.relsz, scope, "unresolved program REL");
	dl_protect_relro(&libc, "mprotect libc.so RELRO failed");   /* relocating is done: its GOT etc. go read-only */
	dl_protect_relro(&prog, "mprotect program RELRO failed");

	dl_puts("ld.so: relocations done, jumping to program\n");

	/* 8. Jump to the program's entry (AT_ENTRY = its crt0._start). The stack is
	 * still the original kernel-provided one (sp -> argc/argv/envp/auxv). We must
	 * restore sp to its original value before jumping so _start sees the same
	 * layout the kernel prepared. `sp` was the arg we received as `long *sp`. */
	/* Restore the original SP (= `sp` arg) and branch to the program's entry.
	 * Cannot be done in pure C (C can't set sp); use inline asm. */
	/* Set sp to the original stack, then branch to the program's entry. Register-pinned operands + a
	 * verbatim single-instruction template per statement (the inline-asm form our cc supports: no %N
	 * substitution, no multi-line templates). r11 (frame ptr) survives the `mov sp`, so the second
	 * statement still reloads r1 from the frame. */
	register long _sp    __asm__("r0") = (long)sp;
	register long _entry __asm__("r1") = (long)prog_entry;
	__asm__ volatile("mov sp, r0" :: "r"(_sp) : "memory");
	__asm__ volatile("bx r1"      :: "r"(_entry) : "memory");
	__builtin_unreachable();
}
