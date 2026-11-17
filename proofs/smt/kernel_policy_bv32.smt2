; SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
; Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
; Independent SMT check (no SAW) of the case-activity policies over all 32-bit inputs.
; kernel: active = (s != 1) and (r != 1 or c != 1)        -- st_case_active / st_active
; strict: active = (s == 0) and (r != 1 or c != 1)        -- st_case_active_strict
; Each check-sat negates a lemma and is expected to print `unsat`.
(set-logic QF_BV)
(declare-const r (_ BitVec 32))
(declare-const c (_ BitVec 32))
(declare-const s (_ BitVec 32))
(define-fun one () (_ BitVec 32) #x00000001)
(define-fun zero () (_ BitVec 32) #x00000000)
(define-fun incomplete () Bool (or (not (= r one)) (not (= c one))))
(define-fun kernel () Bool (and (not (= s one)) incomplete))
(define-fun strict () Bool (and (= s zero) incomplete))

(echo "lemma 1: strict implies kernel")
(push) (assert (and strict (not kernel))) (check-sat) (pop)

(echo "lemma 2: policies differ exactly when s is neither 0 nor 1 and the case is incomplete")
(push) (assert (not (= (xor kernel strict) (and (not (= s zero)) (not (= s one)) incomplete)))) (check-sat) (pop)

(echo "lemma 3: a resolved case (s = 1) is never active under either policy")
(push) (assert (and (= s one) (or kernel strict))) (check-sat) (pop)

(echo "lemma 4: known range and corroboration (r = c = 1) close the case under both policies")
(push) (assert (and (= r one) (= c one) (or kernel strict))) (check-sat) (pop)

(echo "sanity: the mismatch region is non-empty (expected sat)")
(push) (assert (xor kernel strict)) (check-sat) (pop)
