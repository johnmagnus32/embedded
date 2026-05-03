#include "startup.h"
volatile int depth = 0;
__attribute__((noinline)) void func_c(void) { depth++; while (1) {} }
__attribute__((noinline)) void func_b(void) { func_c(); }
__attribute__((noinline)) void func_a(void) { func_b(); }
void main(void) { func_a(); }
