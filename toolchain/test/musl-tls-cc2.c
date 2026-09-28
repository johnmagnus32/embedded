/* musl-tls-cc2.c — the -fPIC half compiled by our cc (general-dynamic via __tls_get_addr). */
__thread int other = 3;
int bump(void) { static __thread int local_tls = 40; return ++local_tls + other; }
