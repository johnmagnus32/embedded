#!/usr/bin/env python3
"""builtin_diff.py — differential check of the builtins the parser lowers. Each case is a C expression and its
exact expected value, computed here in Python (exact integers + IEEE semantics — no host compiler is trusted as the
oracle: the host's GCC 7 gets `__builtin_mul_overflow(LLONG_MIN, 1, &ull)` wrong). Our cc compiles the cases for
ARM and qemu runs them; every value must match.

  __builtin_{add,sub,mul}_overflow   all 512 (operand, operand, result) combinations of the 8 integer types x 3 ops,
                                     each over 3x3 boundary/random values (13,824 cases)
  isnan/isinf/isfinite, isgreater/... float + double over 0, -0, finite, huge, tiny, +-inf, NaN (all pairs)
  abs/llabs, isdigit, *_overflow_p

usage: builtin_diff.py        needs qemu-system-arm; exit 0 = all match
"""
import itertools, math, os, random, struct, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__)); T = os.path.abspath(os.path.join(HERE, "../.."))
CC, AS, LD = T + "/cc/build/cc", T + "/as/build/as", T + "/ld/build/ld"
RT, CRT = T + "/rt/build/libosrt.a", T + "/cc/tests/crt.s"
TY = {'sc': ('signed char', 8), 'uc': ('unsigned char', 8), 'ss': ('short', 16), 'us': ('unsigned short', 16),
      'si': ('int', 32), 'ui': ('unsigned', 32), 'sl': ('long long', 64), 'ul': ('unsigned long long', 64)}
def rng(k): n = TY[k][1]; return (-2**(n - 1), 2**(n - 1) - 1) if k[0] == 's' else (0, 2**n - 1)
def wrap(k, v):                                      # v converted to type k (two's complement wrap)
    n = TY[k][1]; v %= 2**n
    return v - 2**n if k[0] == 's' and v >= 2**(n - 1) else v
def lit(k, v):
    n = TY[k][0]
    if v == -2**63: return '(%s)(-9223372036854775807LL-1)' % n
    return '(%s)%dLL' % (n, v) if k[0] == 's' else '(%s)%dULL' % (n, v)
U64 = 2**64

def overflow_cases():
    rnd = random.Random(11)
    def vals(k):
        lo, hi = rng(k)
        v = {0, 1, hi, lo, hi // 2, hi - 1, lo + 1 if lo else 2, hi // 3, rnd.randint(lo, hi), rnd.randint(lo, hi)}
        if lo < 0: v |= {-1, lo // 2}
        return sorted(v)
    out = []
    for op in ('add', 'sub', 'mul'):
        for a, b, r in itertools.product(TY, TY, TY):
            for x in rnd.sample(vals(a), 3):
                for y in rnd.sample(vals(b), 3):
                    exact = x + y if op == 'add' else x - y if op == 'sub' else x * y
                    lo, hi = rng(r); o = not lo <= exact <= hi
                    want = (o << 63) ^ (wrap(r, exact) % U64)   # overflow flag in bit 63, over the result's bits
                    e = ('({ %s va = %s; %s vb = %s; %s res; int o = __builtin_%s_overflow(va, vb, &res); '
                         '(unsigned long long)o << 63 ^ (unsigned long long)res; })') % (TY[a][0], lit(a, x), TY[b][0], lit(b, y), TY[r][0], op)
                    out.append((e, want))
    return out

def other_cases():
    F = [('0.0', 0.0), ('-0.0', -0.0), ('1.0', 1.0), ('-1.5', -1.5), ('1e30', 1e30), ('1e-30', 1e-30),
         ('(1.0/0.0)', math.inf), ('(-1.0/0.0)', -math.inf), ('(0.0/0.0)', math.nan)]
    as_float = lambda v: struct.unpack('f', struct.pack('f', v))[0]
    rel = {'isgreater': lambda v, w: v > w, 'isgreaterequal': lambda v, w: v >= w, 'isless': lambda v, w: v < w,
           'islessequal': lambda v, w: v <= w, 'islessgreater': lambda v, w: v < w or v > w}
    out = []
    for ty in ('float', 'double'):
        cv = as_float if ty == 'float' else float
        for fn, f in (('isnan', math.isnan), ('isinf', math.isinf), ('isfinite', math.isfinite)):
            out += [('({ %s v = %s; __builtin_%s(v) != 0; })' % (ty, s, fn), int(f(cv(v)))) for s, v in F]
        for fn in list(rel) + ['isunordered']:
            for (s, v), (t, w) in itertools.product(F, F):
                v, w = cv(v), cv(w); nan = math.isnan(v) or math.isnan(w)
                want = nan if fn == 'isunordered' else (not nan and rel[fn](v, w))
                out.append(('({ %s v = %s, w = %s; __builtin_%s(v, w) != 0; })' % (ty, s, t, fn), int(want)))
    out += [('(unsigned long long)__builtin_abs(%d)' % v, abs(v) % U64) for v in (0, 5, -5, 2147483647, -2147483647)]
    out += [('(unsigned long long)__builtin_llabs(%dLL)' % v, abs(v) % U64) for v in (0, -5, 2**63 - 1, -(2**63 - 1))]
    out += [('__builtin_isdigit(%d)' % c, int(48 <= c <= 57)) for c in (48, 57, 97, 32, 47, 58, 200)]
    for op in ('add', 'sub', 'mul'):
        for x, y, k in ((100, 100, 'sc'), (-100, -100, 'sc'), (65535, 1, 'us'), (0x7fffffff, 2, 'si'), (-1, 1, 'ui'),
                        (5000000000, 5000000000, 'sl'), (-3, -3, 'ul')):
            exact = x + y if op == 'add' else x - y if op == 'sub' else x * y
            lo, hi = rng(k)
            out.append(('__builtin_%s_overflow_p(%dLL, %dLL, (%s)0)' % (op, x, y, TY[k][0]), int(not lo <= exact <= hi)))
    return out

def main():
    cases = overflow_cases() + other_cases()
    w = tempfile.mkdtemp(prefix="builtin_diff.")
    subprocess.check_call([AS, "-o", w + "/crt.o", CRT])
    bad = 0
    for part in range(0, len(cases), 2000):                 # one program per 2000 cases (bounded compile size)
        chunk = cases[part:part + 2000]
        b = os.path.join(w, "p%d" % part)
        with open(b + ".c", "w") as f:                      # returns 1 + the index of the first mismatch (0 = all match)
            f.write('static int ok(unsigned long long got, unsigned long long want) { return got == want; }\n'
                    'int main(void) {\n')
            f.writelines('if (!ok((unsigned long long)(%s), %dULL)) return %d;\n' % (e, v, 1 + i % 250) for i, (e, v) in enumerate(chunk))
            f.write('return 0; }\n')
        for cmd in ([CC, "-o", b + ".s", b + ".c"], [AS, "-o", b + ".o", b + ".s"],
                    [LD, "-Ttext", "0x40000000", "-o", b + ".elf", w + "/crt.o", b + ".o", RT]):
            subprocess.check_call(cmd)
        rc = subprocess.call(["timeout", "60", "qemu-system-arm", "-M", "virt", "-cpu", "cortex-a7", "-nographic",
                              "-semihosting", "-net", "none", "-kernel", b + ".elf"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if rc:
            print("FAIL builtin_diff: %s.c, first mismatch at a case i with i %% 250 == %d (e.g. %s == %d)" % (b, rc - 1, *chunk[rc - 1]))
            bad += 1
    if bad: sys.exit(1)
    print("PASS builtin_diff: %d cases match exact arithmetic (overflow x all type triples, FP classify, abs, isdigit, _p)" % len(cases))

main()
