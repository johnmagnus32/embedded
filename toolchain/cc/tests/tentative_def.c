// expect: 0
/* A tentative definition followed by the real one is ONE object (kernel trace events: `static struct X ev;` then
 * `static struct X ev = { .self = &ev, ... };`). Also extern-then-define, and an array completed later. */
struct ev { struct ev *self; int v; };
static struct ev e1;
static struct ev e1 = { &e1, 7 };
extern int g2;
int g2 = 5;
extern int arr[];
int arr[3] = { 1, 2, 3 };
int main(void) {
	if (e1.self != &e1 || e1.v != 7) return 1;
	if (g2 != 5) return 2;
	if (sizeof(arr) != 12 || arr[2] != 3) return 3;
	return 0;
}
