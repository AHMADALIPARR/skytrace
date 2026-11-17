// SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
// Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
/*
 * garside_unl.c — Zero-Dependency Lambda-to-Unlambda Bracket-Abstraction Compiler
 * Emits an arbitrary-strand (n >= 2) Left-Greedy Garside Normalizer in pure Unlambda.
 *
 * Compile: cc -O2 -std=c11 -Wall -Wextra garside_unl.c -o garside_unl
 * Usage:   ./garside_unl [num_strands] > solver.unl
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ============================================================================
 * 1. Arena-Allocated Untyped Lambda / SKIBC AST
 * ============================================================================ */

typedef enum {
    NODE_VAR,
    NODE_LAM,
    NODE_APP,
    NODE_S,
    NODE_K,
    NODE_I,
    NODE_B,
    NODE_C,
    NODE_PRIM /* Unlambda primitive e.g. ".x" or "r" */
} NodeKind;

typedef struct Node {
    NodeKind kind;
    int var_id;          /* Used by NODE_VAR and NODE_LAM */
    const char *prim;    /* Used by NODE_PRIM */
    struct Node *lhs;    /* App func or Lam body */
    struct Node *rhs;    /* App arg */
} Node;

#define ARENA_CAP (1 << 22) /* 4M nodes (~128 MB max static/heap arena) */
static Node *arena = NULL;
static size_t arena_len = 0;

static Node *alloc_node(NodeKind kind) {
    if (!arena) {
        arena = (Node *)malloc(ARENA_CAP * sizeof(Node));
        if (!arena) {
            fprintf(stderr, "fatal: arena allocation failed\n");
            exit(1);
        }
    }
    if (arena_len >= ARENA_CAP) {
        fprintf(stderr, "fatal: arena capacity exceeded\n");
        exit(1);
    }
    Node *n = &arena[arena_len++];
    memset(n, 0, sizeof(Node));
    n->kind = kind;
    return n;
}

static Node *mk_var(int id) {
    Node *n = alloc_node(NODE_VAR);
    n->var_id = id;
    return n;
}

static Node *mk_lam(int id, Node *body) {
    Node *n = alloc_node(NODE_LAM);
    n->var_id = id;
    n->lhs = body;
    return n;
}

static Node *mk_app(Node *f, Node *x) {
    Node *n = alloc_node(NODE_APP);
    n->lhs = f;
    n->rhs = x;
    return n;
}

static Node *mk_comb(NodeKind k) {
    return alloc_node(k);
}

static Node *mk_prim(const char *s) {
    Node *n = alloc_node(NODE_PRIM);
    n->prim = s;
    return n;
}

/* Multi-argument application helper: appN(f, 3, a, b, c) */
static Node *app2(Node *f, Node *a, Node *b) {
    return mk_app(mk_app(f, a), b);
}

static Node *app3(Node *f, Node *a, Node *b, Node *c) {
    return mk_app(app2(f, a, b), c);
}

static int next_var_id = 1;
static int fresh_var(void) {
    return next_var_id++;
}

/* ============================================================================
 * 2. Turner Bracket Abstraction (Lambda -> SKIBC)
 * ============================================================================ */

static bool occurs_free(int v, const Node *e) {
    if (!e) return false;
    switch (e->kind) {
        case NODE_VAR:  return e->var_id == v;
        case NODE_LAM:  return (e->var_id != v) && occurs_free(v, e->lhs);
        case NODE_APP:  return occurs_free(v, e->lhs) || occurs_free(v, e->rhs);
        default:        return false;
    }
}

/* Eliminate variable `x` from combinator term `e` using Turner's rules */
static Node *abstract_var(int x, Node *e) {
    /* Rule 1: [x] E -> K E if x not free in E */
    if (!occurs_free(x, e)) {
        return mk_app(mk_comb(NODE_K), e);
    }
    /* Rule 2: [x] x -> I */
    if (e->kind == NODE_VAR && e->var_id == x) {
        return mk_comb(NODE_I);
    }
    /* Rule 3: Application [x] (E1 E2) */
    if (e->kind == NODE_APP) {
        Node *e1 = e->lhs;
        Node *e2 = e->rhs;
        bool f1 = occurs_free(x, e1);
        bool f2 = occurs_free(x, e2);

        /* Eta-reduction: [x] (E1 x) -> E1 if x not free in E1 */
        if (!f1 && e2->kind == NODE_VAR && e2->var_id == x) {
            return e1;
        }
        /* B combinator: [x] (E1 E2) -> B E1 ([x] E2) */
        if (!f1 && f2) {
            return app2(mk_comb(NODE_B), e1, abstract_var(x, e2));
        }
        /* C combinator: [x] (E1 E2) -> C ([x] E1) E2 */
        if (f1 && !f2) {
            return app2(mk_comb(NODE_C), abstract_var(x, e1), e2);
        }
        /* S combinator: [x] (E1 E2) -> S ([x] E1) ([x] E2) */
        return app2(mk_comb(NODE_S), abstract_var(x, e1), abstract_var(x, e2));
    }
    fprintf(stderr, "fatal: unexpected node in abstract_var\n");
    exit(1);
}

/* Bottom-up compilation of nested Lambdas into SKIBC */
static Node *compile_ski(Node *e) {
    if (!e) return NULL;
    switch (e->kind) {
        case NODE_APP:
            return mk_app(compile_ski(e->lhs), compile_ski(e->rhs));
        case NODE_LAM: {
            Node *body_ski = compile_ski(e->lhs);
            return abstract_var(e->var_id, body_ski);
        }
        default:
            return e;
    }
}

/* ============================================================================
 * 3. Pure Unlambda Stream Emitter (Expands B and C into S, K, I)
 * ============================================================================ */

static void emit_unlambda(FILE *out, const Node *e) {
    switch (e->kind) {
        case NODE_APP:
            fputc('`', out);
            emit_unlambda(out, e->lhs);
            emit_unlambda(out, e->rhs);
            break;
        case NODE_S:
            fputc('s', out);
            break;
        case NODE_K:
            fputc('k', out);
            break;
        case NODE_I:
            fputc('i', out);
            break;
        case NODE_B:
            /* B = S (K S) K */
            fputs("``s`ksk", out);
            break;
        case NODE_C:
            /* C = S (B B S) (K K) = S (S (K (S (K S) K)) S) (K K) */
            fputs("``s``s`k``s`ksks`kk", out);
            break;
        case NODE_PRIM:
            fputs(e->prim, out);
            break;
        default:
            fprintf(stderr, "fatal: uncompiled variable or lambda at emission\n");
            exit(1);
    }
}

/* ============================================================================
 * 4. Standard Lambda Calculus Library (Booleans, Church Numerals, Lists, Y)
 * ============================================================================ */

/* Y = \f. (\x. f (x x)) (\x. f (x x)) */
static Node *lc_Y(void) {
    int f = fresh_var(), x1 = fresh_var(), x2 = fresh_var();
    Node *half1 = mk_lam(x1, mk_app(mk_var(f), mk_app(mk_var(x1), mk_var(x1))));
    Node *half2 = mk_lam(x2, mk_app(mk_var(f), mk_app(mk_var(x2), mk_var(x2))));
    return mk_lam(f, mk_app(half1, half2));
}

static Node *lc_true(void)  { return mk_comb(NODE_K); }
static Node *lc_false(void) { return mk_app(mk_comb(NODE_K), mk_comb(NODE_I)); }

static Node *lc_and(void) {
    int p = fresh_var(), q = fresh_var();
    return mk_lam(p, mk_lam(q, app2(mk_var(p), mk_var(q), mk_var(p))));
}

static Node *lc_or(void) {
    int p = fresh_var(), q = fresh_var();
    return mk_lam(p, mk_lam(q, app2(mk_var(p), mk_var(p), mk_var(q))));
}

static Node *lc_not(void) {
    int p = fresh_var();
    return mk_lam(p, app2(mk_var(p), lc_false(), lc_true()));
}

/* Church Numeral n */
static Node *lc_nat(int n) {
    int f = fresh_var(), x = fresh_var();
    Node *body = mk_var(x);
    for (int i = 0; i < n; i++) {
        body = mk_app(mk_var(f), body);
    }
    return mk_lam(f, mk_lam(x, body));
}

static Node *lc_succ(void) {
    int n = fresh_var(), f = fresh_var(), x = fresh_var();
    return mk_lam(n, mk_lam(f, mk_lam(x,
        mk_app(mk_var(f), app2(mk_var(n), mk_var(f), mk_var(x))))));
}

static Node *lc_add(void) {
    int m = fresh_var(), n = fresh_var(), f = fresh_var(), x = fresh_var();
    return mk_lam(m, mk_lam(n, mk_lam(f, mk_lam(x,
        app2(mk_var(m), mk_var(f), app2(mk_var(n), mk_var(f), mk_var(x)))))));
}

static Node *lc_pair(void) {
    int a = fresh_var(), b = fresh_var(), f = fresh_var();
    return mk_lam(a, mk_lam(b, mk_lam(f, app2(mk_var(f), mk_var(a), mk_var(b)))));
}

static Node *lc_fst(void) {
    int p = fresh_var();
    return mk_lam(p, mk_app(mk_var(p), lc_true()));
}

static Node *lc_snd(void) {
    int p = fresh_var();
    return mk_lam(p, mk_app(mk_var(p), lc_false()));
}

static Node *lc_pred(void) {
    int n = fresh_var(), p = fresh_var();
    Node *step = mk_lam(p, app2(lc_pair(),
        mk_app(lc_snd(), mk_var(p)),
        mk_app(lc_succ(), mk_app(lc_snd(), mk_var(p)))));
    Node *init = app2(lc_pair(), lc_nat(0), lc_nat(0));
    return mk_lam(n, mk_app(lc_fst(), app2(mk_var(n), step, init)));
}

static Node *lc_sub(void) {
    int m = fresh_var(), n = fresh_var();
    return mk_lam(m, mk_lam(n, app2(mk_var(n), lc_pred(), mk_var(m))));
}

static Node *lc_is_zero(void) {
    int n = fresh_var();
    return mk_lam(n, app2(mk_var(n), mk_app(mk_comb(NODE_K), lc_false()), lc_true()));
}

/* m <= n  <=>  is_zero (sub m n) */
static Node *lc_leq(void) {
    int m = fresh_var(), n = fresh_var();
    return mk_lam(m, mk_lam(n, mk_app(lc_is_zero(), app2(lc_sub(), mk_var(m), mk_var(n)))));
}

/* m < n   <=>  not (leq n m) */
static Node *lc_lt(void) {
    int m = fresh_var(), n = fresh_var();
    return mk_lam(m, mk_lam(n, mk_app(lc_not(), app2(lc_leq(), mk_var(n), mk_var(m)))));
}

/* m == n  <=>  and (leq m n) (leq n m) */
static Node *lc_eq(void) {
    int m = fresh_var(), n = fresh_var();
    return mk_lam(m, mk_lam(n, app2(lc_and(),
        app2(lc_leq(), mk_var(m), mk_var(n)),
        app2(lc_leq(), mk_var(n), mk_var(m)))));
}

/* Church-encoded Lists: nil = \c.\n. n, cons = \h.\t.\c.\n. c h (t c n) */
static Node *lc_nil(void) {
    int c = fresh_var(), n = fresh_var();
    return mk_lam(c, mk_lam(n, mk_var(n)));
}

static Node *lc_cons(void) {
    int h = fresh_var(), t = fresh_var(), c = fresh_var(), n = fresh_var();
    return mk_lam(h, mk_lam(t, mk_lam(c, mk_lam(n,
        app2(mk_var(c), mk_var(h), app2(mk_var(t), mk_var(c), mk_var(n)))))));
}

static Node *lc_is_nil(void) {
    int l = fresh_var(), h = fresh_var(), t = fresh_var();
    return mk_lam(l, app2(mk_var(l),
        mk_lam(h, mk_lam(t, lc_false())),
        lc_true()));
}

static Node *lc_head(void) {
    int l = fresh_var(), h = fresh_var(), t = fresh_var();
    return mk_lam(l, app2(mk_var(l),
        mk_lam(h, mk_lam(t, mk_var(h))),
        lc_nat(0)));
}

/* O(1) tail via pair-trick */
static Node *lc_tail(void) {
    int l = fresh_var(), h = fresh_var(), t = fresh_var(), p = fresh_var();
    Node *step = mk_lam(h, mk_lam(p,
        app2(lc_pair(),
            mk_app(lc_snd(), mk_var(p)),
            app2(lc_cons(), mk_var(h), mk_app(lc_snd(), mk_var(p))))));
    Node *init = app2(lc_pair(), lc_nil(), lc_nil());
    (void)t;
    return mk_lam(l, mk_app(lc_fst(), app2(mk_var(l), step, init)));
}

static Node *lc_append(void) {
    int xs = fresh_var(), ys = fresh_var();
    return mk_lam(xs, mk_lam(ys, app2(mk_var(xs), lc_cons(), mk_var(ys))));
}

static Node *lc_length(void) {
    int xs = fresh_var(), h = fresh_var(), acc = fresh_var();
    return mk_lam(xs, app2(mk_var(xs), mk_lam(h, mk_lam(acc, mk_app(lc_succ(), mk_var(acc)))), lc_nat(0)));
}

/* ============================================================================
 * 5. Arbitrary-Strand Symmetric Group S_n & Garside Poset Operations
 * ============================================================================ */

/* nth(xs, idx): 0-indexed list lookup */
static Node *lc_nth(void) {
    int xs = fresh_var(), idx = fresh_var();
    return mk_lam(xs, mk_lam(idx,
        mk_app(lc_head(), app2(mk_var(idx), lc_tail(), mk_var(xs)))));
}

/* set_nth(xs, idx, val): returns new list with xs[idx] = val */
static Node *lc_set_nth(void) {
    int rec = fresh_var(), xs = fresh_var(), idx = fresh_var(), val = fresh_var();
    Node *body = mk_lam(xs, mk_lam(idx, mk_lam(val,
        app2(mk_app(lc_is_zero(), mk_var(idx)),
            app2(lc_cons(), mk_var(val), mk_app(lc_tail(), mk_var(xs))),
            app2(lc_cons(),
                mk_app(lc_head(), mk_var(xs)),
                app3(mk_var(rec),
                    mk_app(lc_tail(), mk_var(xs)),
                    mk_app(lc_pred(), mk_var(idx)),
                    mk_var(val)))))));
    return mk_app(lc_Y(), mk_lam(rec, body));
}

/* Build concrete list [0, 1, ..., n-1] (identity simple braid) */
static Node *perm_id(int n) {
    Node *lst = lc_nil();
    for (int i = n - 1; i >= 0; i--) {
        lst = app2(lc_cons(), lc_nat(i), lst);
    }
    return lst;
}

/* Build concrete list [n-1, ..., 1, 0] (Garside fundamental half-twist Delta_n) */
static Node *perm_delta(int n) {
    Node *lst = lc_nil();
    for (int i = 0; i < n; i++) {
        lst = app2(lc_cons(), lc_nat(i), lst);
    }
    return lst;
}

/* Simple transposition sigma_i = (i, i+1) on n strands */
static Node *perm_sigma(int n, int i) {
    Node *lst = lc_nil();
    for (int k = n - 1; k >= 0; k--) {
        int v = (k == i) ? (i + 1) : ((k == i + 1) ? i : k);
        lst = app2(lc_cons(), lc_nat(v), lst);
    }
    return lst;
}

/* Permutation composition: (pi1 o pi2)[k] = pi1[pi2[k]] */
static Node *lc_perm_mul(int n) {
    int p1 = fresh_var(), p2 = fresh_var();
    Node *res = lc_nil();
    for (int k = n - 1; k >= 0; k--) {
        Node *idx = app2(lc_nth(), mk_var(p2), lc_nat(k));
        Node *val = app2(lc_nth(), mk_var(p1), idx);
        res = app2(lc_cons(), val, res);
    }
    return mk_lam(p1, mk_lam(p2, res));
}

/* Permutation inverse pi^{-1} */
static Node *lc_perm_inv(int n) {
    int p = fresh_var();
    Node *res = perm_id(n);
    for (int k = 0; k < n; k++) {
        Node *pos = app2(lc_nth(), mk_var(p), lc_nat(k));
        res = app3(lc_set_nth(), res, pos, lc_nat(k));
    }
    return mk_lam(p, res);
}

/* Coxeter inversion count l(pi) = |{(i,j) : i < j and pi[i] > pi[j]}| */
static Node *lc_coxeter_len(int n) {
    int p = fresh_var();
    Node *acc = lc_nat(0);
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            Node *pi = app2(lc_nth(), mk_var(p), lc_nat(i));
            Node *pj = app2(lc_nth(), mk_var(p), lc_nat(j));
            Node *inv = app2(app2(lc_lt(), pj, pi), lc_nat(1), lc_nat(0));
            acc = app2(lc_add(), acc, inv);
        }
    }
    return mk_lam(p, acc);
}

/* Right descent test: sigma_i <=_R pi  <=>  pi[i] > pi[i+1] */
static Node *lc_right_descent(int i) {
    int p = fresh_var();
    Node *pi = app2(lc_nth(), mk_var(p), lc_nat(i));
    Node *pi1 = app2(lc_nth(), mk_var(p), lc_nat(i + 1));
    return mk_lam(p, app2(lc_lt(), pi1, pi));
}

/* Left descent test: sigma_i <=_L pi  <=>  pi^{-1}[i] > pi^{-1}[i+1] */
static Node *lc_left_descent(int n, int i) {
    int p = fresh_var();
    Node *pinv = mk_app(lc_perm_inv(n), mk_var(p));
    Node *qi = app2(lc_nth(), pinv, lc_nat(i));
    Node *qi1 = app2(lc_nth(), pinv, lc_nat(i + 1));
    return mk_lam(p, app2(lc_lt(), qi1, qi));
}

/* ============================================================================
 * 6. Left-Greedy Garside Slide & Full Word Normalization (w -> (p, [A_1..A_k]))
 * ============================================================================ */

/*
 * Single left-greedy step between adjacent simple factors (A, B) in D_n:
 * For i = 0 .. n-2:
 *   If sigma_i <=_L B  AND  NOT (sigma_i <=_R A):
 *     A' = A * sigma_i
 *     B' = sigma_i * B   (since sigma_i^2 = 1 in S_n)
 */
static Node *lc_greedy_pair_step(int n) {
    int pair_ab = fresh_var();
    Node *cur = mk_var(pair_ab);

    for (int i = 0; i < n - 1; i++) {
        int p_var = fresh_var();
        Node *a = mk_app(lc_fst(), mk_var(p_var));
        Node *b = mk_app(lc_snd(), mk_var(p_var));

        Node *cond = app2(lc_and(),
            mk_app(lc_left_descent(n, i), b),
            mk_app(lc_not(), mk_app(lc_right_descent(i), a)));

        Node *si = perm_sigma(n, i);
        Node *a_next = app2(lc_perm_mul(n), a, si);
        Node *b_next = app2(lc_perm_mul(n), si, b);
        Node *swapped = app2(lc_pair(), a_next, b_next);

        Node *step_fn = mk_lam(p_var, app2(cond, swapped, mk_var(p_var)));
        cur = mk_app(step_fn, cur);
    }
    return mk_lam(pair_ab, cur);
}

/*
 * Convert raw generator token (idx, sign_is_pos) into a Garside pair (inf_delta, simple_factor):
 * - Positive sigma_i:       (Z_ZERO, sigma_i)
 * - Negative sigma_i^{-1}:  (Z_NEG1, Delta_n * sigma_i)  [since Delta_n = sigma_i * (sigma_i * Delta_n)]
 */
static Node *lc_gen_to_garside(int n) {
    int idx = fresh_var(), is_pos = fresh_var();
    Node *si = perm_id(n);
    /* Dispatch idx in 0..n-2 to concrete sigma_idx */
    for (int i = n - 2; i >= 0; i--) {
        si = app2(app2(lc_eq(), mk_var(idx), lc_nat(i)), perm_sigma(n, i), si);
    }
    Node *pos_pair = app2(lc_pair(), app2(lc_pair(), lc_nat(0), lc_nat(0)), si);
    Node *neg_factor = app2(lc_perm_mul(n), perm_delta(n), si);
    Node *neg_pair = app2(lc_pair(), app2(lc_pair(), lc_nat(0), lc_nat(1)), neg_factor);

    return mk_lam(idx, mk_lam(is_pos, app2(mk_var(is_pos), pos_pair, neg_pair)));
}

/*
 * Conjugate simple factor A by Delta_n^k (k mod 2):
 * Delta_n^{-1} * A * Delta_n maps sigma_i -> sigma_{n-2-i}, i.e., reverses permutation complement.
 */
static Node *lc_conj_delta(int n) {
    int a = fresh_var();
    Node *d = perm_delta(n);
    return mk_lam(a, app2(lc_perm_mul(n), d, app2(lc_perm_mul(n), mk_var(a), d)));
}

/*
 * Bubble a newly appended simple factor B leftwards through canonical list [A_1, ..., A_k]
 * using left-greedy pair slides.
 */
static Node *lc_insert_factor(int n) {
    int rec = fresh_var(), factors = fresh_var(), b = fresh_var();
    Node *body = mk_lam(factors, mk_lam(b,
        app2(mk_app(lc_is_nil(), mk_var(factors)),
            app2(lc_cons(), mk_var(b), lc_nil()),
            /* Slide (head(factors), b) -> (a', b'), recurse on tail */
            mk_app(
                mk_lam(1000001, /* temporary binding via beta-redex */
                    app2(lc_cons(),
                        mk_app(lc_fst(), mk_var(1000001)),
                        app2(mk_var(rec),
                            mk_app(lc_tail(), mk_var(factors)),
                            mk_app(lc_snd(), mk_var(1000001))))),
                mk_app(lc_greedy_pair_step(n),
                    app2(lc_pair(), mk_app(lc_head(), mk_var(factors)), mk_var(b)))))));
    return mk_app(lc_Y(), mk_lam(rec, body));
}

/*
 * Full Left-Greedy Garside Normalizer on a braid word list [(idx, is_pos), ...]:
 * Accumulates state = (Z_inf = (p_pos, p_neg), canonical_factors = [A_1, ..., A_k]).
 */
static Node *lc_garside_normalize(int n) {
    int word = fresh_var(), tok = fresh_var(), state = fresh_var();

    /* Step function folding each generator token into (Z_inf, factors) */
    int g_pair = fresh_var();
    Node *tok_idx = mk_app(lc_fst(), mk_var(tok));
    Node *tok_pos = mk_app(lc_snd(), mk_var(tok));

    Node *cur_inf = mk_app(lc_fst(), mk_var(state));
    Node *cur_facs = mk_app(lc_snd(), mk_var(state));

    Node *new_inf_pos = app2(lc_add(),
        mk_app(lc_fst(), cur_inf),
        mk_app(lc_fst(), mk_app(lc_fst(), mk_var(g_pair))));
    Node *new_inf_neg = app2(lc_add(),
        mk_app(lc_snd(), cur_inf),
        mk_app(lc_snd(), mk_app(lc_fst(), mk_var(g_pair))));

    /* If tok_pos is false (negative generator), conjugate existing factors by Delta_n */
    Node *shifted_facs = app2(tok_pos,
        cur_facs,
        app2(cur_facs,
            mk_lam(1000002, mk_lam(1000003,
                app2(lc_cons(), mk_app(lc_conj_delta(n), mk_var(1000002)), mk_var(1000003)))),
            lc_nil()));

    Node *next_facs = app2(lc_insert_factor(n),
        shifted_facs,
        mk_app(lc_snd(), mk_var(g_pair)));

    Node *step_body = mk_app(
        mk_lam(g_pair,
            app2(lc_pair(), app2(lc_pair(), new_inf_pos, new_inf_neg), next_facs)),
        app2(lc_gen_to_garside(n), tok_idx, tok_pos));

    Node *fold_step = mk_lam(tok, mk_lam(state, step_body));
    Node *init_state = app2(lc_pair(), app2(lc_pair(), lc_nat(0), lc_nat(0)), lc_nil());

    /* Reverse word or fold right-to-left so left-most generators accumulate first */
    return mk_lam(word, app2(mk_var(word), fold_step, init_state));
}

/* ============================================================================
 * 7. Unary Output Printer & Sample Trajectory Harness
 * ============================================================================ */

/* Emit n '*' characters followed by a newline via Unlambda primitives */
static Node *lc_print_unary(void) {
    int n = fresh_var(), x = fresh_var();
    Node *star_fn = mk_lam(x, mk_app(mk_prim(".*"), mk_var(x)));
    return mk_lam(n, mk_app(mk_prim("r"), app2(mk_var(n), star_fn, lc_true())));
}

int main(int argc, char **argv) {
    int n = 3; /* Default: 3-strand braid group B_3 */
    if (argc > 1) {
        n = atoi(argv[1]);
        if (n < 2 || n > 12) {
            fprintf(stderr, "error: num_strands must be between 2 and 12\n");
            return 1;
        }
    }

    /* Sample braid word: sigma_0 * sigma_1 * sigma_0^{-1} in B_n */
    Node *tok1 = app2(lc_pair(), lc_nat(0), lc_true());
    Node *tok2 = app2(lc_pair(), lc_nat(1 % (n - 1)), lc_true());
    Node *tok3 = app2(lc_pair(), lc_nat(0), lc_false());
    Node *sample_word = app2(lc_cons(), tok1,
                        app2(lc_cons(), tok2,
                        app2(lc_cons(), tok3, lc_nil())));

    /* Normalize word -> (Z_inf, canonical_factors) */
    int norm_res = fresh_var();
    Node *normalizer = lc_garside_normalize(n);
    Node *printer = lc_print_unary();

    /* Compute canonical length k and total Coxeter factor sum */
    int f_var = fresh_var(), acc_var = fresh_var();
    Node *factor_sum_fn = mk_lam(f_var, mk_lam(acc_var,
        app2(lc_add(), mk_app(lc_coxeter_len(n), mk_var(f_var)), mk_var(acc_var))));

    Node *facs = mk_app(lc_snd(), mk_var(norm_res));
    Node *inf_neg = mk_app(lc_snd(), mk_app(lc_fst(), mk_var(norm_res)));
    Node *canon_len = mk_app(lc_length(), facs);
    Node *cox_sum = app2(facs, factor_sum_fn, lc_nat(0));

    /* Sequence stdout emission: inf_neg ticks, canonical_len ticks, cox_sum ticks */
    int d1 = fresh_var(), d2 = fresh_var();
    Node *program = mk_app(
        mk_lam(norm_res,
            mk_app(
                mk_lam(d1,
                    mk_app(
                        mk_lam(d2, mk_app(printer, cox_sum)),
                        mk_app(printer, canon_len))),
                mk_app(printer, inf_neg))),
        mk_app(normalizer, sample_word));

    Node *compiled = compile_ski(program);
    emit_unlambda(stdout, compiled);
    fputc('\n', stdout);

    fprintf(stderr, "Compiled B_%d Garside normalizer (%zu AST nodes)\n", n, arena_len);
    free(arena);
    return 0;
}

