/*
 * dl_main.c — the heart of ld-gv3: parse the startup stack, map libc.so, resolve
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
	dl_puts("ld-gv3: "); dl_puts(msg); dl_puts("\n");
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
static Elf32_Addr dl_map_so(const char *path, dso_t *d)
{
	long fd = raw_syscall3(SYS_open, (long)path, 0 /*O_RDONLY*/, 0);
	if (fd < 0) { dl_puts("ld-gv3: cannot open "); dl_die(path); }

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
	uint32_t phoff   = *(uint32_t *)(e + 28);
	/* phent = *(uint16_t*)(e+42) — we know it's sizeof(Elf32_Phdr)=32, skip */
	uint16_t phnum   = *(uint16_t *)(e + 44);
	if (phnum > 8) phnum = 8;               /* clamp to our buffer */
	const Elf32_Phdr *ph = (const Elf32_Phdr *)(ehdr_buf + phoff);

	/* Compute the span [min_vaddr, max_end) and mmap the whole region, then lay
	 * in each LOAD segment. (Simple single-mmap; a real loader mmaps per-segment
	 * with proper permissions. For our small libc this is fine.) */
	Elf32_Addr min_va = 0xffffffff, max_end = 0;
	for (int i = 0; i < phnum; i++) if (ph[i].p_type == PT_LOAD) {
		Elf32_Addr vs = ph[i].p_vaddr & ~0xFFF;
		Elf32_Addr ve = (ph[i].p_vaddr + ph[i].p_memsz + 0xFFF) & ~0xFFF;
		if (vs < min_va) min_va = vs;
		if (ve > max_end) max_end = ve;
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
	return base;
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

	dl_puts("ld-gv3: entry, argc=");
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
	}

	/* 3. Map the program's dependency, read from its DT_NEEDED (NOT hardcoded).
	 * dl_parse already collected the NEEDED names into prog.needed[]. We resolve
	 * each against a fixed search dir ("/lib/"), like a real linker's default
	 * path. SCOPE: we support a single dependency (our libc); a program with more
	 * NEEDED entries would need a per-lib dso_t array + transitive resolution. */
	if (prog.nneeded < 1)
		dl_die("program has no DT_NEEDED (nothing to link against)");
	if (prog.nneeded > 1)
		dl_puts("ld-gv3: warning: >1 DT_NEEDED, only the first is mapped\n");

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
	dl_puts("ld-gv3: mapped "); dl_puts(lib_path); dl_puts("\n");

	/* 4. Resolve libc.so's own REL (GLOB_DAT for errno/environ — against the
	 * program, which provides them as global BSS symbols from crt/libc_start). */
	if (libc.rel && libc.relsz) {
		int nrel = (int)(libc.relsz / sizeof(Elf32_Rel));
		for (int i = 0; i < nrel; i++) {
			/* For GLOB_DAT in libc resolving against the program (errno/environ):
			 * look up in the PROGRAM's symtab, then fall back to libc itself. */
			if (!reloc_apply(&libc, &prog, &libc.rel[i]))
				if (!reloc_apply(&libc, &libc, &libc.rel[i]))
					dl_die("unresolved libc REL");
		}
	}

	/* 5. Resolve libc.so's PLT (JUMP_SLOT). Most resolve against libc itself
	 * (internal calls: malloc→sbrk, printf→write…), but `main` is defined in
	 * the PROGRAM — so try libc first, then fall back to the program. */
	if (libc.jmprel && libc.pltrelsz) {
		int nrel = (int)(libc.pltrelsz / sizeof(Elf32_Rel));
		for (int i = 0; i < nrel; i++) {
			if (!reloc_apply(&libc, &libc, &libc.jmprel[i]))
				if (!reloc_apply(&libc, &prog, &libc.jmprel[i]))
					dl_die("unresolved libc JUMP_SLOT");
		}
	}

	/* 6. Resolve the PROGRAM's PLT (JUMP_SLOT — calls into libc like printf). */
	if (prog.jmprel && prog.pltrelsz) {
		int nrel = (int)(prog.pltrelsz / sizeof(Elf32_Rel));
		for (int i = 0; i < nrel; i++) {
			if (!reloc_apply(&prog, &libc, &prog.jmprel[i]))
				dl_die("unresolved program JUMP_SLOT");
		}
	}

	/* 7. Resolve the program's REL (GLOB_DAT — typically none for our simple
	 * programs, but handle if present). */
	if (prog.rel && prog.relsz) {
		int nrel = (int)(prog.relsz / sizeof(Elf32_Rel));
		for (int i = 0; i < nrel; i++)
			reloc_apply(&prog, &libc, &prog.rel[i]);
	}

	dl_puts("ld-gv3: relocations done, jumping to program\n");

	/* 8. Jump to the program's entry (AT_ENTRY = its crt0._start). The stack is
	 * still the original kernel-provided one (sp -> argc/argv/envp/auxv). We must
	 * restore sp to its original value before jumping so _start sees the same
	 * layout the kernel prepared. `sp` was the arg we received as `long *sp`. */
	/* Restore the original SP (= `sp` arg) and branch to the program's entry.
	 * Cannot be done in pure C (C can't set sp); use inline asm. */
	__asm__ volatile(
		"mov sp, %0\n\t"
		"bx  %1\n\t"
		:: "r"(sp), "r"(prog_entry) : "memory"
	);
	__builtin_unreachable();
}
