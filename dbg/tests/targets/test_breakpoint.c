#include "startup.h"
volatile int counter = 0;
__attribute__((noinline)) void func_a(void) { counter++; }
__attribute__((noinline)) void func_b(void) { counter += 10; }
void main(void) {
    func_a();  /* line 6 */
    func_b();  /* line 7 */
    func_a();  /* line 8 */
    while (1) {}
}
