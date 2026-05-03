#include "startup.h"
__attribute__((noinline)) int add(int a, int b) { return a + b; }
__attribute__((noinline)) int multiply(int a, int b) {
    int result = 0;
    for (int i = 0; i < b; i++)
        result = add(result, a);
    return result;
}
void main(void) {
    volatile int x = multiply(3, 4);  /* line 10 */
    volatile int y = add(x, 1);       /* line 11 */
    while (1) {}
}
