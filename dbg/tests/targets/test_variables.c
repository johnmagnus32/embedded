#include "startup.h"
int g_int;

struct point { int x; int y; };

void main(void) {
    g_int = 42;                                 /* line 7 */
    int local_int = 7;                          /* line 8 */
    struct point p = {10, 20};                  /* line 9 */
    int arr[4] = {1, 2, 3, 4};                 /* line 10 */
    struct point *pp = &p;                      /* line 11 */
    volatile int sink = local_int + p.x + arr[2] + pp->y;  /* line 12 */
    (void)sink;
    while (1) {}  /* line 14 */
}
