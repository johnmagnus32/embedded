/*
 * sys/mount.h — mount(2) / umount2(2) for gv3libc.
 *
 * Thin wrappers over SYS_mount / SYS_umount2 (the kernel accepts these; on the
 * custom kernel they currently succeed as no-ops — there is no procfs/sysfs/
 * devtmpfs backend yet, that is future work). Enough for a `mount` coreutil and
 * for /init to issue the standard proc/sys/dev mounts portably.
 */
#ifndef _GV3_SYS_MOUNT_H
#define _GV3_SYS_MOUNT_H

/* umount2() flags (subset; matches Linux). */
#define MNT_FORCE       0x00000001
#define MNT_DETACH      0x00000002

int mount(const char *source, const char *target, const char *fstype,
          unsigned long flags, const void *data);
int umount2(const char *target, int flags);
int umount(const char *target);   /* umount2(target, 0) */

#endif /* _GV3_SYS_MOUNT_H */
