/*
 * fat.c — read-only FAT16/FAT32 reader for the gameboy-v3 bootloader.
 *
 * Flow: MBR (LBA 0) -> first FAT partition -> BPB -> detect FAT16/32 by cluster
 * count -> walk the root directory (reassembling VFAT long filenames) -> follow
 * the cluster chain to load a file. Reads go through sd_read_blocks().
 *
 * All offsets/thresholds verified against the Microsoft FAT spec (fatgen103)
 * and osdev. Our image is FAT16 (mkfs.vfat on a 63 MB partition); FAT32 is
 * handled too so a larger partition still works.
 */

#include <stdint.h>
#include "fat.h"
#include "sdcard.h"

int printf(const char *fmt, ...);

/* ---- little-endian readers ------------------------------------------------ */
static uint16_t rd16(const uint8_t *p) { return p[0] | (p[1] << 8); }
static uint32_t rd32(const uint8_t *p) {
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
	       ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* ---- mounted-volume state ------------------------------------------------- */
static struct {
	uint32_t part_lba;        /* partition start (absolute LBA)          */
	uint32_t fat_start;       /* first FAT sector (absolute)             */
	uint32_t root_start;      /* root dir sector (FAT16) — absolute      */
	uint32_t root_sectors;    /* root dir length in sectors (FAT16)      */
	uint32_t data_start;      /* first data sector (absolute)            */
	uint32_t sec_per_clus;
	uint32_t bytes_per_sec;
	uint32_t root_cluster;    /* FAT32 root dir cluster                  */
	int      is_fat32;
} vol;

static uint8_t secbuf[512];   /* scratch for one sector */

/* absolute-LBA sector read of exactly one 512-byte sector */
static int read_sec(uint32_t lba, uint8_t *dst) { return sd_read_blocks(lba, 1, dst); }

/* ---- mount: parse MBR + BPB ----------------------------------------------- */
int fat_mount(void)
{
	if (read_sec(0, secbuf) != 0) return -1;
	if (rd16(secbuf + 0x1FE) != 0xAA55) { printf("FAT: no MBR signature\n"); return -2; }

	/* Find the first FAT-type partition in the MBR table (0x1BE, 4 x 16B). */
	uint32_t part_lba = 0;
	for (int i = 0; i < 4; i++) {
		const uint8_t *e = secbuf + 0x1BE + i * 16;
		uint8_t type = e[0x04];
		if (type == 0x0B || type == 0x0C ||    /* FAT32 */
		    type == 0x0E || type == 0x06 || type == 0x04) {  /* FAT16 */
			part_lba = rd32(e + 0x08);
			break;
		}
	}
	if (part_lba == 0) { printf("FAT: no FAT partition in MBR\n"); return -3; }
	vol.part_lba = part_lba;

	/* Read the volume boot record / BPB. */
	if (read_sec(part_lba, secbuf) != 0) return -4;
	const uint8_t *b = secbuf;
	vol.bytes_per_sec = rd16(b + 0x0B);
	vol.sec_per_clus  = b[0x0D];
	uint32_t rsvd     = rd16(b + 0x0E);
	uint32_t num_fats = b[0x10];
	uint32_t root_ent = rd16(b + 0x11);
	uint32_t totsec16 = rd16(b + 0x13);
	uint32_t fatsz16  = rd16(b + 0x16);
	uint32_t totsec32 = rd32(b + 0x20);
	uint32_t fatsz32  = rd32(b + 0x24);

	if (vol.bytes_per_sec != 512) { printf("FAT: unsupported sector size %u\n", vol.bytes_per_sec); return -5; }

	uint32_t fatsz  = fatsz16 ? fatsz16 : fatsz32;
	uint32_t totsec = totsec16 ? totsec16 : totsec32;
	uint32_t root_dir_sectors = ((root_ent * 32) + (vol.bytes_per_sec - 1)) / vol.bytes_per_sec;

	vol.fat_start   = part_lba + rsvd;
	vol.root_start  = vol.fat_start + num_fats * fatsz;          /* FAT16 root region */
	vol.root_sectors = root_dir_sectors;
	vol.data_start  = vol.root_start + root_dir_sectors;

	/* FAT type detection: by data cluster count (fatgen103). */
	uint32_t data_sectors = totsec - (rsvd + num_fats * fatsz + root_dir_sectors);
	uint32_t clusters = vol.sec_per_clus ? data_sectors / vol.sec_per_clus : 0;
	vol.is_fat32 = (clusters >= 65525);
	vol.root_cluster = rd32(b + 0x2C);      /* only meaningful on FAT32 */

	printf("FAT%d mounted: part@%u data@%u spc=%u clusters=%u\n",
	       vol.is_fat32 ? 32 : 16, vol.part_lba, vol.data_start,
	       vol.sec_per_clus, clusters);
	return 0;
}

/* ---- FAT entry lookup (next cluster in the chain) ------------------------- */
static uint32_t fat_next(uint32_t clus)
{
	uint32_t fat_off = vol.is_fat32 ? clus * 4 : clus * 2;
	uint32_t sec = vol.fat_start + fat_off / 512;
	uint32_t off = fat_off % 512;
	if (read_sec(sec, secbuf) != 0) return 0x0FFFFFFF;
	if (vol.is_fat32)
		return rd32(secbuf + off) & 0x0FFFFFFF;
	return rd16(secbuf + off);
}
static int fat_is_eoc(uint32_t clus)
{
	return vol.is_fat32 ? (clus >= 0x0FFFFFF8) : (clus >= 0xFFF8);
}
static uint32_t clus_to_lba(uint32_t clus)
{
	return vol.data_start + (clus - 2) * vol.sec_per_clus;
}

/* ---- name matching -------------------------------------------------------- */
/* case-insensitive ASCII compare */
static int ieq(const char *a, const char *b)
{
	while (*a && *b) {
		char ca = *a, cb = *b;
		if (ca >= 'A' && ca <= 'Z') ca += 32;
		if (cb >= 'A' && cb <= 'Z') cb += 32;
		if (ca != cb) return 0;
		a++; b++;
	}
	return *a == *b;
}

/* extract the 13 UCS-2 chars from one LFN entry into out[] (ASCII-folded) */
static void lfn_chars(const uint8_t *e, char *out)
{
	static const int pos[13] = {1,3,5,7,9, 14,16,18,20,22,24, 28,30};
	for (int i = 0; i < 13; i++) {
		uint16_t c = e[pos[i]] | (e[pos[i] + 1] << 8);
		out[i] = (c == 0 || c == 0xffff) ? 0 : (char)(c & 0xff);
	}
}

/* build the 8.3 short name into out ("NAME.EXT", lowercased, no padding) */
static void short_name(const uint8_t *e, char *out)
{
	int o = 0;
	for (int i = 0; i < 8 && e[i] != ' '; i++) out[o++] = e[i];
	if (e[8] != ' ') {
		out[o++] = '.';
		for (int i = 8; i < 11 && e[i] != ' '; i++) out[o++] = e[i];
	}
	out[o] = 0;
	for (int i = 0; out[i]; i++)
		if (out[i] >= 'A' && out[i] <= 'Z') out[i] += 32;
}

/* ---- directory scan: find `name`, return (first_cluster, size) ------------ */
static int dir_find(const char *name, uint32_t *first_clus, uint32_t *size)
{
	char lfn[260];
	int lfn_len = 0;
	lfn[0] = 0;

	/* iterate directory entries. FAT16: fixed root region; FAT32: chain. */
	uint32_t clus = vol.root_cluster;             /* FAT32 */
	uint32_t sec_in_region = 0;

	for (;;) {
		uint32_t lba;
		if (!vol.is_fat32) {
			if (sec_in_region >= vol.root_sectors) break;
			lba = vol.root_start + sec_in_region;
		} else {
			if (clus < 2 || fat_is_eoc(clus)) break;
			lba = clus_to_lba(clus) + (sec_in_region % vol.sec_per_clus);
		}
		if (read_sec(lba, secbuf) != 0) return -1;

		for (int i = 0; i < 512; i += 32) {
			uint8_t *e = secbuf + i;
			if (e[0] == 0x00) return -2;          /* end of directory */
			if (e[0] == 0xE5) { lfn_len = 0; lfn[0] = 0; continue; }
			uint8_t attr = e[0x0B];
			if (attr == 0x0F) {                    /* LFN entry */
				uint8_t ord = e[0] & 0x1F;
				char part[13];
				lfn_chars(e, part);
				/* ordinal N is (N-1)*13 chars in; entries appear
				 * high-ordinal-first, before the 8.3 entry. */
				int base = (ord - 1) * 13;
				if (base >= 0 && base + 13 < (int)sizeof(lfn)) {
					for (int k = 0; k < 13; k++) lfn[base + k] = part[k];
					if (base + 13 > lfn_len) lfn_len = base + 13;
				}
				continue;
			}
			if (attr & 0x08) { lfn_len = 0; lfn[0] = 0; continue; } /* vol label */

			/* real 8.3 entry — decide the name to compare */
			char cand[260];
			if (lfn_len > 0) {
				/* NUL-terminate the reassembled LFN */
				int n = lfn_len;
				if (n >= (int)sizeof(lfn)) n = sizeof(lfn) - 1;
				lfn[n] = 0;
				for (int k = 0; k <= n; k++) cand[k] = lfn[k];
			} else {
				short_name(e, cand);
			}
			lfn_len = 0; lfn[0] = 0;

			if (ieq(cand, name)) {
				*first_clus = ((uint32_t)rd16(e + 0x14) << 16) | rd16(e + 0x1A);
				*size = rd32(e + 0x1C);
				return 0;
			}
		}

		sec_in_region++;
		if (vol.is_fat32 && (sec_in_region % vol.sec_per_clus) == 0)
			clus = fat_next(clus);
	}
	return -3;   /* not found */
}

/* ---- public: load a file by name ------------------------------------------ */
long fat_load(const char *name, void *buf, uint32_t max)
{
	uint32_t first_clus = 0, size = 0;
	if (dir_find(name, &first_clus, &size) != 0) {
		printf("FAT: '%s' not found\n", name);
		return -1;
	}
	if (size > max) {
		printf("FAT: '%s' (%u B) exceeds buffer (%u B)\n", name, size, max);
		return -2;
	}

	uint8_t *out = (uint8_t *)buf;
	uint32_t remaining = size;
	uint32_t clus = first_clus;
	uint32_t clus_bytes = vol.sec_per_clus * 512u;

	while (remaining > 0 && clus >= 2 && !fat_is_eoc(clus)) {
		uint32_t lba = clus_to_lba(clus);
		uint32_t want = remaining < clus_bytes ? remaining : clus_bytes;
		uint32_t secs = (want + 511) / 512;
		if (sd_read_blocks(lba, secs, out) != 0) return -3;
		out += want;
		remaining -= want;
		clus = fat_next(clus);
	}
	if (remaining != 0) { printf("FAT: '%s' chain ended early\n", name); return -4; }

	printf("FAT: loaded '%s' (%u bytes)\n", name, size);
	return (long)size;
}
