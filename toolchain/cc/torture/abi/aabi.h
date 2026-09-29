/* aabi.h — AAPCS doubleword alignment of arguments, GCC 9+'s rule: a scalar by its type's natural alignment (an
 * aligned() typedef doesn't count), a struct by its members' alignments (a member's aligned() does, the struct's own
 * doesn't, packed members have none). Each case puts an int first, so an 8-aligned argument skips r1. */
typedef __builtin_va_list va_list;
typedef int a8i __attribute__((aligned(8)));
typedef double a16d __attribute__((aligned(16)));
struct __attribute__((aligned(8))) SA { int a, b; };          /* the struct's own: not doubleword */
struct SL { int a; long long b; };                             /* a long long member: doubleword */
struct SM { int a __attribute__((aligned(8))); int b; };       /* an aligned member: doubleword */
struct __attribute__((packed)) SP { char c; long long x; };    /* packed: no member alignment */
int f_va(int n, ...);                                          /* a8i varargs: plain words */
int f_sa(int x, struct SA s);
int f_sl(int x, struct SL s);
int f_sm(int x, struct SM s);
int f_sp(int x, struct SP s);
int f_vsa(int n, ...);                                         /* va_arg of struct SA: not 8-aligned */
__attribute__((pcs("aapcs"))) double f_cd(int x, _Complex double z);   /* base PCS: an even pair */
__attribute__((pcs("aapcs"))) double f_ad(int x, a16d d);
