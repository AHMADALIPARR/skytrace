% SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
% Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
% Replaces spec/tests/schema_contract.py and research/tests/test_hypotheses.py.
:- begin_tests(schema_contract).
:- use_module('../json_schema').

src(Rel, F) :- getenv('SKYTRACE_SOURCE_DIR', S), atomic_list_concat([S, '/', Rel], F).
schema(N, S) :- atomic_list_concat(['spec/schemas/', N, '.schema.json'], R), src(R, F), load_json(F, S), check_schema(S).
example(N, X) :- atomic_list_concat(['spec/examples/', N, '.json'], R), src(R, F), load_json(F, X).

test(all_seven_schemas_are_valid, forall(member(N, [artifact, event, observation, correlation, reconstruction, claim, verdict]))) :- schema(N, _).
test(monocular_unknown_range_is_accepted) :- schema(reconstruction, S), example('monocular-unknown-range', X), valid(S, X).
test(monocular_claim_is_accepted) :- schema(claim, S), example('indeterminate-claim', X), valid(S, X).
test(unidentifiable_range_cannot_emit_speed) :-
    schema(reconstruction, S), example('monocular-unknown-range', X0), put_dict(speed_m_s, X0, 100000.0, X),
    validate(S, X, V), V = [_|_], \+ valid(S, X).
test(missing_falsification_is_rejected) :-
    schema(claim, S), example('indeterminate-claim', X0), put_dict(falsifiers, X0, [], X), \+ valid(S, X).
test(unsigned_unknown_claim_field_is_rejected) :-
    schema(claim, S), example('indeterminate-claim', X0), put_dict(secret_new_field, X0, "silent", X),
    validate(S, X, V), memberchk(violation(_, additionalProperties, secret_new_field), V).
% Additional checks of the validator itself.
test(identifiable_range_may_emit_speed) :-
    schema(reconstruction, S), example('monocular-unknown-range', X0),
    put_dict(_{range_identifiable:true, speed_m_s:12.5, status:"solved"}, X0, X), valid(S, X).
test(bad_claim_id_pattern) :- schema(claim, S), example('indeterminate-claim', X0), put_dict(claim_id, X0, "CL-1", X), \+ valid(S, X).
test(duplicate_evidence_rejected) :-
    schema(claim, S), example('indeterminate-claim', X0), X0.evidence_sha256 = [H], put_dict(evidence_sha256, X0, [H, H], X), \+ valid(S, X).
test(uncertainty_oneof) :-
    schema(claim, S), example('indeterminate-claim', X0),
    put_dict(uncertainty, X0, _{value:1.0, unit:"m", lower_95:0.5, upper_95:2.0, method:"bootstrap"}, X1), valid(S, X1),
    put_dict(uncertainty, X0, _{value:1.0}, X2), \+ valid(S, X2).
test(integer_semantics, [nondet]) :- json_schema:json_type(3.0, "integer"), \+ json_schema:json_type(3.5, "integer").
test(date_time_format) :-
    json_schema:rfc3339("2026-02-29T00:00:00Z") -> fail ; json_schema:rfc3339("2024-02-29T23:59:60.5+05:30").
test(unsupported_keyword_is_refused, throws(error(skytrace_error('E_SCHEMA', unsupported_keyword(_, maxLength)), _))) :-
    check_schema(_{type:"string", maxLength:3}).
:- end_tests(schema_contract).

:- begin_tests(hypotheses_contract).
:- use_module('../json_schema').

doc(D) :- getenv('SKYTRACE_SOURCE_DIR', S), atom_concat(S, '/research/hypotheses.json', F), load_json(F, D).
test(unique_models) :- doc(D), maplist([M, I]>>get_dict(id, M, I), D.models, Ids), sort(Ids, U), length(Ids, 5), length(U, 5).
test(models_have_falsifiers) :- doc(D), forall(member(M, D.models), (M.predictions = [_|_], M.falsifiers = [_|_])).
test(telemetry_not_conflated, [nondet]) :-
    doc(D), S = D.telemetry_states, nth0(I, S, "obscured_confirmed"), nth0(J, S, "not_in_public_video"), I =\= J.
test(expected_cases) :- doc(D), forall(member(Id, ["GIMBAL", "DOW-UAP-PR113", "DOW-UAP-PR149"]), (member(C, D.case_targets), C.id == Id)).
test(no_gimbal_range_assumption) :-
    doc(D), once((member(G, D.case_targets), G.id == "GIMBAL")),
    G.range == "unknown_public_record", G.redaction_of_range == "not_established".
:- end_tests(hypotheses_contract).
