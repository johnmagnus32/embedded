// expect: 42
/* A transparent_union parameter (Linux: release_pages(release_pages_arg, int)): a call may pass any member's type,
 * and it travels as the first member — here, the pointer itself, not the bytes it points at. */
struct page { int v; }; struct folio { int w; };
typedef union { struct page **pages; struct folio **folios; } rp_arg __attribute__((__transparent_union__));
union __attribute__((transparent_union)) tu2 { int *ip; const char *cp; };
static int first(rp_arg a, int n) { return a.pages[n - 1]->v; }
static int deref(union tu2 u) { return *u.ip; }
int main(void) {
	struct page p0 = { 30 }, p1 = { 10 }; struct page *pp[2] = { &p0, &p1 };
	struct folio f = { 2 }; struct folio *fp[1] = { &f };
	int x = 0; rp_arg whole = { pp };
	x += first(pp + 1, 1);               /* struct page ** */
	x += first((struct page **)fp, 1) - 2 + first(whole, 1) - 30 + first(whole, 2) - 10;   /* 0: the union itself */
	int k = 2;
	return x + deref(&k) + first(pp, 1);   /* 10 + 2 + 30 */
}
