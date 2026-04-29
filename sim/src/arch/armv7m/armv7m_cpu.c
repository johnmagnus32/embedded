/*
 * armv7m_cpu.c — Cortex-M4 Thumb instruction emulator
 *
 * Decodes and executes Thumb (16-bit) and Thumb-2 (32-bit) instructions.
 * Not cycle-accurate — just functionally correct enough to run our RTOS.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "armv7m_cpu.h"
#include "membus.h"
#include "elf_sym.h"

/* Flags in xPSR */
#define FLAG_N (1u << 31)
#define FLAG_Z (1u << 30)
#define FLAG_C (1u << 29)
#define FLAG_V (1u << 28)
#define FLAG_T (1u << 24)  /* Thumb bit — always 1 on Cortex-M */

static void set_nz(struct armv7m_cpu *c, uint32_t result)
{
    c->xpsr &= ~(FLAG_N | FLAG_Z);
    if (result == 0) c->xpsr |= FLAG_Z;
    if (result & 0x80000000) c->xpsr |= FLAG_N;
}

static void set_nzcv_add(struct armv7m_cpu *c, uint32_t a, uint32_t b, uint64_t result)
{
    uint32_t r = (uint32_t)result;
    set_nz(c, r);
    if (result > 0xFFFFFFFF) c->xpsr |= FLAG_C; else c->xpsr &= ~FLAG_C;
    if (((a ^ ~b) & (a ^ r)) & 0x80000000) c->xpsr |= FLAG_V; else c->xpsr &= ~FLAG_V;
}

static void set_nzcv_sub(struct armv7m_cpu *c, uint32_t a, uint32_t b, uint64_t result)
{
    uint32_t r = (uint32_t)result;
    set_nz(c, r);
    if (a >= b) c->xpsr |= FLAG_C; else c->xpsr &= ~FLAG_C;
    if (((a ^ b) & (a ^ r)) & 0x80000000) c->xpsr |= FLAG_V; else c->xpsr &= ~FLAG_V;
}

static int cond_check(struct armv7m_cpu *c, int cond)
{
    int n = (c->xpsr >> 31) & 1;
    int z = (c->xpsr >> 30) & 1;
    int cv = (c->xpsr >> 29) & 1;
    int v = (c->xpsr >> 28) & 1;
    switch (cond) {
    case 0:  return z;           /* EQ */
    case 1:  return !z;          /* NE */
    case 2:  return cv;          /* CS/HS */
    case 3:  return !cv;         /* CC/LO */
    case 4:  return n;           /* MI */
    case 5:  return !n;          /* PL */
    case 6:  return v;           /* VS */
    case 7:  return !v;          /* VC */
    case 8:  return cv && !z;    /* HI */
    case 9:  return !cv || z;    /* LS */
    case 10: return n == v;      /* GE */
    case 11: return n != v;      /* LT */
    case 12: return !z && (n == v); /* GT */
    case 13: return z || (n != v);  /* LE */
    case 14: return 1;           /* AL */
    default: return 1;
    }
}

void armv7m_cpu_init(struct armv7m_cpu *c)
{
    memset(c, 0, sizeof(*c));
    c->xpsr = FLAG_T;  /* Thumb mode */
}

/* Visualization hooks */
/* (vis removed — state visualization is in the web UI) */

void armv7m_cpu_reset(struct armv7m_cpu *c, struct membus *bus)
{
    c->msp = membus_read32(bus, FLASH_BASE + 0);
    c->r[REG_SP] = c->msp;
    c->r[REG_PC] = membus_read32(bus, FLASH_BASE + 4) & ~1u;
    c->xpsr = FLAG_T;
}

/* Forward declarations */
static int exec_thumb32(struct armv7m_cpu *c, struct membus *bus, uint32_t insn);
static void exc_return(struct armv7m_cpu *c, struct membus *bus, uint32_t exc_ret);

int armv7m_cpu_step(struct armv7m_cpu *c, struct membus *bus)
{


    uint32_t pc = c->r[REG_PC];
    uint16_t insn = membus_read16(bus, pc);
    c->r[REG_PC] = pc + 2;
    c->cycle_count++;
    c->irq_shadow = 0;

    /* IT block: check condition for current instruction */
    int it_skip = 0;
    if (c->it_state) {
        /* Top 4 bits = condition for this instruction */
        int cond = (c->it_state >> 4) & 0xF;
        it_skip = !cond_check(c, cond);
        /* Advance: shift mask left, keep base cond bits but update LSB */
        uint8_t mask = c->it_state & 0xF;
        if ((mask & 0x7) == 0)
            c->it_state = 0;  /* was last instruction */
        else
            c->it_state = ((c->it_state & 0xE0) | ((mask << 1) & 0x1F));
    }

    /* Check if this is a 32-bit Thumb-2 instruction */
    if ((insn & 0xE000) == 0xE000 && (insn & 0x1800) != 0) {
        uint16_t insn2 = membus_read16(bus, pc + 2);
        c->r[REG_PC] = pc + 4;
        if (it_skip) return 0;
        uint32_t insn32 = ((uint32_t)insn << 16) | insn2;
        return exec_thumb32(c, bus, insn32);
    }

    if (it_skip) return 0;

    /* ---- 16-bit Thumb instructions ---- */

    /* LSL/LSR/ASR immediate (shift) */
    if ((insn >> 13) == 0 && ((insn >> 11) & 3) < 3) {
        int op = (insn >> 11) & 3;
        int imm5 = (insn >> 6) & 0x1F;
        int rm = (insn >> 3) & 7;
        int rd = insn & 7;
        uint32_t val = c->r[rm];
        if (op == 0) { /* LSL */
            c->r[rd] = (imm5 == 0) ? val : val << imm5;
        } else if (op == 1) { /* LSR */
            c->r[rd] = (imm5 == 0) ? 0 : val >> imm5;
        } else { /* ASR */
            c->r[rd] = (imm5 == 0) ? ((int32_t)val >> 31) : (uint32_t)((int32_t)val >> imm5);
        }
        set_nz(c, c->r[rd]);
        return 0;
    }

    /* ADD/SUB register/immediate (3-bit) */
    if ((insn >> 11) == 0x3) {
        int rd = insn & 7;
        int rn = (insn >> 3) & 7;
        int is_sub = (insn >> 9) & 1;
        int is_imm = (insn >> 10) & 1;
        uint32_t operand = is_imm ? ((insn >> 6) & 7) : c->r[(insn >> 6) & 7];
        uint64_t result;
        if (is_sub) {
            result = (uint64_t)c->r[rn] - operand;
            set_nzcv_sub(c, c->r[rn], operand, result);
        } else {
            result = (uint64_t)c->r[rn] + operand;
            set_nzcv_add(c, c->r[rn], operand, result);
        }
        c->r[rd] = (uint32_t)result;
        return 0;
    }

    /* MOV/CMP/ADD/SUB immediate (8-bit) */
    if ((insn >> 13) == 1) {
        int op = (insn >> 11) & 3;
        int rd = (insn >> 8) & 7;
        uint32_t imm8 = insn & 0xFF;
        switch (op) {
        case 0: /* MOV */
            c->r[rd] = imm8;
            set_nz(c, imm8);
            break;
        case 1: { /* CMP */
            uint64_t res = (uint64_t)c->r[rd] - imm8;
            set_nzcv_sub(c, c->r[rd], imm8, res);
            break;
        }
        case 2: { /* ADD */
            uint64_t res = (uint64_t)c->r[rd] + imm8;
            set_nzcv_add(c, c->r[rd], imm8, res);
            c->r[rd] = (uint32_t)res;
            break;
        }
        case 3: { /* SUB */
            uint64_t res = (uint64_t)c->r[rd] - imm8;
            set_nzcv_sub(c, c->r[rd], imm8, res);
            c->r[rd] = (uint32_t)res;
            break;
        }
        }
        return 0;
    }

    /* ADD/SUB 3-register or 3-bit immediate */
    if ((insn >> 11) == 3) {
        int i = (insn >> 10) & 1;
        int op = (insn >> 9) & 1;
        int rm_imm = (insn >> 6) & 7;
        int rn = (insn >> 3) & 7;
        int rd = insn & 7;
        uint32_t operand = i ? rm_imm : c->r[rm_imm];
        if (op == 0) { /* ADD */
            uint64_t res = (uint64_t)c->r[rn] + operand;
            set_nzcv_add(c, c->r[rn], operand, res);
            c->r[rd] = (uint32_t)res;
        } else { /* SUB */
            uint64_t res = (uint64_t)c->r[rn] - operand;
            set_nzcv_sub(c, c->r[rn], operand, res);
            c->r[rd] = (uint32_t)res;
        }
        return 0;
    }

    /* Data processing (ALU operations between low registers) */
    if ((insn >> 10) == 0x10) {
        int op = (insn >> 6) & 0xF;
        int rm = (insn >> 3) & 7;
        int rd = insn & 7;
        uint32_t a = c->r[rd], b = c->r[rm];
        switch (op) {
        case 0x0: c->r[rd] = a & b; set_nz(c, c->r[rd]); break; /* AND */
        case 0x1: c->r[rd] = a ^ b; set_nz(c, c->r[rd]); break; /* EOR */
        case 0x2: c->r[rd] = a << (b & 31); set_nz(c, c->r[rd]); break; /* LSL */
        case 0x3: c->r[rd] = a >> (b & 31); set_nz(c, c->r[rd]); break; /* LSR */
        case 0x4: c->r[rd] = (uint32_t)((int32_t)a >> (b & 31)); set_nz(c, c->r[rd]); break; /* ASR */
        case 0x7: c->r[rd] = a & ~b; set_nz(c, c->r[rd]); break; /* BIC */
        case 0x8: { uint64_t r = (uint64_t)a - b; set_nzcv_sub(c, a, b, r); break; } /* TST→CMP? actually TST */
        case 0xA: { uint64_t r = (uint64_t)a - b; set_nzcv_sub(c, a, b, r); break; } /* CMP */
        case 0xB: { uint64_t r = (uint64_t)a + b; set_nzcv_add(c, a, b, r); break; } /* CMN */
        case 0xC: c->r[rd] = a | b; set_nz(c, c->r[rd]); break; /* ORR */
        case 0xD: c->r[rd] = a * b; set_nz(c, c->r[rd]); break; /* MUL */
        case 0xF: c->r[rd] = ~b; set_nz(c, c->r[rd]); break; /* MVN */
        default: break;
        }
        return 0;
    }

    /* Special data / branch exchange (high registers) */
    if ((insn >> 10) == 0x11) {
        int op = (insn >> 8) & 3;
        int d = ((insn >> 7) & 1) << 3;
        int rm = (insn >> 3) & 0xF;
        int rd = (insn & 7) | d;
        switch (op) {
        case 0: /* ADD high */
            c->r[rd] += c->r[rm];
            if (rd == REG_PC) c->r[REG_PC] &= ~1u;
            break;
        case 1: { /* CMP high */
            uint64_t r = (uint64_t)c->r[rd] - c->r[rm];
            set_nzcv_sub(c, c->r[rd], c->r[rm], r);
            break;
        }
        case 2: /* MOV high */
            c->r[rd] = c->r[rm];
            if (rd == REG_PC) c->r[REG_PC] &= ~1u;
            break;
        case 3: /* BX / BLX */
            if (insn & 0x80) { /* BLX */
                c->r[REG_LR] = c->r[REG_PC] | 1;
            }
            /* Check for EXC_RETURN (magic values 0xFFFFFFF*) */
            if ((c->r[rm] & 0xFFFFFFF0) == 0xFFFFFFF0) {
                exc_return(c, bus, c->r[rm]);
            } else {
                c->r[REG_PC] = c->r[rm] & ~1u;
            }
            break;
        }
        return 0;
    }

    /* LDR (PC-relative) */
    if ((insn >> 11) == 0x9) {
        int rt = (insn >> 8) & 7;
        uint32_t imm8 = (insn & 0xFF) << 2;
        uint32_t addr = ((pc + 4) & ~3u) + imm8;
        c->r[rt] = membus_read32(bus, addr);
        return 0;
    }

    /* LDR/STR register offset */
    if ((insn >> 12) == 0x5) {
        int op = (insn >> 9) & 7;
        int rm = (insn >> 6) & 7;
        int rn = (insn >> 3) & 7;
        int rt = insn & 7;
        uint32_t addr = c->r[rn] + c->r[rm];
        switch (op) {
        case 0: membus_write32(bus, addr, c->r[rt]); break; /* STR */
        case 1: membus_write16(bus, addr, c->r[rt]); break; /* STRH */
        case 2: membus_write8(bus, addr, c->r[rt]); break;  /* STRB */
        case 3: c->r[rt] = (int8_t)membus_read8(bus, addr); break; /* LDRSB */
        case 4: c->r[rt] = membus_read32(bus, addr); break; /* LDR */
        case 5: c->r[rt] = membus_read16(bus, addr); break; /* LDRH */
        case 6: c->r[rt] = membus_read8(bus, addr); break;  /* LDRB */
        case 7: c->r[rt] = (int16_t)membus_read16(bus, addr); break; /* LDRSH */
        }
        return 0;
    }

    /* LDR/STR immediate offset (word) */
    if ((insn >> 12) == 6) {
        int is_load = (insn >> 11) & 1;
        int imm5 = ((insn >> 6) & 0x1F) << 2;
        int rn = (insn >> 3) & 7;
        int rt = insn & 7;
        uint32_t addr = c->r[rn] + imm5;
        if (is_load)
            c->r[rt] = membus_read32(bus, addr);
        else
            membus_write32(bus, addr, c->r[rt]);
        return 0;
    }

    /* LDRB/STRB immediate offset */
    if ((insn >> 12) == 7) {
        int is_load = (insn >> 11) & 1;
        int imm5 = (insn >> 6) & 0x1F;
        int rn = (insn >> 3) & 7;
        int rt = insn & 7;
        uint32_t addr = c->r[rn] + imm5;
        if (is_load)
            c->r[rt] = membus_read8(bus, addr);
        else
            membus_write8(bus, addr, c->r[rt]);
        return 0;
    }

    /* LDRH/STRH immediate offset */
    if ((insn >> 12) == 8) {
        int is_load = (insn >> 11) & 1;
        int imm5 = ((insn >> 6) & 0x1F) << 1;
        int rn = (insn >> 3) & 7;
        int rt = insn & 7;
        uint32_t addr = c->r[rn] + imm5;
        if (is_load)
            c->r[rt] = membus_read16(bus, addr);
        else
            membus_write16(bus, addr, c->r[rt]);
        return 0;
    }

    /* LDR/STR SP-relative */
    if ((insn >> 12) == 9) {
        int is_load = (insn >> 11) & 1;
        int rt = (insn >> 8) & 7;
        uint32_t imm8 = (insn & 0xFF) << 2;
        uint32_t addr = c->r[REG_SP] + imm8;
        if (is_load)
            c->r[rt] = membus_read32(bus, addr);
        else
            membus_write32(bus, addr, c->r[rt]);
        return 0;
    }

    /* ADD SP/PC (generate address) */
    if ((insn >> 12) == 0xA) {
        int sp = (insn >> 11) & 1;
        int rd = (insn >> 8) & 7;
        uint32_t imm8 = (insn & 0xFF) << 2;
        c->r[rd] = (sp ? c->r[REG_SP] : ((pc + 4) & ~3u)) + imm8;
        return 0;
    }

    /* Misc: ADD/SUB SP, PUSH, POP, CPSID/CPSIE, etc. */
    if ((insn >> 12) == 0xB) {
        /* ADD/SUB SP immediate */
        if ((insn & 0xFF00) == 0xB000) {
            int imm7 = (insn & 0x7F) << 2;
            if (insn & 0x80)
                c->r[REG_SP] -= imm7;
            else
                c->r[REG_SP] += imm7;
            return 0;
        }

        /* PUSH */
        if ((insn & 0xFE00) == 0xB400) {
            int regs = insn & 0xFF;
            int lr = (insn >> 8) & 1;
            if (lr) { c->r[REG_SP] -= 4; membus_write32(bus, c->r[REG_SP], c->r[REG_LR]); }
            for (int i = 7; i >= 0; i--)
                if (regs & (1 << i)) { c->r[REG_SP] -= 4; membus_write32(bus, c->r[REG_SP], c->r[i]); }
            return 0;
        }

        /* POP */
        if ((insn & 0xFE00) == 0xBC00) {
            int regs = insn & 0xFF;
            int pc_bit = (insn >> 8) & 1;
            for (int i = 0; i < 8; i++)
                if (regs & (1 << i)) { c->r[i] = membus_read32(bus, c->r[REG_SP]); c->r[REG_SP] += 4; }
            if (pc_bit) {
                uint32_t val = membus_read32(bus, c->r[REG_SP]);
                c->r[REG_SP] += 4;
                if ((val & 0xFFFFFFF0) == 0xFFFFFFF0) {
                    exc_return(c, bus, val);
                } else {
                    c->r[REG_PC] = val & ~1u;
                }
            }
            return 0;
        }

        /* CPSID i / CPSIE i */
        if ((insn & 0xFFEF) == 0xB672) { c->primask = 1; return 0; } /* CPSID */
        if ((insn & 0xFFEF) == 0xB662) { c->primask = 0; c->irq_shadow = 1; return 0; } /* CPSIE — shadow: don't fire IRQ until next insn */

        /* IT (If-Then) block */
        if ((insn & 0xFF00) == 0xBF00) {
            uint8_t mask = insn & 0xF;
            if (mask == 0) return 0; /* NOP/YIELD/WFI/WFE/SEV (mask=0) */
            uint8_t cond = (insn >> 4) & 0xF;
            c->it_state = (cond << 4) | mask;
            return 0;
        }

        /* CBZ / CBNZ (compare and branch if zero/nonzero) */
        if ((insn & 0xF500) == 0xB100) {
            int rn = insn & 7;
            int nz = (insn >> 11) & 1;  /* 0=CBZ, 1=CBNZ */
            int imm = ((insn >> 3) & 0x1F) << 1 | ((insn >> 9) & 1) << 6;
            if (nz ? (c->r[rn] != 0) : (c->r[rn] == 0))
                c->r[REG_PC] = pc + 4 + imm;
            return 0;
        }

        /* SXTB, SXTH, UXTB, UXTH */
        if ((insn & 0xFF00) == 0xB200) { int rm = (insn>>3)&7; int rd = insn&7; c->r[rd] = (int16_t)(c->r[rm] & 0xFFFF); return 0; } /* SXTH */
        if ((insn & 0xFF00) == 0xB240) { int rm = (insn>>3)&7; int rd = insn&7; c->r[rd] = (int8_t)(c->r[rm] & 0xFF); return 0; } /* SXTB */
        if ((insn & 0xFF00) == 0xB280) { int rm = (insn>>3)&7; int rd = insn&7; c->r[rd] = c->r[rm] & 0xFFFF; return 0; } /* UXTH */
        if ((insn & 0xFF00) == 0xB2C0) { int rm = (insn>>3)&7; int rd = insn&7; c->r[rd] = c->r[rm] & 0xFF; return 0; } /* UXTB */

        /* REV, REV16 */
        if ((insn & 0xFFC0) == 0xBA00) {
            int rm = (insn>>3)&7; int rd = insn&7;
            uint32_t v = c->r[rm];
            c->r[rd] = ((v>>24)&0xFF) | ((v>>8)&0xFF00) | ((v<<8)&0xFF0000) | ((v<<24)&0xFF000000);
            return 0;
        }
    }

    /* STM / LDM (store/load multiple) */
    if ((insn >> 12) == 0xC) {
        int is_load = (insn >> 11) & 1;
        int rn = (insn >> 8) & 7;
        int regs = insn & 0xFF;
        uint32_t addr = c->r[rn];
        for (int i = 0; i < 8; i++) {
            if (regs & (1 << i)) {
                if (is_load)
                    c->r[i] = membus_read32(bus, addr);
                else
                    membus_write32(bus, addr, c->r[i]);
                addr += 4;
            }
        }
        c->r[rn] = addr;  /* writeback */
        return 0;
    }

    /* Conditional branch */
    if ((insn >> 12) == 0xD) {
        int cond = (insn >> 8) & 0xF;
        if (cond == 0xE) return 0; /* UDF */
        if (cond == 0xF) { /* SVC */
            return 0;
        }
        if (cond_check(c, cond)) {
            int32_t offset = (int8_t)(insn & 0xFF);
            c->r[REG_PC] = pc + 4 + (offset << 1);
        }
        return 0;
    }

    /* Unconditional branch */
    if ((insn >> 11) == 0x1C) {
        int32_t offset = insn & 0x7FF;
        if (offset & 0x400) offset |= 0xFFFFF800; /* sign extend */
        c->r[REG_PC] = pc + 4 + (offset << 1);
        return 0;
    }

    uint32_t _off;
    const char *_fn = sym_lookup(pc, &_off);
    fprintf(stderr, "Unknown 16-bit insn: 0x%04X at 0x%08X (%s+0x%x) LR=0x%08X\n",
            insn, pc, _fn ? _fn : "???", _off, c->r[14]);
    exit(1);
    return -1;
}

/* ---- 32-bit Thumb-2 instructions ---- */

static int exec_thumb32(struct armv7m_cpu *c, struct membus *bus, uint32_t insn)
{
    uint16_t hi = insn >> 16;
    uint16_t lo = insn & 0xFFFF;
    uint32_t pc = c->r[REG_PC] - 4;  /* PC of this instruction */

    /* BL (branch with link) */
    if ((hi & 0xF800) == 0xF000 && (lo & 0xD000) == 0xD000) {
        int s = (hi >> 10) & 1;
        int j1 = (lo >> 13) & 1;
        int j2 = (lo >> 11) & 1;
        int i1 = ~(j1 ^ s) & 1;
        int i2 = ~(j2 ^ s) & 1;
        int32_t offset = (s << 24) | (i1 << 23) | (i2 << 22) |
                         ((hi & 0x3FF) << 12) | ((lo & 0x7FF) << 1);
        if (s) offset |= 0xFE000000; /* sign extend */
        c->r[REG_LR] = (c->r[REG_PC]) | 1;
        c->r[REG_PC] = (pc + 4 + offset) & ~1u;
        return 0;
    }

    /* B.W (wide unconditional branch, no link) */
    if ((hi & 0xF800) == 0xF000 && (lo & 0xD000) == 0x9000) {
        int s = (hi >> 10) & 1;
        int j1 = (lo >> 13) & 1;
        int j2 = (lo >> 11) & 1;
        int i1 = ~(j1 ^ s) & 1;
        int i2 = ~(j2 ^ s) & 1;
        int32_t offset = (s << 24) | (i1 << 23) | (i2 << 22) |
                         ((hi & 0x3FF) << 12) | ((lo & 0x7FF) << 1);
        if (s) offset |= 0xFE000000;
        c->r[REG_PC] = (pc + 4 + offset) & ~1u;
        return 0;
    }

    /* LDR/STR immediate (Thumb-2 wide encoding) */
    if ((hi & 0xFFF0) == 0xF8D0) { /* LDR.W Rt, [Rn, #imm12] */
        int rn = hi & 0xF;
        int rt = (lo >> 12) & 0xF;
        uint32_t imm12 = lo & 0xFFF;
        c->r[rt] = membus_read32(bus, c->r[rn] + imm12);
        if (rt == REG_PC) c->r[REG_PC] &= ~1u;
        return 0;
    }
    if ((hi & 0xFFF0) == 0xF8C0) { /* STR.W Rt, [Rn, #imm12] */
        int rn = hi & 0xF;
        int rt = (lo >> 12) & 0xF;
        uint32_t imm12 = lo & 0xFFF;
        membus_write32(bus, c->r[rn] + imm12, c->r[rt]);
        return 0;
    }

    /* LDR.W Rt, [Rn, #-imm8] and pre/post indexed */
    if ((hi & 0xFFF0) == 0xF850) {
        int rn = hi & 0xF;
        int rt = (lo >> 12) & 0xF;
        if (lo & 0x0800) { /* positive offset or indexed */
            int p = (lo >> 10) & 1;
            int u = (lo >> 9) & 1;
            int w = (lo >> 8) & 1;
            uint32_t imm8 = lo & 0xFF;
            uint32_t addr = c->r[rn];
            uint32_t offset = u ? imm8 : -imm8;
            if (p) addr += offset;
            c->r[rt] = membus_read32(bus, addr);
            if (!p) addr += offset;
            if (w || !p) c->r[rn] = addr;
        } else { /* register offset */
            int rm = lo & 0xF;
            int shift = (lo >> 4) & 3;
            c->r[rt] = membus_read32(bus, c->r[rn] + (c->r[rm] << shift));
        }
        if (rt == REG_PC) c->r[REG_PC] &= ~1u;
        return 0;
    }

    /* STR.W with pre/post index */
    if ((hi & 0xFFF0) == 0xF840) {
        int rn = hi & 0xF;
        int rt = (lo >> 12) & 0xF;
        if (lo & 0x0800) { /* pre/post indexed */
            int p = (lo >> 10) & 1;
            int u = (lo >> 9) & 1;
            int w = (lo >> 8) & 1;
            uint32_t imm8 = lo & 0xFF;
            uint32_t addr = c->r[rn];
            uint32_t offset = u ? imm8 : -imm8;
            if (p) addr += offset;
            membus_write32(bus, addr, c->r[rt]);
            if (!p) addr += offset;
            if (w || !p) c->r[rn] = addr;
        } else { /* register offset */
            int rm = lo & 0xF;
            int shift = (lo >> 4) & 3;
            membus_write32(bus, c->r[rn] + (c->r[rm] << shift), c->r[rt]);
        }
        return 0;
    }

    /* LDRB.W / STRB.W */
    if ((hi & 0xFFF0) == 0xF890) { /* LDRB.W Rt, [Rn, #imm12] */
        int rn = hi & 0xF; int rt = (lo >> 12) & 0xF;
        c->r[rt] = membus_read8(bus, c->r[rn] + (lo & 0xFFF));
        return 0;
    }
    if ((hi & 0xFFF0) == 0xF880) { /* STRB.W */
        int rn = hi & 0xF; int rt = (lo >> 12) & 0xF;
        membus_write8(bus, c->r[rn] + (lo & 0xFFF), c->r[rt]);
        return 0;
    }

    /* LDRB.W with pre/post index */
    if ((hi & 0xFFF0) == 0xF810) {
        int rn = hi & 0xF; int rt = (lo >> 12) & 0xF;
        if (lo & 0x0800) { /* pre/post indexed */
            int p = (lo >> 10) & 1; int u = (lo >> 9) & 1; int w = (lo >> 8) & 1;
            uint32_t imm8 = lo & 0xFF;
            uint32_t addr = c->r[rn];
            uint32_t offset = u ? imm8 : -imm8;
            if (p) addr += offset;
            c->r[rt] = membus_read8(bus, addr);
            if (!p) addr += offset;
            if (w || !p) c->r[rn] = addr;
        } else { /* register offset */
            int rm = lo & 0xF;
            int shift = (lo >> 4) & 3;
            c->r[rt] = membus_read8(bus, c->r[rn] + (c->r[rm] << shift));
        }
        return 0;
    }

    /* STRB.W with pre/post index or register offset */
    if ((hi & 0xFFF0) == 0xF800) {
        int rn = hi & 0xF; int rt = (lo >> 12) & 0xF;
        if (lo & 0x0800) {
            int p = (lo >> 10) & 1; int u = (lo >> 9) & 1; int w = (lo >> 8) & 1;
            uint32_t imm8 = lo & 0xFF;
            uint32_t addr = c->r[rn];
            uint32_t offset = u ? imm8 : -imm8;
            if (p) addr += offset;
            membus_write8(bus, addr, c->r[rt]);
            if (!p) addr += offset;
            if (w || !p) c->r[rn] = addr;
        } else {
            int rm = lo & 0xF;
            int shift = (lo >> 4) & 3;
            membus_write8(bus, c->r[rn] + (c->r[rm] << shift), c->r[rt]);
        }
        return 0;
    }

    /* LDRH.W — 12-bit immediate offset */
    if ((hi & 0xFFF0) == 0xF8B0) {
        int rn = hi & 0xF; int rt = (lo >> 12) & 0xF;
        uint32_t imm12 = lo & 0xFFF;
        c->r[rt] = membus_read16(bus, c->r[rn] + imm12);
        return 0;
    }

    /* LDRH.W — pre/post index or register offset */
    if ((hi & 0xFFF0) == 0xF830) {
        int rn = hi & 0xF; int rt = (lo >> 12) & 0xF;
        if (lo & 0x0800) {
            int p = (lo >> 10) & 1; int u = (lo >> 9) & 1; int w = (lo >> 8) & 1;
            uint32_t imm8 = lo & 0xFF;
            uint32_t addr = c->r[rn];
            uint32_t offset = u ? imm8 : -imm8;
            if (p) addr += offset;
            c->r[rt] = membus_read16(bus, addr);
            if (!p) addr += offset;
            if (w || !p) c->r[rn] = addr;
        } else {
            int rm = lo & 0xF; int shift = (lo >> 4) & 3;
            c->r[rt] = membus_read16(bus, c->r[rn] + (c->r[rm] << shift));
        }
        return 0;
    }

    /* STRH.W with pre/post index or register offset */
    if ((hi & 0xFFF0) == 0xF820) {
        int rn = hi & 0xF; int rt = (lo >> 12) & 0xF;
        if (lo & 0x0800) {
            int p = (lo >> 10) & 1; int u = (lo >> 9) & 1; int w = (lo >> 8) & 1;
            uint32_t imm8 = lo & 0xFF;
            uint32_t addr = c->r[rn];
            uint32_t offset = u ? imm8 : -imm8;
            if (p) addr += offset;
            membus_write16(bus, addr, c->r[rt]);
            if (!p) addr += offset;
            if (w || !p) c->r[rn] = addr;
        } else {
            int rm = lo & 0xF; int shift = (lo >> 4) & 3;
            membus_write16(bus, c->r[rn] + (c->r[rm] << shift), c->r[rt]);
        }
        return 0;
    }

    /* MOV.W / MOVW (16-bit immediate) */
    if ((hi & 0xFBF0) == 0xF240) {
        int rd = (lo >> 8) & 0xF;
        uint32_t imm = ((hi & 0xF) << 12) | ((hi & 0x0400) << 1) |
                        ((lo & 0x7000) >> 4) | (lo & 0xFF);
        c->r[rd] = imm;
        return 0;
    }

    /* MOVT (top halfword) */
    if ((hi & 0xFBF0) == 0xF2C0) {
        int rd = (lo >> 8) & 0xF;
        uint32_t imm = ((hi & 0xF) << 12) | ((hi & 0x0400) << 1) |
                        ((lo & 0x7000) >> 4) | (lo & 0xFF);
        c->r[rd] = (c->r[rd] & 0xFFFF) | (imm << 16);
        return 0;
    }

    /* ADD.W / SUB.W / AND.W / ORR.W / etc. with immediate */
    /* But NOT MSR (0xF38x) or MRS (0xF3Ex) or ISB/DSB (0xF3Bx) */
    if ((hi & 0xFA00) == 0xF000 && (hi & 0xFFF0) != 0xF380
        && (hi & 0xFFF0) != 0xF3E0 && (hi & 0xFFF0) != 0xF3B0) {
        int op = (hi >> 5) & 0xF;
        int rn = hi & 0xF;
        int rd = (lo >> 8) & 0xF;
        int s = (hi >> 4) & 1;
        /* Decode Thumb modified immediate (A5.3.2 in ARM ARM) */
        int i = (hi >> 10) & 1;
        int imm3 = (lo >> 12) & 7;
        int imm8 = lo & 0xFF;
        uint32_t imm12 = (i << 11) | (imm3 << 8) | imm8;
        uint32_t imm;
        if ((imm12 >> 8) == 0) {
            imm = imm8;
        } else if ((imm12 >> 8) == 1) {
            imm = (imm8 << 16) | imm8;
        } else if ((imm12 >> 8) == 2) {
            imm = (imm8 << 24) | (imm8 << 8);
        } else if ((imm12 >> 8) == 3) {
            imm = (imm8 << 24) | (imm8 << 16) | (imm8 << 8) | imm8;
        } else {
            /* ROR encoding: 1bcdefgh → rotate right */
            uint32_t unrot = 0x80 | (imm12 & 0x7F);
            int rot = (imm12 >> 7) & 0x1F;
            imm = (unrot >> rot) | (unrot << (32 - rot));
        }

        switch (op) {
        case 0x0: { /* AND / TST */
            uint32_t result = c->r[rn] & imm;
            if (s && rd == 15) { set_nz(c, result); } /* TST: flags only */
            else { c->r[rd] = result; if (s) set_nz(c, result); }
            break;
        }
        case 0x1: c->r[rd] = c->r[rn] & ~imm; if (s) set_nz(c, c->r[rd]); break; /* BIC */
        case 0x2: c->r[rd] = (rn == 15) ? imm : c->r[rn] | imm; break; /* ORR / MOV */
        case 0x3: c->r[rd] = (rn == 15) ? ~imm : c->r[rn] | ~imm; break; /* ORN / MVN */
        case 0x4: c->r[rd] = c->r[rn] ^ imm; break; /* EOR */
        case 0x8: { uint64_t r = (uint64_t)c->r[rn] + imm; if (s) set_nzcv_add(c, c->r[rn], imm, r); if (rd != 15) c->r[rd] = (uint32_t)r; break; } /* ADD / CMN */
        case 0xA: { uint64_t r = (uint64_t)c->r[rn] - imm; if (s) set_nzcv_sub(c, c->r[rn], imm, r); c->r[rd] = (uint32_t)r; break; } /* SUB */
        case 0xD: { uint64_t r = (uint64_t)c->r[rn] - imm; if (rd != 15) c->r[rd] = (uint32_t)r; if (s) set_nzcv_sub(c, c->r[rn], imm, r); break; } /* SUB / CMP */
        case 0xE: { uint64_t r = (uint64_t)imm - c->r[rn]; c->r[rd] = (uint32_t)r; if (s) set_nzcv_sub(c, imm, c->r[rn], r); break; } /* RSB */
        default: c->r[rd] = imm; break;
        }
        if (s && op != 0x0 && op != 0x1 && op != 0x8 && op != 0xA && op != 0xD && op != 0xE) set_nz(c, c->r[rd]);
        return 0;
    }

    /* STMDB (push multiple, decrement before) */
    if ((hi & 0xFFD0) == 0xE900) {
        int rn = hi & 0xF;
        int regs = lo;
        int count = 0;
        for (int i = 0; i < 16; i++) if (regs & (1 << i)) count++;
        uint32_t addr = c->r[rn] - count * 4;
        if (hi & 0x0020) c->r[rn] = addr; /* writeback */
        for (int i = 0; i < 16; i++) {
            if (regs & (1 << i)) {
                membus_write32(bus, addr, c->r[i]);
                addr += 4;
            }
        }
        return 0;
    }

    /* STMIA.W (store multiple, increment after) */
    if ((hi & 0xFFD0) == 0xE880) {
        int rn = hi & 0xF;
        int regs = lo;
        uint32_t addr = c->r[rn];
        for (int i = 0; i < 16; i++) {
            if (regs & (1 << i)) {
                membus_write32(bus, addr, c->r[i]);
                addr += 4;
            }
        }
        if (hi & 0x0020) c->r[rn] = addr; /* writeback */
        return 0;
    }

    /* LDMIA.W (pop multiple, increment after) */
    if ((hi & 0xFFD0) == 0xE890) {
        int rn = hi & 0xF;
        int regs = lo;
        uint32_t addr = c->r[rn];
        for (int i = 0; i < 16; i++) {
            if (regs & (1 << i)) {
                c->r[i] = membus_read32(bus, addr);
                addr += 4;
            }
        }
        if (hi & 0x0020) c->r[rn] = addr; /* writeback */
        if (regs & (1 << REG_PC)) {
            if ((c->r[REG_PC] & 0xFFFFFFF0) == 0xFFFFFFF0)
                exc_return(c, bus, c->r[REG_PC]);
            else
                c->r[REG_PC] &= ~1u;
        }
        return 0;
    }

    /* MSR (move to special register) */
    if ((hi & 0xFFF0) == 0xF380) {
        int rn = hi & 0xF;
        int sysm = lo & 0xFF;
        switch (sysm) {
        case 8: c->msp = c->r[rn]; if (!(c->control & 2)) c->r[REG_SP] = c->msp; break;
        case 9: c->psp = c->r[rn]; if ((c->control & 2) && !c->in_handler) c->r[REG_SP] = c->psp; break;
                if ((c->control & 2) && !c->in_handler) c->r[REG_SP] = c->psp; break;
        case 16: c->primask = c->r[rn] & 1; break;
        case 20: c->control = c->r[rn] & 3;
                 c->r[REG_SP] = (c->control & 2) ? c->psp : c->msp; break;
        }
        return 0;
    }

    /* MRS (move from special register) */
    if ((hi & 0xFFF0) == 0xF3E0) {
        int rd = (lo >> 8) & 0xF;
        int sysm = lo & 0xFF;
        switch (sysm) {
        case 8: c->r[rd] = c->msp; break;
        case 9: c->r[rd] = c->psp; break;
        case 16: c->r[rd] = c->primask; break;
        case 20: c->r[rd] = c->control; break;
        }
        return 0;
    }

    /* ISB / DSB / DMB */
    if ((hi & 0xFFF0) == 0xF3B0) return 0;

    /* UBFX: unsigned bit field extract */
    if ((hi & 0xFFF0) == 0xF3C0) {
        int rn = hi & 0xF;
        int rd = (lo >> 8) & 0xF;
        int lsb = ((lo >> 12) & 7) << 2 | ((lo >> 6) & 3);
        int width = (lo & 0x1F) + 1;
        c->r[rd] = (c->r[rn] >> lsb) & ((1u << width) - 1);
        return 0;
    }

    /* SBFX: signed bit field extract */
    if ((hi & 0xFFF0) == 0xF340) {
        int rn = hi & 0xF;
        int rd = (lo >> 8) & 0xF;
        int lsb = ((lo >> 12) & 7) << 2 | ((lo >> 6) & 3);
        int width = (lo & 0x1F) + 1;
        uint32_t val = (c->r[rn] >> lsb) & ((1u << width) - 1);
        if (val & (1u << (width - 1))) val |= ~((1u << width) - 1); /* sign extend */
        c->r[rd] = val;
        return 0;
    }

    /* UDIV / SDIV */
    if ((hi & 0xFFF0) == 0xFBB0) { /* UDIV */
        int rn = hi & 0xF;
        int rd = (lo >> 8) & 0xF;
        int rm = lo & 0xF;
        c->r[rd] = (c->r[rm] == 0) ? 0 : c->r[rn] / c->r[rm];
        return 0;
    }
    if ((hi & 0xFFF0) == 0xFB90) { /* SDIV */
        int rn = hi & 0xF;
        int rd = (lo >> 8) & 0xF;
        int rm = lo & 0xF;
        c->r[rd] = (c->r[rm] == 0) ? 0 : (uint32_t)((int32_t)c->r[rn] / (int32_t)c->r[rm]);
        return 0;
    }

    /* MUL / MLA / MLS */
    if ((hi & 0xFFF0) == 0xFB00) {
        int rn = hi & 0xF;
        int ra = (lo >> 12) & 0xF;
        int rd = (lo >> 8) & 0xF;
        int rm = lo & 0xF;
        if (ra == 0xF)
            c->r[rd] = c->r[rn] * c->r[rm]; /* MUL */
        else if ((lo >> 4) & 1)
            c->r[rd] = c->r[ra] - c->r[rn] * c->r[rm]; /* MLS */
        else
            c->r[rd] = c->r[rn] * c->r[rm] + c->r[ra]; /* MLA */
        return 0;
    }
    /* SMULL / UMULL */
    if ((hi & 0xFFF0) == 0xFB80 || (hi & 0xFFF0) == 0xFBA0) {
        int rn = hi & 0xF;
        int rdlo = (lo >> 12) & 0xF;
        int rdhi = (lo >> 8) & 0xF;
        int rm = lo & 0xF;
        if ((hi & 0xFFF0) == 0xFB80) { /* SMULL */
            int64_t r = (int64_t)(int32_t)c->r[rn] * (int64_t)(int32_t)c->r[rm];
            c->r[rdlo] = (uint32_t)r;
            c->r[rdhi] = (uint32_t)(r >> 32);
        } else { /* UMULL */
            uint64_t r = (uint64_t)c->r[rn] * (uint64_t)c->r[rm];
            c->r[rdlo] = (uint32_t)r;
            c->r[rdhi] = (uint32_t)(r >> 32);
        }
        return 0;
    }

    /* LDRD / STRD (double word load/store) */
    if ((hi & 0xFE50) == 0xE850 || (hi & 0xFE50) == 0xE840) {
        int is_load = (hi >> 4) & 1;
        int rn = hi & 0xF;
        int rt = (lo >> 12) & 0xF;
        int rt2 = (lo >> 8) & 0xF;
        int p = (hi >> 8) & 1;
        int u = (hi >> 7) & 1;
        int w = (hi >> 5) & 1;
        uint32_t imm8 = (lo & 0xFF) << 2;
        uint32_t addr = c->r[rn];
        uint32_t offset = u ? imm8 : -imm8;
        if (p) addr += offset;
        if (is_load) {
            c->r[rt] = membus_read32(bus, addr);
            c->r[rt2] = membus_read32(bus, addr + 4);
        } else {
            membus_write32(bus, addr, c->r[rt]);
            membus_write32(bus, addr + 4, c->r[rt2]);
        }
        if (!p) addr += offset;
        if (w) c->r[rn] = p ? c->r[rn] + offset : addr;
        return 0;
    }

    /* Register-shifted register: LSL.W, LSR.W, ASR.W, ROR.W */
    if ((hi & 0xFF80) == 0xFA00) {
        int op = (hi >> 5) & 3;
        int rn = hi & 0xF;
        int rd = (lo >> 8) & 0xF;
        int rm = lo & 0xF;
        uint32_t shift = c->r[rm] & 0xFF;
        uint32_t val = c->r[rn];
        switch (op) {
        case 0: c->r[rd] = (shift >= 32) ? 0 : val << shift; break; /* LSL */
        case 1: c->r[rd] = (shift >= 32) ? 0 : val >> shift; break; /* LSR */
        case 2: c->r[rd] = (shift >= 32) ? ((int32_t)val >> 31) : (uint32_t)((int32_t)val >> shift); break; /* ASR */
        case 3: c->r[rd] = (shift == 0) ? val : (val >> shift) | (val << (32 - shift)); break; /* ROR */
        }
        set_nz(c, c->r[rd]);
        return 0;
    }

    /* Data processing (shifted register): MOV.W, ORR.W, AND.W, etc. */
    if ((hi & 0xFE00) == 0xEA00) {
        int op = (hi >> 5) & 0xF;
        int rn = hi & 0xF;
        int rd = (lo >> 8) & 0xF;
        int rm = lo & 0xF;
        int s = (hi >> 4) & 1;
        int shift_type = (lo >> 4) & 3;
        int imm5 = ((lo >> 12) & 7) << 2 | ((lo >> 6) & 3);
        uint32_t val = c->r[rm];

        /* Apply shift */
        switch (shift_type) {
        case 0: val = (imm5 == 0) ? val : val << imm5; break; /* LSL */
        case 1: val = (imm5 == 0) ? 0 : val >> imm5; break;   /* LSR */
        case 2: val = (imm5 == 0) ? ((int32_t)val >> 31) : (uint32_t)((int32_t)val >> imm5); break; /* ASR */
        case 3: /* ROR */ if (imm5) val = (val >> imm5) | (val << (32 - imm5)); break;
        }

        switch (op) {
        case 0x0: c->r[rd] = c->r[rn] & val; break;  /* AND */
        case 0x1: c->r[rd] = c->r[rn] & ~val; break;  /* BIC */
        case 0x2: c->r[rd] = (rn == 15) ? val : c->r[rn] | val; break; /* ORR / MOV */
        case 0x3: c->r[rd] = (rn == 15) ? ~val : c->r[rn] | ~val; break; /* ORN / MVN */
        case 0x4: c->r[rd] = c->r[rn] ^ val; break;  /* EOR */
        case 0x8: { uint64_t r = (uint64_t)c->r[rn] + val; c->r[rd] = (uint32_t)r; if (s) set_nzcv_add(c, c->r[rn], val, r); break; } /* ADD */
        case 0xA: { uint64_t r = (uint64_t)c->r[rn] - val; c->r[rd] = (uint32_t)r; if (s) set_nzcv_sub(c, c->r[rn], val, r);
            if (pc == 0x0800022E) { /* sub.w in print_int */
                static FILE *sf = NULL;
                if (!sf) sf = fopen("/tmp/sub_debug.txt", "w");
                if (sf) { fprintf(sf, "SUB.W r%d = r%d(%u) - r%d_shifted(%u) = %u\n", rd, rn, c->r[rn]+(uint32_t)val, rm, (uint32_t)val, c->r[rd]); fflush(sf); }
            }
            break; } /* SUB */
        case 0xD: { uint64_t r = (uint64_t)c->r[rn] - val; if (rd != 15) c->r[rd] = (uint32_t)r; if (s) set_nzcv_sub(c, c->r[rn], val, r); break; } /* SUB / CMP */
        case 0xE: { uint64_t r = (uint64_t)val - c->r[rn]; c->r[rd] = (uint32_t)r; if (s) set_nzcv_sub(c, val, c->r[rn], r); break; } /* RSB */
        default: c->r[rd] = val; break;
        }
        if (s && op != 0x8 && op != 0xA && op != 0xD) set_nz(c, c->r[rd]);
        return 0;
    }

    /* Conditional branch (wide) — cond must be 0-13, not 14/15 */
    if ((hi & 0xF800) == 0xF000 && (lo & 0xD000) == 0x8000) {
        int cond = (hi >> 6) & 0xF;
        if (cond >= 14) goto not_cond_branch;  /* cond 14/15 = other encodings */
        if (cond_check(c, cond)) {
            int s = (hi >> 10) & 1;
            int j1 = (lo >> 13) & 1;
            int j2 = (lo >> 11) & 1;
            int32_t offset = (s << 20) | (j2 << 19) | (j1 << 18) |
                             ((hi & 0x3F) << 12) | ((lo & 0x7FF) << 1);
            if (s) offset |= 0xFFE00000;
            c->r[REG_PC] = pc + 4 + offset;
        }
        return 0;
    }
    not_cond_branch: ;

    uint32_t _off;
    const char *_fn = sym_lookup(pc, &_off);
    fprintf(stderr, "Unknown 32-bit insn: 0x%08X at 0x%08X (%s+0x%x) LR=0x%08X\n",
            insn, pc, _fn ? _fn : "???", _off, c->r[14]);
    exit(1);
    return -1;
}

/* ---- Interrupt entry/exit ---- */

void armv7m_take_interrupt(struct armv7m_cpu *c, struct membus *bus, int vector_num)
{
    /* Sync PSP/MSP from the SP register (instructions like PUSH update SP but not psp/msp) */
    if (c->control & 2)
        c->psp = c->r[REG_SP];
    else
        c->msp = c->r[REG_SP];

    /* Push exception frame to current stack (PSP or MSP) */
    uint32_t *sp_ptr;
    if (c->control & 2)
        sp_ptr = &c->psp;
    else
        sp_ptr = &c->msp;

    uint32_t sp = *sp_ptr - 32;
    membus_write32(bus, sp + 0,  c->r[0]);
    membus_write32(bus, sp + 4,  c->r[1]);
    membus_write32(bus, sp + 8,  c->r[2]);
    membus_write32(bus, sp + 12, c->r[3]);
    membus_write32(bus, sp + 16, c->r[12]);
    membus_write32(bus, sp + 20, c->r[REG_LR]);
    membus_write32(bus, sp + 24, c->r[REG_PC] | 1);
    /* Encode IT state into xPSR bits [26:25][15:10] before saving */
    uint32_t saved_xpsr = c->xpsr;
    if (c->it_state) {
        saved_xpsr |= ((c->it_state >> 6) & 3) << 25;  /* ICI/IT bits [26:25] */
        saved_xpsr |= (c->it_state & 0x3F) << 10;       /* ICI/IT bits [15:10] */
    }
    membus_write32(bus, sp + 28, saved_xpsr);
    c->it_state = 0;  /* Clear IT state for exception handler */
    *sp_ptr = sp;

    /* Set EXC_RETURN in LR */
    if (c->control & 2)
        c->r[REG_LR] = 0xFFFFFFFD;  /* return to thread mode, PSP */
    else
        c->r[REG_LR] = 0xFFFFFFF9;  /* return to thread mode, MSP */

    /* Switch to handler mode (MSP, privileged) */
    c->r[REG_SP] = c->msp;
    c->in_handler = 1;

    /* Jump to vector */
    uint32_t handler = membus_read32(bus, FLASH_BASE + vector_num * 4);
    c->r[REG_PC] = handler & ~1u;
}

static void exc_return(struct armv7m_cpu *c, struct membus *bus, uint32_t exc_ret)
{
    /* Pop exception frame */
    uint32_t *sp_ptr;
    if (exc_ret & 0x4)
        sp_ptr = &c->psp;  /* return to PSP */
    else
        sp_ptr = &c->msp;  /* return to MSP */

    uint32_t sp = *sp_ptr;
    c->r[0]      = membus_read32(bus, sp + 0);
    c->r[1]      = membus_read32(bus, sp + 4);
    c->r[2]      = membus_read32(bus, sp + 8);
    c->r[3]      = membus_read32(bus, sp + 12);
    c->r[12]     = membus_read32(bus, sp + 16);
    c->r[REG_LR] = membus_read32(bus, sp + 20);
    c->r[REG_PC] = membus_read32(bus, sp + 24) & ~1u;
    c->xpsr      = membus_read32(bus, sp + 28) | FLAG_T;
    /* Restore IT state from xPSR bits [26:25][15:10] */
    uint32_t restored_xpsr = c->xpsr;
    c->it_state = ((restored_xpsr >> 25) & 3) << 6 | ((restored_xpsr >> 10) & 0x3F);
    /* Clear IT bits from xPSR (they live in it_state now) */
    c->xpsr &= ~((3 << 25) | (0x3F << 10));
    *sp_ptr = sp + 32;

    /* Restore stack pointer */
    if (exc_ret & 0x4) {
        c->r[REG_SP] = c->psp;
        c->control |= 2;  /* SPSEL = PSP */
    } else {
        c->r[REG_SP] = c->msp;
    }

    c->in_handler = 0;

    /* Record which task/context we're returning to */


}

/* take_interrupt and exc_return are used by nvic.c via the header */
