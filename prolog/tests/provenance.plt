:- begin_tests(three_valued).
:- use_module('../skytrace').

test(kleene_and_table) :-
    findall(A-B-V, (tv_value(A), tv_value(B), tv_and(A, B, V)), L),
    L == [known_true-known_true-known_true, known_true-known_false-known_false, known_true-unknown-unknown,
          known_false-known_true-known_false, known_false-known_false-known_false, known_false-unknown-known_false,
          unknown-known_true-unknown, unknown-known_false-known_false, unknown-unknown-unknown].
test(kleene_or_table) :-
    findall(A-B-V, (tv_value(A), tv_value(B), tv_or(A, B, V)), L),
    L == [known_true-known_true-known_true, known_true-known_false-known_true, known_true-unknown-known_true,
          known_false-known_true-known_true, known_false-known_false-known_false, known_false-unknown-unknown,
          unknown-known_true-known_true, unknown-known_false-unknown, unknown-unknown-unknown].
test(de_morgan, forall((tv_value(A), tv_value(B)))) :-
    tv_and(A, B, AB), tv_not(AB, L), tv_not(A, NA), tv_not(B, NB), tv_or(NA, NB, R), L == R.
test(unknown_is_not_false) :- tv_not(unknown, X), X == unknown.
test(deterministic, [nondet]) :- forall((tv_value(A), tv_value(B)), (aggregate_all(count, tv_and(A, B, _), 1))).
:- end_tests(three_valued).

:- begin_tests(kernel_policy_parity).
:- use_module('../skytrace').
:- use_module(library(http/json)).

reference(D) :-
    getenv('SKYTRACE_SOURCE_DIR', Src),
    atom_concat(Src, '/fixtures/reference/numerical_analyze.synthetic-batch.out.json', F),
    setup_call_cleanup(open(F, read, In), json_read_dict(In, D, []), close(In)).
combos(L) :- findall(r(R, C, S), (member(S, [-1, 0, 1]), member(R, [-1, 0, 1]), member(C, [-1, 0, 1])), L).

% research/numerical/analyze.py ternary closure, recomputed from kernel_active/4 and strict_active/4.
test(ternary_closure_matches_python_reference) :-
    reference(D), T = D.ternary_closure,
    combos(L), length(L, N),
    aggregate_all(count, (member(r(R, C, S), L), kernel_active(R, C, S, 1)), K),
    aggregate_all(count, (member(r(R, C, S), L), strict_active(R, C, S, 1)), St),
    findall(_{corroborated:C, range:R, resolution:S},
            (member(r(R, C, S), L), kernel_active(R, C, S, A), strict_active(R, C, S, B), A =\= B), Mis),
    length(Mis, NM), maplist([X, Y]>>dict_pairs(X, _, Y), Mis, MP), maplist([X, Y]>>dict_pairs(X, _, Y), T.mismatches, RP),
    N =:= T.ternary_combinations, K =:= T.kernel_active_combinations, St =:= T.strict_active_combinations,
    NM =:= T.policy_mismatch_combinations, MP == RP.
test(synthetic_batch_closure_matches_reference) :-
    reference(D), G = D.graph.closure,
    getenv('SKYTRACE_SOURCE_DIR', Src), atom_concat(Src, '/fixtures/archives/synthetic-batch.json', F),
    load_batch(F),
    aggregate_all(count, active_case(_), K), aggregate_all(count, active_case_strict(_), S),
    aggregate_all(count, (case_codes(_, -1, _, _)), UR), aggregate_all(count, (case_codes(_, _, -1, _)), UC),
    K =:= G.kernel_active, S =:= G.strict_active, UR =:= G.unknown_range, UC =:= G.unknown_corroboration.
test(containment_topology) :-
    getenv('SKYTRACE_SOURCE_DIR', Src), atom_concat(Src, '/fixtures/archives/synthetic-batch.json', F),
    load_batch(F),
    contains_path('AGENCY-DOW', 'DOW-UAP-DEMO-001'), \+ contains_path('DOW-UAP-DEMO-001', _),
    \+ derived_from(_, _).
:- end_tests(kernel_policy_parity).

:- begin_tests(provenance).
:- use_module('../skytrace').

h(C, H) :- length(L, 64), maplist(=(C), L), string_chars(H, L).
base(Id, Extra, R) :- R0 = _{agency:"DOW", canonical_id:Id, release_epoch:"2026-05-08", mime:"video/mp4", source:"pursue"}, put_dict(Extra, R0, R).

% CASE-A: two calibrated observations from distinct recordings, plus a re-encode of OBS-1.
% CASE-B: an observation and its own derived copy (duplicated origin) with a corroborated:true flag.
% CASE-C: missing calibration and missing recording origin.
fixture(_{records:Rs, claims:Cs}) :-
    h(a, HA), h(b, HB), h(c, HC), h(d, HD), h(e, HE), h(f, HF),
    base("CASE-A", _{mime:"application/x.skytrace-case"}, CA),
    base("OBS-1", _{parent_id:"CASE-A", evidence_sha256:HA, recording_origin_id:"FLIR-1", calibrated:true, timestamp_uncertainty_ms:40, has_range_telemetry:false, sensor_modality:"ir_video"}, O1),
    base("OBS-2", _{parent_id:"CASE-A", evidence_sha256:HB, recording_origin_id:"RADAR-7", calibrated:true, timestamp_uncertainty_ms:5, has_range_telemetry:true, sensor_modality:"radar"}, O2),
    base("OBS-3", _{parent_id:"CASE-A", evidence_sha256:HC, derived_from:["OBS-1"], calibrated:true, timestamp_uncertainty_ms:40}, O3),
    base("CASE-B", _{mime:"application/x.skytrace-case", corroborated:true}, CB),
    base("OBS-4", _{parent_id:"CASE-B", evidence_sha256:HD, recording_origin_id:"CAM-9", calibrated:true, has_range_telemetry:false}, O4),
    base("OBS-5", _{parent_id:"CASE-B", evidence_sha256:HE, derived_from:["OBS-4"], calibrated:true, has_range_telemetry:false}, O5),
    base("CASE-C", _{mime:"application/x.skytrace-case", corroborated:false}, CC),
    base("OBS-6", _{parent_id:"CASE-C", evidence_sha256:HF, has_range_telemetry:true, timestamp_uncertainty_ms:null}, O6),
    Rs = [CA, O1, O2, O3, CB, O4, O5, CC, O6],
    Cs = [_{claim_id:"CL-1", subject:"OBS-1", attribute:"shape", value:"sphere", evidence:"OBS-1"},
          _{claim_id:"CL-2", subject:"OBS-1", attribute:"shape", value:"cylinder", evidence:"OBS-3"},
          _{claim_id:"CL-3", subject:"OBS-2", attribute:"shape", value:"sphere", evidence:"OBS-2"},
          _{claim_id:"CL-4", subject:"OBS-1", attribute:"color", value:"white", evidence:"OBS-1"}].
load :- fixture(D), load_batch_dict(D).

test(source_record) :- load, source_record('OBS-1', pursue, 'DOW').
test(dag_and_forest_are_distinct) :-
    load, derived_from('OBS-3', 'OBS-1'), \+ contained_in('OBS-3', 'OBS-1'),
    contained_in('OBS-3', 'CASE-A'), \+ derived_from('OBS-3', 'CASE-A').
test(origins) :- load, origins('OBS-3', O), O == ['FLIR-1'], origins('OBS-6', O6), O6 = [unknown('OBS-6')].
test(independent_distinct_recordings) :- load, independent_origin('OBS-1', 'OBS-2').
test(duplicated_origin_is_not_independent) :-
    load, independent_origin_tv('OBS-1', 'OBS-3', V), V == known_false, \+ independent_origin('OBS-3', 'OBS-1').
test(unknown_origin_is_unknown) :- load, independent_origin_tv('OBS-1', 'OBS-6', V), V == unknown.
test(corroborated_by_independent_pair) :- load, corroborated_case('CASE-A').
test(duplicated_origins_do_not_corroborate) :-
    load, corroboration_tv('CASE-B', V), V == unknown, \+ corroborated_case('CASE-B').
test(single_observation_flag_false) :- load, corroboration_tv('CASE-C', V), V == known_false.
test(telemetry_complete) :- load, telemetry_complete('OBS-2'), \+ telemetry_complete('OBS-1').
test(missing_calibration_is_unknown_not_false) :- load, telemetry_tv('OBS-6', V), V == unknown.
test(missing_timestamp_never_zero) :- load, telemetry_tv('OBS-6', V), V \== known_true.
test(range_from_telemetry, [nondet]) :- load, physically_identifiable('CASE-A', range), physically_identifiable('CASE-A', speed).
test(no_range_from_duplicated_bearing_track, [nondet]) :-
    load, identifiable_tv('CASE-B', range, V), V == known_false, \+ physically_identifiable('CASE-B', speed),
    physically_identifiable('CASE-B', bearing).
test(missing_calibration_blocks_range) :- load, identifiable_tv('CASE-C', range, V), V == unknown.
test(identifiable_enumerates, all(Q == [bearing, angular_rate, range, speed])) :- load, physically_identifiable('CASE-A', Q).
test(valid_path, all(P == [['OBS-3', 'OBS-1']])) :- load, valid_provenance_path('OBS-3', P).
test(primary_path, all(P == [['OBS-2']])) :- load, valid_provenance_path('OBS-2', P).
test(no_path_without_origin, fail) :- load, valid_provenance_path('OBS-6', _).
test(no_path_without_evidence, fail) :- load, valid_provenance_path('CASE-A', _).
test(contradictory_claims, all(X == ['CL-1'-'CL-2'])) :- load, conflicting_observation(A, B), X = A-B.
test(same_value_different_subject_is_not_a_conflict, fail) :- load, conflicting_observation('CL-1', 'CL-3').
test(no_conflict_across_attributes, fail) :- load, conflicting_observation('CL-1', 'CL-4').
:- end_tests(provenance).

:- begin_tests(adversarial).
:- use_module('../skytrace').

h(H) :- length(L, 64), maplist(=(a), L), string_chars(H, L).
r(Id, Extra, R) :- R0 = _{agency:"DOW", canonical_id:Id, release_epoch:"2026-05-08", mime:"video/mp4", source:"pursue"}, put_dict(Extra, R0, R).
code(G, C) :- catch((G, C = none), error(skytrace_error(C0, _), _), C = C0).

test(containment_cycle) :- r("A", _{parent_id:"B"}, A), r("B", _{parent_id:"A"}, B), code(load_batch_dict(_{records:[A, B]}), C), C == 'E_CYCLE'.
test(containment_self_loop) :- r("A", _{parent_id:"A"}, A), code(load_batch_dict(_{records:[A]}), C), C == 'E_CYCLE'.
test(derivation_cycle) :-
    r("A", _{derived_from:["C"]}, A), r("B", _{derived_from:["A"]}, B), r("C", _{derived_from:["B"]}, Cc),
    code(load_batch_dict(_{records:[A, B, Cc]}), C), C == 'E_CYCLE'.
test(self_derivation) :- r("A", _{derived_from:["A"]}, A), code(load_batch_dict(_{records:[A]}), C), C == 'E_CYCLE'.
test(dangling_parent) :- r("A", _{parent_id:"Z"}, A), code(load_batch_dict(_{records:[A]}), C), C == 'E_DANGLING'.
test(dangling_derivation) :- r("A", _{derived_from:["Z"]}, A), code(load_batch_dict(_{records:[A]}), C), C == 'E_DANGLING'.
test(duplicate_id) :- r("A", _{}, A), code(load_batch_dict(_{records:[A, A]}), C), C == 'E_DUPLICATE'.
test(duplicate_derivation_edge) :- r("A", _{}, A), r("B", _{derived_from:["A", "A"]}, B), code(load_batch_dict(_{records:[A, B]}), C), C == 'E_DUPLICATE'.
test(bad_digest) :- r("A", _{evidence_sha256:"ABC"}, A), code(load_batch_dict(_{records:[A]}), C), C == 'E_DIGEST'.
test(unknown_source) :- r("A", _{source:"blog"}, A), code(load_batch_dict(_{records:[A]}), C), C == 'E_SOURCE_POLICY'.
test(negative_uncertainty) :- r("A", _{timestamp_uncertainty_ms: -1}, A), code(load_batch_dict(_{records:[A]}), C), C == 'E_TIME'.
test(bad_mime) :- r("A", _{mime:"video"}, A), code(load_batch_dict(_{records:[A]}), C), C == 'E_SCHEMA'.
test(bad_tristate) :- r("A", _{calibrated:"yes"}, A), code(load_batch_dict(_{records:[A]}), C), C == 'E_SCHEMA'.
test(missing_field) :- code(load_batch_dict(_{records:[_{canonical_id:"A"}]}), C), C == 'E_SCHEMA'.
test(not_a_batch) :- code(load_batch_dict(_{items:[]}), C), C == 'E_SCHEMA'.
test(claim_dangling_subject) :-
    r("A", _{}, A), code(load_batch_dict(_{records:[A], claims:[_{claim_id:"X", subject:"Q", attribute:"a", value:1, evidence:"A"}]}), C), C == 'E_DANGLING'.
test(claim_duplicate_id) :-
    r("A", _{}, A), Cl = _{claim_id:"X", subject:"A", attribute:"a", value:1, evidence:"A"},
    code(load_batch_dict(_{records:[A], claims:[Cl, Cl]}), C), C == 'E_DUPLICATE'.
% Tabling: closure over a cyclic fact set (bypassing the loader) terminates and exposes the cycle.
test(tabled_closure_terminates_on_cycle) :-
    h(H), r("a", _{evidence_sha256:H, recording_origin_id:"O"}, Ra),
    assert_batch_terms([rec(a, Ra), rec(b, Ra), rec(c, Ra)],
                       [skytrace:edge_derived(a, b), skytrace:edge_derived(b, c), skytrace:edge_derived(c, a)]),
    findall(X, derives(a, X), Xs), sort(Xs, S), S == [a, b, c],
    provenance_cycle(b),
    code(check_acyclic, C), C == 'E_CYCLE',
    \+ valid_provenance_path(a, _).
test(tabled_closure_long_chain) :-
    numlist(1, 400, Ns), h(H), r("n", _{evidence_sha256:H}, R),
    findall(rec(N, R), member(N, Ns), Recs),
    findall(skytrace:edge_derived(N, M), (member(N, Ns), N < 400, M is N + 1), Es),
    assert_batch_terms(Recs, Es),
    aggregate_all(count, derives(1, _), K), K =:= 399, \+ provenance_cycle(_).
test(diamond_dag_is_not_a_forest_error) :-
    h(H), r("S", _{evidence_sha256:H, recording_origin_id:"O"}, S), r("L", _{evidence_sha256:H, derived_from:["S"]}, L),
    r("R", _{evidence_sha256:H, derived_from:["S"]}, R), r("J", _{evidence_sha256:H, derived_from:["L", "R"]}, J),
    load_batch_dict(_{records:[S, L, R, J]}),
    findall(P, valid_provenance_path('J', P), Ps), msort(Ps, Ps1), Ps1 == [['J', 'L', 'S'], ['J', 'R', 'S']],
    origins('J', O), O == ['O'], independent_origin_tv('L', 'R', V), V == known_false.
:- end_tests(adversarial).
