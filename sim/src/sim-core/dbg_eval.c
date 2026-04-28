#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "cpu.h"
#include "elf_sym.h"
#include "dbg_eval.h"

#define LOG(fmt, ...) fprintf(stderr, "[sim-core] " fmt "\n", ##__VA_ARGS__)

int dbg_eval(const char *expr, struct cpu_state *cpu, uint8_t *flash,
             uint8_t *ram, char *buf, int bufsize)
{
    const char *p = expr;
    uint32_t addr = 0;
    uint32_t cur_type = 0;
    int valid = 0;
    int loc = 0;

    /* Leading * dereference */
    int leading_deref = 0;
    if (*p == '*') { leading_deref = 1; p++; }

    /* Parse base variable name */
    char base[64]; int bi = 0;
    while (*p && *p != '.' && *p != '-' && *p != '[' && bi < 63)
        base[bi++] = *p++;
    base[bi] = '\0';

    /* Resolve base variable */
    int reg; uint32_t val;
    loc = var_lookup(base, cpu->r[REG_PC], &reg, &val);
    cur_type = var_type_die(base, cpu->r[REG_PC]);
    LOG("expr base='%s' pc=0x%08X loc=%d type=0x%X", base, cpu->r[REG_PC], loc, cur_type);

    if (loc == 1) { /* register */
        uint32_t regval = cpu->r[reg];
        uint32_t scratch = RAM_BASE + RAM_SIZE - 8;
        *(uint32_t*)(ram + RAM_SIZE - 8) = regval;
        addr = scratch;
        valid = 1;
    } else if (loc == 2) { /* constant */
        addr = val;
        valid = 1;
    } else if (loc == 3) { /* stack (fbreg) */
        uint32_t cfa = cfa_offset_at_pc(cpu->r[REG_PC]);
        addr = cpu->r[REG_SP] + cfa + val;
        valid = 1;
    } else {
        /* Try global symbol */
        uint32_t sym = sym_find_by_name(base);
        if (sym) { addr = sym; valid = 1; }
        /* Try register name */
        static const char *rn[] = {"r0","r1","r2","r3","r4","r5","r6","r7",
                                   "r8","r9","r10","r11","r12","sp","lr","pc"};
        for (int r = 0; r < 16; r++)
            if (strcmp(base, rn[r]) == 0) { addr = cpu->r[r]; valid = 1; break; }
    }

    /* Apply leading dereference */
    if (leading_deref && valid && cur_type) {
        uint32_t pointee = type_deref(cur_type);
        if (pointee) {
            uint32_t ptr = addr;
            if (loc != 1 && loc != 2) {
                if (addr >= RAM_BASE && addr < RAM_BASE + RAM_SIZE)
                    ptr = *(uint32_t*)(ram + (addr - RAM_BASE));
                else if (addr >= FLASH_BASE && addr < FLASH_BASE + FLASH_SIZE)
                    ptr = *(uint32_t*)(flash + (addr - FLASH_BASE));
            }
            addr = ptr;
            cur_type = pointee;
        }
    }

    /* Apply chained operators: .member, ->member, [index] */
    while (*p && valid) {
        if (*p == '.' && *(p+1) != '\0') {
            p++;
            char mem[32]; int mi = 0;
            while (*p && *p != '.' && *p != '-' && *p != '[' && mi < 31)
                mem[mi++] = *p++;
            mem[mi] = '\0';
            uint32_t off, mtype;
            if (type_member(cur_type, mem, &off, &mtype)) {
                addr += off;
                cur_type = mtype;
            } else { valid = 0; }

        } else if (*p == '-' && *(p+1) == '>') {
            p += 2;
            uint32_t pointee = type_deref(cur_type);
            if (pointee) {
                uint32_t ptr = 0;
                if (addr >= RAM_BASE && addr < RAM_BASE + RAM_SIZE)
                    ptr = *(uint32_t*)(ram + (addr - RAM_BASE));
                else if (addr >= FLASH_BASE && addr < FLASH_BASE + FLASH_SIZE)
                    ptr = *(uint32_t*)(flash + (addr - FLASH_BASE));
                addr = ptr;
                cur_type = pointee;
            }
            char mem[32]; int mi = 0;
            while (*p && *p != '.' && *p != '-' && *p != '[' && mi < 31)
                mem[mi++] = *p++;
            mem[mi] = '\0';
            uint32_t off, mtype;
            if (type_member(cur_type, mem, &off, &mtype)) {
                addr += off;
                cur_type = mtype;
            } else { valid = 0; }

        } else if (*p == '[') {
            p++;
            char idx_expr[32]; int ii = 0;
            while (*p && *p != ']' && ii < 31) idx_expr[ii++] = *p++;
            idx_expr[ii] = '\0';
            if (*p == ']') p++;

            int idx = atoi(idx_expr);
            if (idx == 0 && idx_expr[0] != '0') {
                int vreg; uint32_t vval;
                int vloc = var_lookup(idx_expr, cpu->r[REG_PC], &vreg, &vval);
                if (vloc == 1) idx = (int)cpu->r[vreg];
                else if (vloc == 2) idx = (int)vval;
                else if (vloc == 3) {
                    uint32_t a = cpu->r[REG_SP] + vval;
                    if (a >= RAM_BASE && a < RAM_BASE + RAM_SIZE)
                        idx = (int)*(uint32_t*)(ram + (a - RAM_BASE));
                }
            }

            uint32_t elem_size;
            uint32_t elem_type = type_array_elem(cur_type, &elem_size);
            if (elem_type) {
                addr += idx * elem_size;
                cur_type = elem_type;
            } else { valid = 0; }
        } else {
            break;
        }
    }

    if (valid && cur_type) {
        char tbuf[3000];
        type_format(cur_type, addr, ram, flash, tbuf, sizeof(tbuf));
        snprintf(buf, bufsize, "{\"expr\":\"%s\",\"val\":\"%s\"}", expr, tbuf);
    } else if (valid) {
        uint32_t v = 0;
        if (addr >= RAM_BASE && addr < RAM_BASE + RAM_SIZE)
            v = *(uint32_t*)(ram + (addr - RAM_BASE));
        snprintf(buf, bufsize, "{\"expr\":\"%s\",\"val\":\"%u\",\"hex\":\"0x%08x\"}", expr, v, v);
    } else {
        snprintf(buf, bufsize, "{\"expr\":\"%s\",\"error\":\"not found\"}", expr);
    }
    return 0;
}
