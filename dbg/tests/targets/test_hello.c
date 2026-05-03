#include "startup.h"
volatile int marker = 0;
void main(void) {
    marker = 0xDEADBEEF;
    while (1) {}
}
