/*
 * gic.h — ARM GIC-400 (GICv2) minimal driver: enable one PPI, ack/EOI.
 */
#ifndef GV3K_GIC_H
#define GV3K_GIC_H

#include <stdint.h>

void gic_init(void);              /* enable distributor + this CPU interface */
void gic_enable_intid(uint32_t intid, uint8_t priority);
uint32_t gic_ack(void);           /* read IAR -> INTID (0x3FF = spurious) */
void gic_eoi(uint32_t iar);       /* write EOIR with the value from gic_ack */

#define GIC_SPURIOUS  0x3FFu

#endif /* GV3K_GIC_H */
