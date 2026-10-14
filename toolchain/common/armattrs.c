/*
 * common/armattrs.c — see armattrs.h. Reading only: nothing here writes a section.
 */
#include <string.h>
#include "armattrs.h"

/* Unsigned LEB128. Advances *p, never past end; returns 0 on a truncated value. */
static int uleb(const unsigned char **p, const unsigned char *end, unsigned long *out) {
    unsigned long v = 0; int shift = 0;
    while (*p < end) {
        unsigned char b = *(*p)++;
        if (shift < 64) v |= (unsigned long)(b & 0x7f) << shift;
        shift += 7;
        if (!(b & 0x80)) { *out = v; return 1; }
    }
    return 0;
}

static unsigned long rd32(const unsigned char *p) {   /* little-endian: ARM objects only */
    return (unsigned long)p[0] | ((unsigned long)p[1] << 8) | ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}

/* Which tags carry a NUL-terminated string rather than a LEB128 integer (IHI 0045 7.2: tags 4, 5 and 67, plus
 * the odd-numbered ones above 32 by the encoding rule). Getting this wrong desynchronises the whole stream,
 * so a tag whose kind we cannot know stops the parse rather than guessing. */
static int tag_is_string(unsigned long t) { return t == 4 || t == 5 || t == 67 || (t > 32 && (t & 1)); }

int armattrs_parse(const unsigned char *data, size_t n, ArmAttrs *out) {
    memset(out, 0, sizeof *out);
    if (!data || n < 5 || data[0] != 'A') return 0;

    const unsigned char *p = data + 1, *end = data + n;
    while (end - p >= 4) {
        unsigned long sublen = rd32(p);
        if (sublen < 5 || sublen > (unsigned long)(end - p)) return out->have;   /* truncated: keep what we have */
        const unsigned char *sub = p + 4, *subend = p + sublen;
        size_t namelen = 0;
        while (sub + namelen < subend && sub[namelen]) namelen++;
        if (sub + namelen >= subend) return out->have;                            /* unterminated vendor name */
        int aeabi = namelen == 5 && !memcmp(sub, "aeabi", 5);
        const unsigned char *q = sub + namelen + 1;
        if (!aeabi) { p = subend; continue; }                                     /* another vendor: not ours */

        while (subend - q >= 5) {            /* sub-subsections: <u8 scope><u32 length> */
            unsigned char scope = *q;
            unsigned long len = rd32(q + 1);
            if (len < 5 || len > (unsigned long)(subend - q)) return out->have;
            const unsigned char *t = q + 5, *tend = q + len;
            if (scope == 1) {                /* file scope — the only one GCC or our as emits */
                out->have = 1;
                while (t < tend) {
                    unsigned long tag, v;
                    if (!uleb(&t, tend, &tag)) break;
                    if (tag_is_string(tag)) {
                        while (t < tend && *t) t++;            /* skip the string */
                        if (t < tend) t++;                      /* and its NUL */
                        if (tag < ARM_ATTR_MAX) out->set[tag] = 1;
                        if (tag == Tag_conformance) continue;   /* (string-valued, nothing numeric to keep) */
                        continue;
                    }
                    if (!uleb(&t, tend, &v)) break;
                    if (tag < ARM_ATTR_MAX) { out->set[tag] = 1; out->val[tag] = (long)v; }
                }
            }
            q = tend;
        }
        p = subend;
    }
    return out->have;
}

int armattrs_declares_fp_abi(const ArmAttrs *a) {
    return a->have && a->set[Tag_ABI_FP_number_model] && a->val[Tag_ABI_FP_number_model] != 0;
}

int armattrs_vfp_args(const ArmAttrs *a) {
    return a->have && a->set[Tag_ABI_VFP_args] ? (int)a->val[Tag_ABI_VFP_args] : 0;
}
