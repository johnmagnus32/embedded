#ifndef ELF_SYM_H
#define ELF_SYM_H

#include <stdint.h>

/* Load ELF segments into flash/ram and parse symbol table + debug lines.
 * Returns 0 on success, -1 if not an ELF (caller should try raw .bin). */
int elf_load(const char *path, uint8_t *flash, uint8_t *ram);

/* Look up a PC address → function name + offset from start.
 * Returns NULL if no symbol found. */
const char *sym_lookup(uint32_t pc, uint32_t *offset);

/* Find a symbol's address by name. Returns 0 if not found. */
uint32_t sym_find_by_name(const char *name);

/* Look up a PC address → source file and line number.
 * Returns NULL if no debug info. */
const char *line_lookup(uint32_t pc, int *line_out);

/* Resolve a breakpoint spec ("func", "file:line", "0xADDR") to an address.
 * Returns 0 if not found. */
uint32_t resolve_breakpoint(const char *spec);
int var_lookup(const char *name, uint32_t pc, int *reg_out, uint32_t *val_out);
uint32_t var_type_die(const char *name, uint32_t pc);
int type_format(uint32_t type_die, uint32_t addr, uint8_t *ram, uint8_t *flash,
                char *buf, int bufsize);
uint32_t type_deref(uint32_t type_die);
uint32_t type_byte_size(uint32_t type_die);
uint32_t type_member(uint32_t struct_type_die, const char *member_name,
                     uint32_t *offset_out, uint32_t *member_type_out);
uint32_t type_array_elem(uint32_t array_type_die, uint32_t *elem_size_out);

/* ELF section info (allocated sections with addresses) */
struct elf_section { uint32_t addr; uint32_t size; char name[32]; };
int elf_get_sections(const struct elf_section **out);

/* Symbol info for globals */
struct sym_entry { uint32_t addr; uint32_t size; char name[64]; };
int sym_in_range(uint32_t lo, uint32_t hi, struct sym_entry *out, int max);

/* DWARF-derived TCB struct layout. Returns 0 in tcb_size if not found. */
void dwarf_get_tcb_layout(uint32_t *tcb_size, int *sp_off, int *name_off);

/* Stack variable info for annotation */
struct stack_var { char name[32]; int32_t sp_offset; uint32_t type_die; };
int vars_on_stack(uint32_t pc, struct stack_var *out, int max);
uint32_t cfa_offset_at_pc(uint32_t pc);

#endif
