/* musl-tls-cc.c — thread-local storage compiled by OUR cc (with musl-tls-cc2.c, -fPIC), linked by our ld against musl's
 * static libc, as musl-tls.c is for GCC: local-exec / initial-exec here, general-dynamic in the -fPIC half. */
typedef unsigned long pthread_t;
int pthread_create(pthread_t *t, const void *attr, void *(*fn)(void *), void *arg);
int pthread_join(pthread_t t, void **ret);
int printf(const char *fmt, ...);
int puts(const char *s);
_Thread_local int counter = 100;             /* .tdata */
__thread char buf[64];                       /* .tbss */
static _Thread_local long long big = 7;      /* 8-aligned */
extern __thread int other;                   /* defined in musl-tls-cc2.c: initial-exec */
int bump(void);
static void *worker(void *arg)
{
	(void)arg;
	counter += 1; buf[0] = buf[1] ? 'x' : 'w'; other = 5;
	return (void *)(long)(counter + bump() + (int)big);
}
int main(void)
{
	pthread_t t; void *r;
	counter = 1; buf[0] = 'm'; other = 9; big = 70;
	if (pthread_create(&t, 0, worker, 0) || pthread_join(t, &r)) { puts("tls: pthread failed"); return 1; }
	printf("musl-tls %d %c %d %ld %lld %d\n", counter, buf[0], other, (long)r, big, bump());
	return 0;
}
