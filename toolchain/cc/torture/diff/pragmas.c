/* pragmas.c — the pragmas that change a program, as GCC applies them: #pragma pack in its forms (n, (), push, push n,
 * pop) capping member alignment — scalars, arrays, nested structs, bitfields, an aligned(N) member, a packed attribute,
 * an aligned struct, a union — through _Pragma too; #pragma weak on an undefined function (its address is 0) and as a
 * weak alias; #pragma redefine_extname (a declaration's symbol). Prints the layouts; must match GCC. */
int printf(const char *, ...);
#define L(T) printf("%-6s size %2d align %2d\n", #T, (int)sizeof(struct T), (int)__alignof__(struct T))
#define O(T, m) printf("  %s.%s at %d\n", #T, #m, (int)__builtin_offsetof(struct T, m))
struct n0 { char c; int i; double d; short s; };
#pragma pack(2)
struct p2 { char c; int i; double d; short s; };
struct inner { char c; long long x; };
#pragma pack(push, 1)
struct p1 { char c; int i; double d; struct inner in; short arr[3]; };
struct bf1 { char c; unsigned a : 3, b : 13; int i : 20; char d; };
struct al1 { char c; int i __attribute__((aligned(8))); };
union u1 { char c; double d; int i; };
#pragma pack(push)
#pragma pack(4)
struct p4 { char c; double d; long long ll; };
#pragma pack(pop)
struct back1 { char c; int i; };
#pragma pack(pop)
struct back2 { char c; double d; };
#pragma pack()
struct dflt { char c; double d; };
_Pragma("pack(1)") struct viap { char c; int i; }; _Pragma("pack()")
#pragma pack(8)
struct p8 { char c; double d; };
struct __attribute__((aligned(16))) a16 { char c; int i; };
#pragma pack()
struct __attribute__((packed)) pk { char c; int i; };
#pragma pack(16)
struct p16 { char c; double d; };
#pragma pack()

#pragma pack(2)
struct bf2 { char c; int a : 20; int b : 20; char d; };
struct bf2b { char c; short s : 9; int x : 17; char e; };
#pragma pack(4)
struct bf4 { char c; long long a : 40; char d; int :0; char f; };
#pragma pack()
struct bfn { char c; int a : 20; int b : 20; char d; };
static void bytes(const char *n, const void *p, int sz) {
	printf("%s:", n); for (int i = 0; i < sz; i++) printf(" %02x", ((const unsigned char *)p)[i]); printf("\n");
}

#pragma weak missing_fn
void missing_fn(void);
int real_impl(void) { return 42; }
#pragma weak alias_fn = real_impl
int alias_fn(void);
#pragma redefine_extname renamed_decl real_impl
int renamed_decl(void);

int main(void) {
	L(n0); O(n0, i); O(n0, d); O(n0, s);
	L(p2); O(p2, i); O(p2, d); O(p2, s);
	L(inner); O(inner, x);
	L(p1); O(p1, i); O(p1, d); O(p1, in); O(p1, arr);
	L(bf1); O(bf1, d);
	L(al1); O(al1, i);
	printf("u1     size %2d align %2d\n", (int)sizeof(union u1), (int)__alignof__(union u1));
	L(p4); O(p4, d); O(p4, ll);
	L(back1); O(back1, i);
	L(back2); O(back2, d);
	L(dflt); O(dflt, d);
	L(viap); O(viap, i);
	L(p8); O(p8, d);
	L(a16); O(a16, i);
	L(pk); O(pk, i);
	L(p16); O(p16, d);
	L(bf2); O(bf2, d); L(bf2b); O(bf2b, e); L(bf4); O(bf4, d); O(bf4, f); L(bfn); O(bfn, d);
	{ struct bf2 a = { 0x11, 0xabcde, 0x54321, 0x22 }; bytes("bf2", &a, sizeof a); }
	{ struct bf2b a = { 0x11, 0x1a5, 0x1bcde, 0x33 }; bytes("bf2b", &a, sizeof a); }
	{ struct bf4 a = { 0x11, 0x123456789aLL, 0x22, 0x44 }; bytes("bf4", &a, sizeof a); }
	{ struct bfn a = { 0x11, 0xabcde, 0x54321, 0x22 }; bytes("bfn", &a, sizeof a); }
	struct p1 v = { 1, 2, 3.5, { 4, 5 }, { 6, 7, 8 } };
	printf("p1 values %d %d %d %d %lld %d\n", v.c, v.i, (int)(v.d * 2), v.in.c, v.in.x, v.arr[2]);
	printf("weak: %d alias: %d renamed: %d\n", &missing_fn == 0, alias_fn(), renamed_decl());
	return 0;
}
