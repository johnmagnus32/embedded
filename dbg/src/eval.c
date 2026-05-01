/*
 * eval.c — Expression evaluator
 *
 * Handles: variable, *deref, .member, ->field, [index], register names.
 * Reads registers/memory via the debug client (GDB RSP).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "eval.h"
#include "client.h"
#include "elf_sym.h"

/* --- RSP helpers (same as cmd.c) --- */

static uint32_t parse_le32(const char *hex)
{
    uint32_t v = 0;
    for (int b = 0; b < 4; b++) {
        unsigned int byte;
        sscanf(hex + b * 2, "%2x", &byte);
        v |= byte << (b * 8);
    }
    return v;
}

static int get_regs(struct dbg_client *c, uint32_t regs[16])
{
    dbg_send(c, "g");
    char *resp = dbg_recv(c);
    if (!resp || strlen(resp) < 16 * 8) return -1;
    for (int i = 0; i < 16; i++)
        regs[i] = parse_le32(resp + i * 8);
    return 0;
}

static int read_mem(struct dbg_client *c, uint32_t addr, uint8_t *out, int len)
{
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "m%x,%x", addr, len);
    dbg_send(c, cmd);
    char *resp = dbg_recv(c);
    if (!resp) return -1;
    for (int i = 0; i < len && resp[i*2] && resp[i*2+1]; i++) {
        unsigned int b;
        sscanf(resp + i * 2, "%2x", &b);
        out[i] = b;
    }
    return 0;
}

static uint32_t read_mem32(struct dbg_client *c, uint32_t addr)
{
    uint8_t buf[4];
    if (read_mem(c, addr, buf, 4) < 0) return 0;
    return buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
}

/* --- Evaluator --- */

void eval_expr_with_regs(struct dbg_client *c, const char *expr,
                         uint32_t regs[16], struct eval_result *out)
{
    memset(out, 0, sizeof(*out));
    uint32_t pc = regs[15];

    const char *p = expr;
    uint32_t addr = 0;
    uint32_t cur_type = 0;
    int valid = 0;

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
    int loc = var_lookup(base, pc, &reg, &val);
    cur_type = var_type_die(base, pc);

    if (loc == 1) { /* register */
        addr = regs[reg];
        valid = 1;
        if (cur_type && !*p && !leading_deref) {
            out->valid = 1;
            out->val = regs[reg];
            out->addr = 0;
            out->type_die = cur_type;
            out->is_register = 1;
            return;
        }
    } else if (loc == 2) { /* constant / stack_value */
        if (!*p && !leading_deref) {
            /* Simple constant — value is the variable's value, not an address */
            out->valid = 1;
            out->val = val;
            out->type_die = cur_type;
            out->is_register = 1;  /* treat like register — val is the value itself */
            return;
        }
        addr = val;
        valid = 1;
    } else if (loc == 3) { /* stack (fbreg) */
        uint32_t cfa = cfa_offset_at_pc(pc);
        addr = regs[13] + cfa + val;
        valid = 1;
    } else {
        /* Try global symbol */
        uint32_t sym = sym_find_by_name(base);
        if (sym) { addr = sym; valid = 1; }
        /* Try register name */
        static const char *rn[] = {"r0","r1","r2","r3","r4","r5","r6","r7",
                                   "r8","r9","r10","r11","r12","sp","lr","pc"};
        for (int r = 0; r < 16; r++)
            if (strcmp(base, rn[r]) == 0) {
                out->valid = 1;
                out->val = regs[r];
                out->is_register = 1;
                return;
            }
    }

    /* Apply leading dereference */
    if (leading_deref && valid && cur_type) {
        uint32_t pointee = type_deref(cur_type);
        if (pointee) {
            addr = read_mem32(c, addr);
            cur_type = pointee;
        }
    }

    /* Apply chained operators: .member, ->member, [index] */
    while (*p && valid) {
        if (*p == '.' && *(p+1)) {
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
                addr = read_mem32(c, addr);
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
            char idx_str[32]; int ii = 0;
            while (*p && *p != ']' && ii < 31) idx_str[ii++] = *p++;
            idx_str[ii] = '\0';
            if (*p == ']') p++;
            int idx = atoi(idx_str);
            uint32_t elem_size;
            uint32_t elem_type = type_array_elem(cur_type, &elem_size);
            if (elem_type) {
                addr += idx * elem_size;
                cur_type = elem_type;
            } else { valid = 0; }
        } else break;
    }

    if (valid) {
        out->valid = 1;
        out->addr = addr;
        out->type_die = cur_type;
        uint32_t bsz = type_byte_size(cur_type);
        if (bsz <= 4) {
            out->val = read_mem32(c, addr);
            if (bsz == 1) out->val &= 0xFF;
            else if (bsz == 2) out->val &= 0xFFFF;
        }
    }
}

int eval_format(struct dbg_client *c, struct eval_result *r, const char *expr,
                char *buf, int bufsize)
{
    if (!r->valid)
        return snprintf(buf, bufsize, "<unknown>");
    if (r->is_register)
        return snprintf(buf, bufsize, "%u (0x%x)", r->val, r->val);
    uint32_t bsz = r->type_die ? type_byte_size(r->type_die) : 4;
    if (bsz <= 4)
        return snprintf(buf, bufsize, "%u (0x%x)", r->val, r->val);
    return snprintf(buf, bufsize, "@ 0x%08x (%u bytes)", r->addr, bsz);
}

void eval_expr(struct dbg_client *c, const char *expr, struct eval_result *out)
{
    uint32_t regs[16];
    if (get_regs(c, regs) < 0) { memset(out, 0, sizeof(*out)); return; }
    eval_expr_with_regs(c, expr, regs, out);
}
