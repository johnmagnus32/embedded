// error: unsupported attribute 'frobnicate'
static void __attribute__((frobnicate)) init(void) { }   /* an attribute we don't implement: an error, not silently dropped */
