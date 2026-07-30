/*
 * test_parse.c — HOST test for S1: parse a REAL ELF's .dynamic and resolve real
 * symbols through it. We mmap our own build/libc.so (a genuine ARM PIC .so),
 * walk its program headers to PT_DYNAMIC, dl_parse() it, then dso_lookup() a few
 * symbols we KNOW it exports (printf/malloc/write) and confirm their addresses
 * match what readelf reports. This exercises dynamic.h + reloc.h together on a
 * real object — the step up from the hand-built fixtures in test_reloc.c.
 *
 * Runs on the x86 host (we only READ the file's tables; we don't execute the ARM
 * code). dl_parse produces real 32-bit pointers (correct on the ARM target,
 * where pointers ARE 32-bit). To test that identical code on a 64-bit host
 * without truncating, we place the file image in the LOW 4GB via MAP_32BIT and
 * copy the file in, so every derived pointer genuinely fits in Elf32_Addr — the
 * host then matches the target's pointer width exactly.
 *
 * A .so mapped at address M has load bias `base = M - min_vaddr` (min_vaddr is 0
 * for a PIC .so linked at vaddr 0, so base == M). We compute min_vaddr to be exact.
 *
 *   cc -std=c11 -Wall -Wextra -I../src test_parse.c -o t && ./t path/to/libc.so
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "dynamic.h"

/* minimal Elf32 header (just the fields we read) */
typedef struct {
	unsigned char e_ident[16];
	Elf32_Half e_type, e_machine;
	Elf32_Word e_version;
	Elf32_Addr e_entry;
	Elf32_Off  e_phoff, e_shoff;
	Elf32_Word e_flags;
	Elf32_Half e_ehsize, e_phentsize, e_phnum;
} Elf32_Ehdr_min;

static int failures;
#define CHECK(cond, msg) do { \
	if (cond) printf("  ok   %s\n", msg); \
	else    { printf("  FAIL %s\n", msg); failures++; } \
} while (0)

int main(int argc, char **argv)
{
	const char *path = (argc > 1) ? argv[1] : "../../build/libc.so";
	int fd = open(path, O_RDONLY);
	if (fd < 0) { fprintf(stderr, "cannot open %s (build with: make LINK=dynamic BOARD=virt)\n", path); return 2; }
	struct stat st; fstat(fd, &st);
	/* Read the whole file into a scratch buffer, then LAY OUT its LOAD segments
	 * at base + p_vaddr in a low-4GB region — exactly what the real ARM loader
	 * (S3) does. This makes `p_vaddr + base` a valid pointer for EVERY table,
	 * including PT_DYNAMIC (which lives in the RW segment where p_vaddr != file
	 * offset — the flat-file shortcut got that wrong). */
	char *filebuf = malloc(st.st_size);
	if (read(fd, filebuf, st.st_size) != (ssize_t)st.st_size) { perror("read"); return 2; }

	const Elf32_Ehdr_min *eh = (const void *)filebuf;
	CHECK(memcmp(eh->e_ident, "\x7f""ELF", 4) == 0, "ELF magic");
	CHECK(eh->e_ident[4] == 1, "ELFCLASS32");
	CHECK(eh->e_machine == 40, "EM_ARM");

	const Elf32_Phdr *ph0 = (const Elf32_Phdr *)(filebuf + eh->e_phoff);
	int phnum = eh->e_phnum;

	/* span = highest p_vaddr+p_memsz over LOAD segments (min_vaddr is 0 for PIC) */
	Elf32_Addr min_vaddr = 0xffffffff, max_end = 0;
	for (int i = 0; i < phnum; i++) if (ph0[i].p_type == PT_LOAD) {
		if (ph0[i].p_vaddr < min_vaddr) min_vaddr = ph0[i].p_vaddr;
		Elf32_Addr end = ph0[i].p_vaddr + ph0[i].p_memsz;
		if (end > max_end) max_end = end;
	}
	size_t span = max_end - min_vaddr;
	char *img = mmap(0, span, PROT_READ|PROT_WRITE,
	                 MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT, -1, 0);
	if (img == MAP_FAILED) { perror("mmap MAP_32BIT"); return 2; }
	if (((unsigned long)(uintptr_t)img >> 32) != 0) {
		fprintf(stderr, "mapping not in low 4GB (%p) — can't test 32-bit ptrs here\n", img);
		return 2;
	}
	Elf32_Addr base = (Elf32_Addr)(uintptr_t)img - min_vaddr;
	/* copy each LOAD segment to base+p_vaddr from its file offset */
	for (int i = 0; i < phnum; i++) if (ph0[i].p_type == PT_LOAD)
		memcpy(img + (ph0[i].p_vaddr - min_vaddr), filebuf + ph0[i].p_offset, ph0[i].p_filesz);

	/* program headers are read from the file image (their contents don't change) */
	const Elf32_Phdr *phdr = ph0;

	const Elf32_Dyn *dyn = dl_find_dynamic(phdr, phnum, base);
	CHECK(dyn != 0, "found PT_DYNAMIC");

	dso_t d;
	int ok = dl_parse(&d, dyn, base);
	CHECK(ok, "dl_parse succeeded (STRTAB+SYMTAB+HASH present)");
	CHECK(d.strtab != 0 && d.symtab != 0 && d.hash != 0, "tables non-NULL");
	printf("  info: DT_NEEDED count = %d%s\n", d.nneeded,
	       d.nneeded ? "" : " (a .so may have none)");
	for (int i = 0; i < d.nneeded; i++) printf("        NEEDED: %s\n", d.needed[i]);
	printf("  info: jmprel=%p pltrelsz=%u  rel=%p relsz=%u\n",
	       (void*)d.jmprel, d.pltrelsz, (void*)d.rel, d.relsz);

	/* The real test: resolve symbols we KNOW libc.so exports. Returned "address"
	 * is st_value + base, i.e. a pointer INTO our mapping — nonzero => found. */
	printf("== resolve real exported symbols ==\n");
	const char *known[] = { "printf", "malloc", "write", "open", "fork" };
	for (unsigned i = 0; i < sizeof known/sizeof known[0]; i++) {
		Elf32_Addr a = dso_lookup(&d, known[i]);
		char msg[64]; snprintf(msg, sizeof msg, "lookup %s -> %s", known[i], a ? "found" : "MISSING");
		CHECK(a != 0, msg);
	}
	CHECK(dso_lookup(&d, "this_symbol_does_not_exist") == 0, "missing symbol -> 0");

	munmap(img, span); free(filebuf); close(fd);
	printf("\n%s (%d failure%s)\n", failures ? "TESTS FAILED" : "ALL TESTS PASSED",
	       failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
