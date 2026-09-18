/*
 * reboot.c — reboot(cmd): the 4-arg kernel syscall reboot(magic1, magic2, cmd, arg),
 * with the magic constants supplied so callers pass just an RB_* command.
 */
#include <sys/reboot.h>
#include "syscall_internal.h"

int reboot(int cmd)
{
	return (int)__ret(__syscall6(SYS_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
	                             (long)(unsigned)cmd, 0, 0, 0));
}
