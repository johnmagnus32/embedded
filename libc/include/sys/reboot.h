/*
 * sys/reboot.h — reboot(2). The 1-arg glibc-style wrapper supplies the magic values.
 */
#ifndef _LIBC_SYS_REBOOT_H
#define _LIBC_SYS_REBOOT_H

#define RB_AUTOBOOT     0x01234567
#define RB_HALT_SYSTEM  0xcdef0123
#define RB_ENABLE_CAD   0x89abcdef
#define RB_DISABLE_CAD  0x00000000
#define RB_POWER_OFF    0x4321fedc

#define LINUX_REBOOT_MAGIC1 0xfee1dead
#define LINUX_REBOOT_MAGIC2 672274793

int reboot(int cmd);

#endif /* _LIBC_SYS_REBOOT_H */
