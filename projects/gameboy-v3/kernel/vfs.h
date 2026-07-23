/*
 * vfs.h — the small "virtual filesystem" glue layer (S9).
 *
 * Sits between the syscalls/proc and the ramfs: resolves paths (relative to a
 * process cwd), opens inodes into struct files, and loads an ELF from a file.
 * It's thin because we have exactly one filesystem (ramfs); the point is that
 * execve and the syscalls talk to THIS interface, so swapping/adding a real fs
 * later doesn't touch them.
 */
#ifndef GV3K_VFS_H
#define GV3K_VFS_H

#include <stdint.h>

struct rf_inode;

/* Resolve `path` (absolute, or relative to `cwd`) to an inode, following the
 * final symlink iff `follow`. Returns NULL if missing. */
struct rf_inode *vfs_resolve(struct rf_inode *cwd, const char *path, int follow);

/* Load the ELF stored in file inode `ino` into address space `l1_pa`, applying
 * load `bias` (0 for ET_EXEC), and filling *info (entry, brk, phdrs, interp).
 * Returns 0, or negative on error. Reads the file into a temporary kernel
 * buffer, then hands it to elf_load(). */
struct elf_info;
int vfs_load_elf(struct rf_inode *ino, uint32_t l1_pa, uint32_t bias,
                 struct elf_info *info);

#endif /* GV3K_VFS_H */
