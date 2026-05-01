/*
 * eval.h — Expression evaluator (shared by REPL and DAP)
 */
#ifndef EVAL_H
#define EVAL_H

#include <stdint.h>

struct dbg_client;

struct eval_result {
    int valid;
    uint32_t addr;      /* memory address of the value */
    uint32_t val;       /* the value (for <= 4 byte types) */
    uint32_t type_die;  /* DWARF type DIE */
    int is_register;    /* value is a register, not memory */
};

/*
 * Evaluate a C-like expression (variable, .member, ->field, [index], *deref).
 * Reads memory via the debug client. Uses provided registers (avoids round-trip).
 */
void eval_expr_with_regs(struct dbg_client *c, const char *expr,
                         uint32_t regs[16], struct eval_result *out);

/* Convenience: reads registers from client, then evaluates. */
void eval_expr(struct dbg_client *c, const char *expr, struct eval_result *out);

/*
 * Format an eval result as a string. Returns number of chars written.
 */
int eval_format(struct dbg_client *c, struct eval_result *r, const char *expr,
                char *buf, int bufsize);

#endif
