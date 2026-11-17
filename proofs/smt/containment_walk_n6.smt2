; SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
; Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
; Bounded lemma (n = 6 nodes) for the containment-forest check used by kernel/src/skytrace.c
; st_validate and src/graph/csr.c: following a functional parent relation p : node -> node | -1,
; a walk that has not reached the root marker -1 after n steps must have repeated a node (so the
; parent relation has a cycle), and a walk that reaches -1 cannot start on a cycle.
; Each check-sat below is expected to print `unsat`.
(set-logic QF_UFLIA)
(declare-fun p (Int) Int)
(define-fun node ((x Int)) Bool (and (>= x 0) (< x 6)))
(define-fun step ((x Int)) Int (ite (= x (- 1)) (- 1) (p x)))
(declare-const x0 Int)
(define-fun x1 () Int (step x0)) (define-fun x2 () Int (step x1)) (define-fun x3 () Int (step x2))
(define-fun x4 () Int (step x3)) (define-fun x5 () Int (step x4)) (define-fun x6 () Int (step x5))
(assert (node x0))
(assert (or (= (p 0) (- 1)) (node (p 0))))
(assert (or (= (p 1) (- 1)) (node (p 1))))
(assert (or (= (p 2) (- 1)) (node (p 2))))
(assert (or (= (p 3) (- 1)) (node (p 3))))
(assert (or (= (p 4) (- 1)) (node (p 4))))
(assert (or (= (p 5) (- 1)) (node (p 5))))

(echo "lemma 1: n steps without reaching -1 forces a repeated node (pigeonhole)")
(push)
(assert (not (= x6 (- 1))))
(assert (distinct x0 x1 x2 x3 x4 x5 x6))
(check-sat)
(pop)

(echo "lemma 2: a walk that returns to its start never reaches -1 within n steps")
(push)
(assert (or (= x1 x0) (= x2 x0) (= x3 x0) (= x4 x0) (= x5 x0) (= x6 x0)))
(assert (= x6 (- 1)))
(check-sat)
(pop)

(echo "lemma 3: -1 is absorbing, so reaching it within n steps means x6 = -1")
(push)
(assert (or (= x1 (- 1)) (= x2 (- 1)) (= x3 (- 1)) (= x4 (- 1)) (= x5 (- 1))))
(assert (not (= x6 (- 1))))
(check-sat)
(pop)
