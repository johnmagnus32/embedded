/* musl-tls2.c — the -fPIC half of the TLS test (general-dynamic `other`, local-dynamic `local_tls`). */
__thread int other = 3;
static __thread int local_tls = 40;
int bump(void) { return ++local_tls + other; }
