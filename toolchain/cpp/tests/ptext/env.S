/* -P output, byte for byte: U-Boot's board .env files go through `cpp -P -x assembler-with-cpp -undef` and an awk
 * script that reads a leading space as a continuation line */

/* the first line after comments and blank lines starts in column 1 */

stdin=serial,usbkbd
stdout=serial,vidconsole
bootcmd=run distro_bootcmd
	tab_continued=1
    four_spaces=2
#define ADDR 0x40000000
fdt_addr=ADDR
  ADDR_indented
/* c */ after_comment=3
