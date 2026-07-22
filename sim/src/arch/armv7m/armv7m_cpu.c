/*
 * armv7m_cpu.c — Cortex-M4 Thumb instruction emulator
 *
 * Decodes and executes Thumb (16-bit) and Thumb-2 (32-bit) instructions.
 * Not cycle-accurate — just functionally correct enough to run our RTOS.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "armv7m_cpu.h"
#include "membus.h"

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
    int in_it = 0;
    if (c->it_state) {
        in_it = 1;
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

    /* In IT block: 16-bit instructions must NOT update flags. */
    uint32_t saved_flags = c->xpsr & (FLAG_N | FLAG_Z | FLAG_C | FLAG_V);

    /* Computed goto dispatch table indexed by insn >> 11 (32 groups) */
    static void *dispatch[32] = {
        &&lsl_imm, &&lsr_imm, &&asr_imm, &&add_sub,
        &&mov_imm, &&cmp_imm, &&add_imm8, &&sub_imm8,
        &&alu_special, &&ldr_pc, &&ls_reg, &&ls_reg,
        &&str_imm, &&ldr_imm, &&strb_imm, &&ldrb_imm,
        &&strh_imm, &&ldrh_imm, &&str_sp, &&ldr_sp,
        &&adr, &&add_sp_imm, &&misc, &&misc,
        &&stm, &&ldm, &&cond_branch, &&cond_branch,
        &&branch, &&unknown16, &&unknown16, &&unknown16,
    };
    goto *dispatch[insn >> 11];

/* ---- Groups 0-2: Shift immediate ---- */
lsl_imm: {
    int imm5 = (insn >> 6) & 0x1F, rm = (insn >> 3) & 7, rd = insn & 7;
    c->r[rd] = (imm5 == 0) ? c->r[rm] : c->r[rm] << imm5;
    set_nz(c, c->r[rd]);
    goto done_16;
}
lsr_imm: {
    int imm5 = (insn >> 6) & 0x1F, rm = (insn >> 3) & 7, rd = insn & 7;
    c->r[rd] = (imm5 == 0) ? 0 : c->r[rm] >> imm5;
    set_nz(c, c->r[rd]);
    goto done_16;
}
asr_imm: {
    int imm5 = (insn >> 6) & 0x1F, rm = (insn >> 3) & 7, rd = insn & 7;
    c->r[rd] = (imm5 == 0) ? ((int32_t)c->r[rm] >> 31) : (uint32_t)((int32_t)c->r[rm] >> imm5);
    set_nz(c, c->r[rd]);
    goto done_16;
}

/* ---- Group 3: ADD/SUB register/imm3 ---- */
add_sub: {
    int rd = insn & 7, rn = (insn >> 3) & 7;
    int is_sub = (insn >> 9) & 1, is_imm = (insn >> 10) & 1;
    uint32_t operand = is_imm ? ((insn >> 6) & 7) : c->r[(insn >> 6) & 7];
    uint64_t result;
    if (is_sub) { result = (uint64_t)c->r[rn] - operand; set_nzcv_sub(c, c->r[rn], operand, result); }
    else { result = (uint64_t)c->r[rn] + operand; set_nzcv_add(c, c->r[rn], operand, result); }
    c->r[rd] = (uint32_t)result;
    goto done_16;
}

/* ---- Groups 4-7: MOV/CMP/ADD/SUB imm8 ---- */
mov_imm: {
    int rd = (insn >> 8) & 7; uint32_t imm8 = insn & 0xFF;
    c->r[rd] = imm8; set_nz(c, imm8);
    goto done_16;
}
cmp_imm: {
    int rd = (insn >> 8) & 7; uint32_t imm8 = insn & 0xFF;
    uint64_t res = (uint64_t)c->r[rd] - imm8; set_nzcv_sub(c, c->r[rd], imm8, res);
    goto done_16;
}
add_imm8: {
    int rd = (insn >> 8) & 7; uint32_t imm8 = insn & 0xFF;
    uint64_t res = (uint64_t)c->r[rd] + imm8; set_nzcv_add(c, c->r[rd], imm8, res);
    c->r[rd] = (uint32_t)res;
    goto done_16;
}
sub_imm8: {
    int rd = (insn >> 8) & 7; uint32_t imm8 = insn & 0xFF;
    uint64_t res = (uint64_t)c->r[rd] - imm8; set_nzcv_sub(c, c->r[rd], imm8, res);
    c->r[rd] = (uint32_t)res;
    goto done_16;
}

/* ---- Group 8: ALU ops + special data/branch ---- */
alu_special: {
    if ((insn >> 10) == 0x10) {
        /* Data processing (ALU) */
        int op = (insn >> 6) & 0xF, rm = (insn >> 3) & 7, rd = insn & 7;
        uint32_t a = c->r[rd], b = c->r[rm];
        switch (op) {
        case 0x0: c->r[rd] = a & b; set_nz(c, c->r[rd]); break;
        case 0x1: c->r[rd] = a ^ b; set_nz(c, c->r[rd]); break;
        case 0x2: c->r[rd] = a << (b & 31); set_nz(c, c->r[rd]); break;
        case 0x3: c->r[rd] = a >> (b & 31); set_nz(c, c->r[rd]); break;
        case 0x4: c->r[rd] = (uint32_t)((int32_t)a >> (b & 31)); set_nz(c, c->r[rd]); break;
        case 0x5: { uint64_t r = (uint64_t)a + b + ((c->xpsr >> 29) & 1); set_nzcv_add(c, a, b + ((c->xpsr >> 29) & 1), r); c->r[rd] = (uint32_t)r; break; }
        case 0x6: { uint64_t r = (uint64_t)a - b - (((c->xpsr >> 29) & 1) ^ 1); set_nzcv_sub(c, a, b, r); c->r[rd] = (uint32_t)r; break; }
        case 0x7: { uint32_t s = b & 31; c->r[rd] = s ? (a >> s) | (a << (32 - s)) : a; set_nz(c, c->r[rd]); break; }
        case 0x8: { uint32_t r = a & b; set_nz(c, r); break; }
        case 0x9: { uint64_t r = (uint64_t)0 - b; set_nzcv_sub(c, 0, b, r); c->r[rd] = (uint32_t)r; break; }
        case 0xA: { uint64_t r = (uint64_t)a - b; set_nzcv_sub(c, a, b, r); break; }
        case 0xB: { uint64_t r = (uint64_t)a + b; set_nzcv_add(c, a, b, r); break; }
        case 0xC: c->r[rd] = a | b; set_nz(c, c->r[rd]); break;
        case 0xD: c->r[rd] = a * b; set_nz(c, c->r[rd]); break;
        case 0xE: c->r[rd] = a & ~b; set_nz(c, c->r[rd]); break;
        case 0xF: c->r[rd] = ~b; set_nz(c, c->r[rd]); break;
        }
    } else {
        /* Special data / branch exchange */
        int op = (insn >> 8) & 3, d = ((insn >> 7) & 1) << 3;
        int rm = (insn >> 3) & 0xF, rd = (insn & 7) | d;
        switch (op) {
        case 0: c->r[rd] += c->r[rm]; if (rd == REG_PC) c->r[REG_PC] &= ~1u; break;
        case 1: { uint64_t r = (uint64_t)c->r[rd] - c->r[rm]; set_nzcv_sub(c, c->r[rd], c->r[rm], r); break; }
        case 2: c->r[rd] = c->r[rm]; if (rd == REG_PC) c->r[REG_PC] &= ~1u; break;
        case 3:
            if (insn & 0x80) c->r[REG_LR] = c->r[REG_PC] | 1;
            if ((c->r[rm] & 0xFFFFFFF0) == 0xFFFFFFF0) exc_return(c, bus, c->r[rm]);
            else c->r[REG_PC] = c->r[rm] & ~1u;
            break;
        }
    }
    goto done_16;
}

/* ---- Group 9: LDR PC-relative ---- */
ldr_pc: {
    int rt = (insn >> 8) & 7;
    uint32_t imm8 = (insn & 0xFF) << 2;
    c->r[rt] = membus_read32(bus, ((pc + 4) & ~3u) + imm8);
    goto done_16;
}

/* ---- Groups 10-11: Load/store register offset ---- */
ls_reg: {
    int op = (insn >> 9) & 7, rm = (insn >> 6) & 7, rn = (insn >> 3) & 7, rt = insn & 7;
    uint32_t addr = c->r[rn] + c->r[rm];
    switch (op) {
    case 0: membus_write32(bus, addr, c->r[rt]); break;
    case 1: membus_write16(bus, addr, c->r[rt]); break;
    case 2: membus_write8(bus, addr, c->r[rt]); break;
    case 3: c->r[rt] = (int8_t)membus_read8(bus, addr); break;
    case 4: c->r[rt] = membus_read32(bus, addr); break;
    case 5: c->r[rt] = membus_read16(bus, addr); break;
    case 6: c->r[rt] = membus_read8(bus, addr); break;
    case 7: c->r[rt] = (int16_t)membus_read16(bus, addr); break;
    }
    goto done_16;
}
str_imm: { int imm5=((insn>>6)&0x1F)<<2,rn=(insn>>3)&7,rt=insn&7; membus_write32(bus,c->r[rn]+imm5,c->r[rt]); goto done_16; }
ldr_imm: { int imm5=((insn>>6)&0x1F)<<2,rn=(insn>>3)&7,rt=insn&7; c->r[rt]=membus_read32(bus,c->r[rn]+imm5); goto done_16; }
strb_imm: { int imm5=(insn>>6)&0x1F,rn=(insn>>3)&7,rt=insn&7; membus_write8(bus,c->r[rn]+imm5,c->r[rt]); goto done_16; }
ldrb_imm: { int imm5=(insn>>6)&0x1F,rn=(insn>>3)&7,rt=insn&7; c->r[rt]=membus_read8(bus,c->r[rn]+imm5); goto done_16; }
strh_imm: { int imm5=((insn>>6)&0x1F)<<1,rn=(insn>>3)&7,rt=insn&7; membus_write16(bus,c->r[rn]+imm5,c->r[rt]); goto done_16; }
ldrh_imm: { int imm5=((insn>>6)&0x1F)<<1,rn=(insn>>3)&7,rt=insn&7; c->r[rt]=membus_read16(bus,c->r[rn]+imm5); goto done_16; }
str_sp: { int rt=(insn>>8)&7; membus_write32(bus,c->r[REG_SP]+((insn&0xFF)<<2),c->r[rt]); goto done_16; }
ldr_sp: { int rt=(insn>>8)&7; c->r[rt]=membus_read32(bus,c->r[REG_SP]+((insn&0xFF)<<2)); goto done_16; }
adr: { c->r[(insn>>8)&7]=((pc+4)&~3u)+((insn&0xFF)<<2); goto done_16; }
add_sp_imm: { c->r[(insn>>8)&7]=c->r[REG_SP]+((insn&0xFF)<<2); goto done_16; }
misc: {
    if ((insn&0xFF00)==0xB000) { int imm7=(insn&0x7F)<<2; if(insn&0x80) c->r[REG_SP]-=imm7; else c->r[REG_SP]+=imm7; goto done_16; }
    if ((insn&0xFE00)==0xB400) { int regs=insn&0xFF,lr=(insn>>8)&1; if(lr){c->r[REG_SP]-=4;membus_write32(bus,c->r[REG_SP],c->r[REG_LR]);} for(int i=7;i>=0;i--) if(regs&(1<<i)){c->r[REG_SP]-=4;membus_write32(bus,c->r[REG_SP],c->r[i]);} goto done_16; }
    if ((insn&0xFE00)==0xBC00) { int regs=insn&0xFF,pc_bit=(insn>>8)&1; for(int i=0;i<8;i++) if(regs&(1<<i)){c->r[i]=membus_read32(bus,c->r[REG_SP]);c->r[REG_SP]+=4;} if(pc_bit){uint32_t val=membus_read32(bus,c->r[REG_SP]);c->r[REG_SP]+=4;if((val&0xFFFFFFF0)==0xFFFFFFF0)exc_return(c,bus,val);else c->r[REG_PC]=val&~1u;} goto done_16; }
    if (insn==0xB672) { c->primask=1; goto done_16; }
    if (insn==0xB662) { c->primask=0; c->irq_shadow=1; goto done_16; }
    if ((insn&0xFF00)==0xBF00) { uint8_t mask=insn&0xF; if(mask==0) goto done_16; c->it_state=(((insn>>4)&0xF)<<4)|mask; goto done_16; }
    if ((insn&0xF500)==0xB100) { int rn=insn&7,nz=(insn>>11)&1,imm=((insn>>3)&0x1F)<<1|((insn>>9)&1)<<6; if(nz?(c->r[rn]!=0):(c->r[rn]==0)) c->r[REG_PC]=pc+4+imm; goto done_16; }
    if ((insn&0xFFC0)==0xB200) { c->r[insn&7]=(int16_t)(c->r[(insn>>3)&7]&0xFFFF); goto done_16; }
    if ((insn&0xFFC0)==0xB240) { c->r[insn&7]=(int8_t)(c->r[(insn>>3)&7]&0xFF); goto done_16; }
    if ((insn&0xFFC0)==0xB280) { c->r[insn&7]=c->r[(insn>>3)&7]&0xFFFF; goto done_16; }
    if ((insn&0xFFC0)==0xB2C0) { c->r[insn&7]=c->r[(insn>>3)&7]&0xFF; goto done_16; }
    if ((insn&0xFFC0)==0xBA00) { uint32_t v=c->r[(insn>>3)&7]; c->r[insn&7]=((v>>24)&0xFF)|((v>>8)&0xFF00)|((v<<8)&0xFF0000)|((v<<24)&0xFF000000); goto done_16; }
    if ((insn&0xFFC0)==0xBA40) { uint32_t v=c->r[(insn>>3)&7]; c->r[insn&7]=((v&0xFF00FF00)>>8)|((v&0x00FF00FF)<<8); goto done_16; }
    if ((insn&0xFF00)==0xBE00) {
        if (insn==0xBE00) { c->r[REG_PC]-=2; return CPU_BREAKPOINT; }
        if (insn==0xBEAB) { uint32_t func=c->r[0],arg=c->r[1]; switch(func) {
            case 0x03: fputc(membus_read8(bus,arg),stderr); break;
            case 0x04: { char ch; while((ch=membus_read8(bus,arg++))!='\0') fputc(ch,stderr); break; }
            case 0x10: { struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts); c->r[0]=(uint32_t)(ts.tv_sec*1000000ULL+ts.tv_nsec/1000); break; }
            case 0x18: { uint32_t code=arg; if(arg>=RAM_BASE&&arg<RAM_BASE+RAM_SIZE) code=membus_read32(bus,arg+4); else if(arg==0x20026) code=0; return CPU_SEMIHOST_EXIT|(code&0xFF); }
            default: c->r[0]=(uint32_t)-1; break;
        } }
        goto done_16;
    }
    goto done_16;
}
stm: { int rn=(insn>>8)&7,regs=insn&0xFF; uint32_t addr=c->r[rn]; for(int i=0;i<8;i++) if(regs&(1<<i)){membus_write32(bus,addr,c->r[i]);addr+=4;} c->r[rn]=addr; goto done_16; }
ldm: { int rn=(insn>>8)&7,regs=insn&0xFF; uint32_t addr=c->r[rn]; for(int i=0;i<8;i++) if(regs&(1<<i)){c->r[i]=membus_read32(bus,addr);addr+=4;} c->r[rn]=addr; goto done_16; }
cond_branch: { int cond=(insn>>8)&0xF; if(cond>=0xE) goto done_16; if(cond_check(c,cond)){int32_t off=(int8_t)(insn&0xFF); c->r[REG_PC]=pc+4+(off<<1);} goto done_16; }
branch: { int32_t off=insn&0x7FF; if(off&0x400) off|=0xFFFFF800; c->r[REG_PC]=pc+4+(off<<1); goto done_16; }
unknown16: { fprintf(stderr,"Unknown 16-bit insn: 0x%04X at 0x%08X LR=0x%08X\n",insn,pc,c->r[14]); exit(1); }

done_16:
    if (in_it) {
        c->xpsr = (c->xpsr & ~(FLAG_N | FLAG_Z | FLAG_C | FLAG_V)) | saved_flags;
    }
    return 0;

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
        uint32_t base = c->r[rn];
        if (rn == REG_PC) base &= ~3u;  /* PC must be word-aligned */
        c->r[rt] = membus_read32(bus, base + imm12);
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
            if (rn == REG_PC) addr &= ~3u;
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

    /* LDRSH.W — 12-bit immediate */
    if ((hi & 0xFFF0) == 0xF9B0) {
        int rn = hi & 0xF; int rt = (lo >> 12) & 0xF;
        c->r[rt] = (int16_t)membus_read16(bus, c->r[rn] + (lo & 0xFFF));
        return 0;
    }

    /* LDRSH.W — register offset or pre/post index */
    if ((hi & 0xFFF0) == 0xF930) {
        int rn = hi & 0xF; int rt = (lo >> 12) & 0xF;
        if (lo & 0x0800) {
            int p = (lo >> 10) & 1; int u = (lo >> 9) & 1; int w = (lo >> 8) & 1;
            uint32_t imm8 = lo & 0xFF;
            uint32_t addr = c->r[rn];
            uint32_t offset = u ? imm8 : -imm8;
            if (p) addr += offset;
            c->r[rt] = (int16_t)membus_read16(bus, addr);
            if (!p) addr += offset;
            if (w || !p) c->r[rn] = addr;
        } else {
            int rm = lo & 0xF; int shift = (lo >> 4) & 3;
            c->r[rt] = (int16_t)membus_read16(bus, c->r[rn] + (c->r[rm] << shift));
        }
        return 0;
    }

    /* STRH.W — 12-bit immediate offset */
    if ((hi & 0xFFF0) == 0xF8A0) {
        int rn = hi & 0xF; int rt = (lo >> 12) & 0xF;
        uint32_t imm12 = lo & 0xFFF;
        membus_write16(bus, c->r[rn] + imm12, c->r[rt]);
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

    /* ADDW Rd, Rn, #imm12 (T4 encoding) */
    if ((hi & 0xFBF0) == 0xF200) {
        int rd = (lo >> 8) & 0xF;
        int rn = hi & 0xF;
        uint32_t imm = ((hi & 0x0400) << 1) | ((lo & 0x7000) >> 4) | (lo & 0xFF);
        c->r[rd] = c->r[rn] + imm;
        return 0;
    }

    /* SUBW Rd, Rn, #imm12 (T4 encoding) */
    if ((hi & 0xFBF0) == 0xF2A0) {
        int rd = (lo >> 8) & 0xF;
        int rn = hi & 0xF;
        uint32_t imm = ((hi & 0x0400) << 1) | ((lo & 0x7000) >> 4) | (lo & 0xFF);
        c->r[rd] = c->r[rn] - imm;
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
    if ((hi & 0xFA00) == 0xF000 && !(lo & 0x8000)
        && (hi & 0xFFF0) != 0xF380
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
        case 0: c->r[rd] = c->xpsr & 0xF8000000; break; /* APSR (flags) */
        case 3: c->r[rd] = c->xpsr; break;              /* xPSR (all) */
        case 5: c->r[rd] = c->xpsr & 0x1FF; break;      /* IPSR (vector number) */
        case 6: c->r[rd] = c->xpsr & 0xF80001FF; break; /* EPSR+IPSR */
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

    fprintf(stderr, "Unknown 32-bit insn: 0x%08X at 0x%08X LR=0x%08X\n",
            insn, pc, c->r[14]);
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

    /* Set IPSR (xPSR bits [8:0]) to active vector number */
    c->xpsr = (c->xpsr & ~0x1FF) | (vector_num & 0x1FF);

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
    c->xpsr &= ~0x1FF;  /* Clear IPSR — back to thread mode */

    /* Record which task/context we're returning to */


}

/* take_interrupt and exc_return are used by nvic.c via the header */
