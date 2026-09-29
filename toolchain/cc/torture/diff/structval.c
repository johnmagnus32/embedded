/* structval.c — members of struct VALUES: (c ? p : q).m and ({...; s;}).m are temporaries; GCC takes the member of
 * the assigned object for (p = q).m and of the right operand for (x, s).m. Must match GCC. */
int printf(const char *, ...);
struct foo { char x, y, z[2]; } p, q, s;
int main(void) {
	int r = 1;
	(r ? p : q).z[1] = 7; (p = q).z[0] = 9; ((void)0, s).z[1] = 5; ({ r++; s; }).z[0] = 3;
	printf("%d %d %d %d %d\n", p.z[1], p.z[0], q.z[0], s.z[1], s.z[0]);
	return 0;
}
