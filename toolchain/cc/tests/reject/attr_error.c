// error: declared with attribute error
/* __attribute__((error)) on a call that SURVIVES. This is the kernel's BUILD_BUG_ON, 4210 sites of it. */
extern void boom(void) __attribute__((__error__("BUILD_BUG_ON failed")));
int f(void) { if (!(sizeof(int) == 8)) boom(); return 0; }
