/*
 * nor_layout.h — SPI-NOR component layout shared by the bootloader and the flash
 * tooling. Single source of truth; keep in sync with ../projects/gameboy-v3/README.md (SPI-NOR layout).
 *
 * NOR holds boot COMPONENTS only (kernel/DTB/initramfs), never a bootloader:
 * offset 0 is kept blank of any eGON so the BROM always drops to FEL, and the
 * bootloader is delivered via FEL. A small component table at offset 0 (magic
 * "GV3NOR1", deliberately NOT "eGON.BT0") records each component's offset+size.
 */
#ifndef GV3_NOR_LAYOUT_H
#define GV3_NOR_LAYOUT_H

#include <stdint.h>

/* --- fixed NOR offsets (64 KB aligned) ------------------------------------ */
#define NOR_TABLE_OFF     0x000000u   /* component table (in the 64KB guard)   */
#define NOR_KERNEL_OFF    0x010000u   /* zImage / kernel image                 */
#define NOR_DTB_OFF       0x840000u   /* device tree blob (kernel gets 8.2 MiB) */
#define NOR_INITRD_OFF    0x850000u   /* initramfs.cpio.gz (to ~14.7 of 16 MiB)  */

/* --- DRAM load addresses (match main.c / boot.cmd) ------------------------ */
/* Clear of the DECOMPRESSED kernel (~56 MiB when built by our cc, so it ends near 0x43820000), with
 * margin for the decompressor relocating itself past that end when loaded low. See the note in main.c. */
#define NOR_KERNEL_ADDR   0x45000000u
#define NOR_DTB_ADDR      0x46800000u
#define NOR_INITRD_ADDR   0x46000000u

/* --- component table (little-endian) at NOR_TABLE_OFF --------------------- */
#define NOR_TABLE_MAGIC   "GV3NOR1"   /* 8 bytes incl. NUL; NOT "eGON.BT0"     */
#define NOR_TABLE_VERSION 1u

struct nor_table {
	char     magic[8];      /* "GV3NOR1\0"                                    */
	uint32_t version;       /* = NOR_TABLE_VERSION                            */
	uint32_t reserved;
	uint32_t kernel_off;    /* = NOR_KERNEL_OFF                               */
	uint32_t kernel_size;   /* actual bytes                                   */
	uint32_t dtb_off;       /* = NOR_DTB_OFF                                  */
	uint32_t dtb_size;
	uint32_t initrd_off;    /* = NOR_INITRD_OFF                               */
	uint32_t initrd_size;
	uint32_t crc32;         /* over bytes [0..0x27]; 0 = unchecked            */
} __attribute__((packed));  /* 0x2C bytes                                     */

#endif /* GV3_NOR_LAYOUT_H */
