// error: unsupported attribute 'constructor'
static void __attribute__((constructor)) init(void) { }   /* would need .init_array: an error, not silently dropped */
