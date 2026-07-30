/*
 * test_reloc.c — HOST (x86) unit tests for the pure linker logic in reloc.h.
 *
 * WHY: the relocation math + symbol hash lookup are the algorithmically tricky
 * part of the linker, and inside a bare ARM process (no libc, no debugger) they
 * are miserable to debug. Here we build small ELF symbol/hash/relocation
 * fixtures by hand and assert the results on the host, where gdb + printf work.
 * The SAME reloc.h runs on the ARM target unchanged.
 *
 *   cc -std=c11 -Wall -Wextra -I../src test_reloc.c -o test_reloc && ./test_reloc
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "reloc.h"

static int failures;
#define CHECK(cond, msg) do { \
	if (cond) { printf("  ok   %s\n", msg); } \
	else      { printf("  FAIL %s\n", msg); failures++; } \
} while (0)

/* ---- build a tiny DSO fixture: a string table, symbol table, and a SysV hash
 * table exporting a few symbols. Mirrors what a real .so's .dynamic points at. */

/* string table: index 0 is the empty string by convention. */
static char STR[128];
static Elf32_Sym SYM[8];
static uint32_t HASH[64];

/* add a name to STR, return its offset */
static Elf32_Word add_str(Elf32_Word *cur, const char *s)
{
	Elf32_Word at = *cur;
	strcpy(&STR[at], s);
	*cur += (Elf32_Word)strlen(s) + 1;
	return at;
}

/* Build a fixture DSO exporting: printf@0x1000, malloc@0x2000, write@0x3000
 * (all "defined" here), plus an imported symbol "extern_undef" (SHN_UNDEF). */
static dso_t build_fixture(void)
{
	memset(STR, 0, sizeof STR);
	memset(SYM, 0, sizeof SYM);
	memset(HASH, 0, sizeof HASH);
	Elf32_Word cur = 1;   /* leave STR[0] = '\0' */

	/* symbol 0 is the reserved undefined entry */
	SYM[0] = (Elf32_Sym){0};
	struct { const char *n; Elf32_Addr v; int undef; } defs[] = {
		{ "printf", 0x1000, 0 },
		{ "malloc", 0x2000, 0 },
		{ "write",  0x3000, 0 },
		{ "extern_undef", 0, 1 },
	};
	int n = 1;
	for (unsigned i = 0; i < sizeof defs / sizeof defs[0]; i++, n++) {
		SYM[n].st_name  = add_str(&cur, defs[i].n);
		SYM[n].st_value = defs[i].v;
		SYM[n].st_info  = (STB_GLOBAL << 4) | STT_FUNC;
		SYM[n].st_shndx = defs[i].undef ? SHN_UNDEF : 1;
	}
	int nsym = n;

	/* SysV hash: [nbucket][nchain][bucket...][chain...]. Use nbucket=nchain=nsym
	 * with 1 bucket-per-index chaining (simple but valid). */
	uint32_t nbucket = (uint32_t)nsym;
	uint32_t nchain  = (uint32_t)nsym;
	HASH[0] = nbucket;
	HASH[1] = nchain;
	uint32_t *bucket = &HASH[2];
	uint32_t *chain  = &bucket[nbucket];
	for (uint32_t i = 0; i < nbucket; i++) bucket[i] = 0;
	for (uint32_t i = 0; i < nchain;  i++) chain[i]  = 0;
	/* insert each defined+undef symbol into the hash chains */
	for (int i = 1; i < nsym; i++) {
		const char *nm = &STR[SYM[i].st_name];
		uint32_t b = elf_hash(nm) % nbucket;
		chain[i] = bucket[b];      /* push onto the chain head */
		bucket[b] = (uint32_t)i;
	}

	dso_t d = {0};
	d.base = 0;
	d.symtab = SYM;
	d.strtab = STR;
	d.hash = HASH;
	d.syment = sizeof(Elf32_Sym);
	return d;
}

int main(void)
{
	dso_t lib = build_fixture();

	printf("== symbol lookup ==\n");
	CHECK(dso_lookup(&lib, "printf") == 0x1000, "lookup printf -> 0x1000");
	CHECK(dso_lookup(&lib, "malloc") == 0x2000, "lookup malloc -> 0x2000");
	CHECK(dso_lookup(&lib, "write")  == 0x3000, "lookup write  -> 0x3000");
	CHECK(dso_lookup(&lib, "nope")   == 0,      "lookup missing -> 0");
	CHECK(dso_lookup(&lib, "extern_undef") == 0, "undef symbol not treated as a definition");

	/* Reloc VALUE math is pure arithmetic over (type, base, S, addend) — test it
	 * with plain integers (no fake pointers, so it's clean on a 64-bit host). The
	 * memory-poking reloc_apply() is exercised for real on the ARM target by the
	 * boot harness; here we lock down the formulas. */
	printf("== R_ARM_JUMP_SLOT (value = S, addend ignored) ==\n");
	{
		Elf32_Addr S = dso_lookup(&lib, "printf");   /* 0x1000 */
		reloc_value_t rv = reloc_value(R_ARM_JUMP_SLOT, /*base*/0, S, /*A*/0xdeadbeef);
		CHECK(rv.ok && rv.value == 0x1000, "JUMP_SLOT printf -> 0x1000 (addend ignored)");
	}

	printf("== R_ARM_GLOB_DAT (value = S + A) ==\n");
	{
		Elf32_Addr S = dso_lookup(&lib, "malloc");    /* 0x2000 */
		reloc_value_t rv = reloc_value(R_ARM_GLOB_DAT, 0, S, 0x10);
		CHECK(rv.ok && rv.value == 0x2000 + 0x10, "GLOB_DAT malloc + addend 0x10");
	}

	printf("== R_ARM_RELATIVE (value = base + A, no symbol) ==\n");
	{
		reloc_value_t rv = reloc_value(R_ARM_RELATIVE, /*base*/0x400000, /*S*/0, /*A*/0x40);
		CHECK(rv.ok && rv.value == 0x400000 + 0x40, "RELATIVE base+addend");
	}

	printf("== R_ARM_ABS32 (value = S + A) ==\n");
	{
		Elf32_Addr S = dso_lookup(&lib, "write");     /* 0x3000 */
		reloc_value_t rv = reloc_value(R_ARM_ABS32, 0, S, 0x4);
		CHECK(rv.ok && rv.value == 0x3000 + 0x4, "ABS32 write + addend 0x4");
	}

	printf("== unresolved / unsupported -> ok=0 (caller must error) ==\n");
	{
		reloc_value_t u1 = reloc_value(R_ARM_JUMP_SLOT, 0, /*S=*/0, 0);
		CHECK(!u1.ok, "JUMP_SLOT with unresolved symbol (S==0) reported as failure");
		reloc_value_t u2 = reloc_value(0xff /*bogus*/, 0, 0x1000, 0);
		CHECK(!u2.ok, "unsupported reloc type reported as failure");
	}

	printf("\n%s (%d failure%s)\n", failures ? "TESTS FAILED" : "ALL TESTS PASSED",
	       failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
