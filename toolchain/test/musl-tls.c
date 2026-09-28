/* musl-tls.c — thread-local storage through our ld (with musl-tls2.c). This file is non-PIC: local-exec for what it
 * defines, initial-exec for `other`; musl-tls2.c is -fPIC: general- and local-dynamic via __tls_get_addr. A second
 * thread must see its OWN copies, initialized from the template (.tdata) and zeroed (.tbss). */
#include <stdio.h>
#include <pthread.h>
__thread int counter = 100;                  /* .tdata */
__thread char buf[64];                       /* .tbss */
static __thread long long big = 7;           /* 8-aligned */
extern __thread int other;                   /* defined in musl-tls2.c */
int bump(void);
static void *worker(void *arg)
{
	(void)arg;
	counter += 1; buf[0] = buf[1] ? 'x' : 'w'; other = 5;   /* this thread's copies: 101, 'w' (zeroed tail), 5 */
	return (void *)(long)(counter + bump() + (int)big);    /* 101 + (41 + 5) + 7 */
}
int main(void)
{
	pthread_t t; void *r;
	counter = 1; buf[0] = 'm'; other = 9; big = 70;
	if (pthread_create(&t, 0, worker, 0) || pthread_join(t, &r)) { puts("tls: pthread failed"); return 1; }
	printf("musl-tls %d %c %d %ld %lld %d\n", counter, buf[0], other, (long)r, big, bump());   /* main: 1 m 9 154 70 (41+9) */
	return 0;
}
