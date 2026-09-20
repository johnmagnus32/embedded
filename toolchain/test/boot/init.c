/*
 * init.c — the minimal userspace for the custom-toolchain boot test. It is compiled,
 * assembled, and linked ENTIRELY by our from-scratch toolchain (cpp -> cc -> as -> ld)
 * against our own libc.a, packed as /init, and booted as PID 1 on a mainline ARM kernel
 * under QEMU -M virt. If the banner + the computed value appear on the console, the whole
 * chain works end to end: our preprocessor, compiler, assembler, linker, and libc.
 *
 * printf pulls a real slice of libc (formatting + write + malloc + qsort's closure), so a
 * successful boot also exercises the extern/static/function-pointer fixes that let the full
 * multi-object libc link succeed. `add` is `static` -> a file-local symbol.
 */
#include <stdio.h>

static int add(int a, int b) { return a + b; }

int main(void)
{
	printf("\n[custom-toolchain] init: built by our own cpp -> cc -> as -> ld + libc.a\n");
	printf("[custom-toolchain] booted as PID 1 on the mainline kernel (QEMU -M virt)\n");
	printf("[custom-toolchain] codegen proof: add(2, 3) = %d\n", add(2, 3));
	return 0;
}
