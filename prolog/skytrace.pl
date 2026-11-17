/*  SKYTRACE provenance reasoning.

    Facts come from a JSON provenance batch in the fixtures/archives format (the same format the C
    `skytrace records` command loads), optionally extended with "derived_from" lists and "claims":

      {"records": [ {agency, canonical_id, release_epoch, mime, source, parent_id?, evidence_sha256?,
                     sensor_modality?, calibrated?, timestamp_uncertainty_ms?, recording_origin_id?,
                     has_range_telemetry?, corroborated?, resolved?, derived_from?: [id, ...]} ...],
       "claims":  [ {claim_id, subject, attribute, value, evidence} ... ] }

    Two graphs are kept apart and never confused:
      * containment forest  contained_in(Child, Parent)   from parent_id (one parent, acyclic);
      * provenance DAG      derived_from(Derived, Source) from derived_from (many sources, acyclic).

    Truth values are three-valued: known_true, known_false, unknown. A missing or null flag is
    unknown; a missing timestamp uncertainty is unknown, never zero. Each public predicate that
    "holds" (independent_origin/2, corroborated_case/1, telemetry_complete/1,
    physically_identifiable/2) succeeds exactly when its truth-value form yields known_true.
    active_case/1 is the exception by design: it is the C kernel policy (st_case_active), under which
    an unknown flag keeps a case open; active_case_tv/2 gives the three-valued reading.

    Errors are raised as  error(skytrace_error(Code, Context), _)  with Code one of the C engine's
    machine-readable names (E_SCHEMA, E_DUPLICATE, E_DANGLING, E_CYCLE, E_SOURCE_POLICY, E_DIGEST,
    E_TIME, E_BOUND).
*/
:- module(skytrace,
          [ load_batch/1, load_batch_dict/1, clear_batch/0, assert_batch_terms/2,
            tv_and/3, tv_or/3, tv_not/2, tv_value/1,
            source_record/3, contained_in/2, contains_path/2, derived_from/2, derives/2,
            origin/2, origins/2,
            active_case/1, active_case_tv/2, active_case_strict/1, case_codes/4, kernel_active/4, strict_active/4,
            independent_origin/2, independent_origin_tv/3,
            observation_of/2, corroborated_case/1, corroboration_tv/2,
            telemetry_complete/1, telemetry_tv/2,
            physically_identifiable/2, identifiable_tv/3,
            valid_provenance_path/2,
            claim/5, conflicting_observation/2,
            provenance_cycle/1, check_acyclic/0, record_count/1
          ]).
:- use_module(library(http/json)).
:- use_module(library(lists)).
:- use_module(library(apply)).
:- use_module(library(pcre)).

:- dynamic rec/2, edge_contains/2, edge_derived/2, claim_fact/5.

% ------------------------------------------------------------------ three-valued (Kleene) logic
tv_value(known_true).  tv_value(known_false).  tv_value(unknown).

tv_and(known_false, _, known_false) :- !.
tv_and(_, known_false, known_false) :- !.
tv_and(known_true, known_true, known_true) :- !.
tv_and(_, _, unknown).
tv_or(known_true, _, known_true) :- !.
tv_or(_, known_true, known_true) :- !.
tv_or(known_false, known_false, known_false) :- !.
tv_or(_, _, unknown).
tv_not(known_true, known_false).
tv_not(known_false, known_true).
tv_not(unknown, unknown).

tv_all(L, V) :- foldl([X,A0,A]>>tv_and(A0, X, A), L, known_true, V).
tv_any(L, V) :- foldl([X,A0,A]>>tv_or(A0, X, A), L, known_false, V).

% ------------------------------------------------------------------ loading
registry_source(S) :- memberchk(S, [pursue, aaro, nara, nasa, geipan, nuforc, opensky, celestrak, jpl_fireball, ams]).

fail_with(Code, Ctx) :- throw(error(skytrace_error(Code, Ctx), _)).

clear_batch :-
    retractall(rec(_, _)), retractall(edge_contains(_, _)), retractall(edge_derived(_, _)),
    retractall(claim_fact(_, _, _, _, _)), abolish_all_tables.

load_batch(File) :-
    setup_call_cleanup(open(File, read, In, [encoding(utf8)]),
                       json_read_dict(In, D, [null(null), true(true), false(false), value_string_as(string)]),
                       close(In)),
    load_batch_dict(D).

load_batch_dict(D) :-
    clear_batch,
    (   is_dict(D), get_dict(records, D, Rs), is_list(Rs) -> true ; fail_with('E_SCHEMA', records_not_array) ),
    (   get_dict(claims, D, Cs) -> ( is_list(Cs) -> true ; fail_with('E_SCHEMA', claims_not_array) ) ; Cs = [] ),
    foldl(load_record, Rs, 0, _),
    forall(rec(Id, R), link_record(Id, R)),
    check_containment_forest,
    check_acyclic,
    foldl(load_claim, Cs, 0, _).

% Build a batch directly from Prolog terms rec(Id, Dict) (used by adversarial tests that need
% states the JSON loader refuses, e.g. a derivation cycle, to show tabled closure still terminates).
assert_batch_terms(Recs, Edges) :-
    clear_batch,
    forall(member(rec(Id, R), Recs), assertz(rec(Id, R))),
    forall(member(E, Edges), assertz(E)).

load_record(R, I, I1) :-
    I1 is I + 1,
    ( is_dict(R) -> true ; fail_with('E_SCHEMA', record_not_object(I)) ),
    forall(member(K, [agency, canonical_id, release_epoch, mime, source]), req_string(R, K, I)),
    forall(member(K, [parent_id, evidence_sha256, sensor_modality, recording_origin_id]), opt_string(R, K, I)),
    forall(member(K, [has_range_telemetry, corroborated, resolved, calibrated]), tri_field(R, K, I)),
    get_dict(mime, R, Mime),
    (   sub_string(Mime, _, _, _, "/"), \+ sub_string(Mime, _, _, _, "\n"), \+ sub_string(Mime, _, _, _, "\r")
    ->  true ; fail_with('E_SCHEMA', invalid_mime(I)) ),
    get_dict(source, R, S0), atom_string(S, S0),
    ( registry_source(S) -> true ; fail_with('E_SOURCE_POLICY', invalid_source(I)) ),
    (   get_dict(evidence_sha256, R, H), H \== null
    ->  ( re_match("^[0-9a-f]{64}$", H) -> true ; fail_with('E_DIGEST', invalid_evidence_sha256(I)) )
    ;   true ),
    (   get_dict(timestamp_uncertainty_ms, R, U), U \== null
    ->  ( number(U), U >= 0 -> true ; fail_with('E_TIME', invalid_timestamp_uncertainty(I)) )
    ;   true ),
    (   get_dict(derived_from, R, DF)
    ->  ( is_list(DF), maplist(string, DF) -> true ; fail_with('E_SCHEMA', invalid_derived_from(I)) )
    ;   true ),
    get_dict(canonical_id, R, Id0), atom_string(Id, Id0),
    ( rec(Id, _) -> fail_with('E_DUPLICATE', duplicate_canonical_id(Id)) ; true ),
    assertz(rec(Id, R)).

req_string(R, K, I) :- ( get_dict(K, R, V), string(V), V \== "" -> true ; fail_with('E_SCHEMA', missing_or_empty(K, I)) ).
opt_string(R, K, I) :- ( get_dict(K, R, V), V \== null -> ( string(V), V \== "" -> true ; fail_with('E_SCHEMA', not_a_string(K, I)) ) ; true ).
tri_field(R, K, I) :- ( get_dict(K, R, V) -> ( memberchk(V, [true, false, null]) -> true ; fail_with('E_SCHEMA', not_tristate(K, I)) ) ; true ).

link_record(Id, R) :-
    (   get_dict(parent_id, R, P0), P0 \== null
    ->  atom_string(P, P0),
        ( P == Id -> fail_with('E_CYCLE', containment_cycle(Id)) ; true ),
        ( rec(P, _) -> assertz(edge_contains(P, Id)) ; fail_with('E_DANGLING', unknown_parent_id(Id, P)) )
    ;   true ),
    (   get_dict(derived_from, R, DF)
    ->  forall(member(S0, DF),
               ( atom_string(S, S0),
                 ( S == Id -> fail_with('E_CYCLE', self_derivation(Id)) ; true ),
                 ( rec(S, _) -> true ; fail_with('E_DANGLING', unknown_derivation_source(Id, S)) ),
                 ( edge_derived(Id, S) -> fail_with('E_DUPLICATE', duplicate_derivation(Id, S)) ; assertz(edge_derived(Id, S)) ) ))
    ;   true ).

load_claim(C, I, I1) :-
    I1 is I + 1,
    ( is_dict(C) -> true ; fail_with('E_SCHEMA', claim_not_object(I)) ),
    forall(member(K, [claim_id, subject, attribute, evidence]), req_string(C, K, I)),
    ( get_dict(value, C, V) -> true ; fail_with('E_SCHEMA', missing_or_empty(value, I)) ),
    maplist([K, A]>>(get_dict(K, C, S), atom_string(A, S)), [claim_id, subject, attribute, evidence], [Cid, Subj, Attr, Ev]),
    ( rec(Subj, _) -> true ; fail_with('E_DANGLING', unknown_claim_subject(Cid, Subj)) ),
    ( rec(Ev, _) -> true ; fail_with('E_DANGLING', unknown_claim_evidence(Cid, Ev)) ),
    ( claim_fact(Cid, _, _, _, _) -> fail_with('E_DUPLICATE', duplicate_claim_id(Cid)) ; true ),
    assertz(claim_fact(Cid, Subj, Attr, V, Ev)).

record_count(N) :- aggregate_all(count, rec(_, _), N).

% ------------------------------------------------------------------ graph structure
source_record(Id, Source, Agency) :-
    rec(Id, R), get_dict(source, R, S), atom_string(Source, S), get_dict(agency, R, A), atom_string(Agency, A).

contained_in(Child, Parent) :- edge_contains(Parent, Child).
derived_from(Derived, Source) :- edge_derived(Derived, Source).

% Transitive closures are tabled: they terminate on any finite graph, cyclic or not.
:- table contains_path/2.
contains_path(A, D) :- edge_contains(A, D).
contains_path(A, D) :- contains_path(A, M), edge_contains(M, D).

:- table derives/2.
derives(D, S) :- edge_derived(D, S).
derives(D, S) :- derives(D, M), edge_derived(M, S).

provenance_cycle(X) :- derives(X, X).

check_containment_forest :-
    (   edge_contains(P1, C), edge_contains(P2, C), P1 \== P2
    ->  fail_with('E_SCHEMA', ambiguous_parent(C)) ; true ),
    (   contains_path(X, X) -> fail_with('E_CYCLE', containment_cycle(X)) ; true ).
check_acyclic :-
    (   provenance_cycle(X) -> fail_with('E_CYCLE', derivation_cycle(X)) ; true ).

% ------------------------------------------------------------------ record field access
flag(Id, Key, V) :-
    rec(Id, R),
    (   get_dict(Key, R, X) -> tri_of(X, V) ; V = unknown ).
tri_of(true, known_true).
tri_of(false, known_false).
tri_of(null, unknown).

field(Id, Key, V) :- rec(Id, R), get_dict(Key, R, V0), V0 \== null, atom_string(V, V0).

% ------------------------------------------------------------------ recording origins
% Primary records have no derivation sources. Their origin is the recording_origin_id, or
% unknown(Id) when it is missing: two unknown origins are never assumed equal or distinct.
primary(Id) :- rec(Id, _), \+ edge_derived(Id, _).
:- table origin/2.
origin(Id, O) :- primary(Id), ( field(Id, recording_origin_id, O0) -> O = O0 ; O = unknown(Id) ).
origin(Id, O) :- derives(Id, P), primary(P), origin(P, O).
origins(Id, Os) :- rec(Id, _), findall(O, origin(Id, O), Os0), sort(Os0, Os).

% independent_origin_tv(A, B, V): known_false if A and B share a known origin or one derives from
% the other; known_true if all origins are known and disjoint; unknown otherwise.
independent_origin_tv(A, B, V) :-
    rec(A, _), rec(B, _), A \== B,
    origins(A, OA), origins(B, OB),
    (   ( derives(A, B) ; derives(B, A) ) -> V = known_false
    ;   member(O, OA), O \= unknown(_), memberchk(O, OB) -> V = known_false
    ;   ( member(unknown(_), OA) ; member(unknown(_), OB) ) -> V = unknown
    ;   V = known_true ).
independent_origin(A, B) :- independent_origin_tv(A, B, known_true).

% ------------------------------------------------------------------ cases
case_codes(Id, R, C, S) :-
    rec(Id, _),
    maplist([K, X]>>(flag(Id, K, T), code(T, X)), [has_range_telemetry, corroborated, resolved], [R, C, S]).
code(known_true, 1).  code(known_false, 0).  code(unknown, -1).

% Kernel policy, identical to C st_case_active: resolution 1 closes; otherwise the case stays open
% unless range telemetry and corroboration are both known present.
kernel_active(R, C, S, A) :- ( S =:= 1 -> A = 0 ; ( R =\= 1 ; C =\= 1 ) -> A = 1 ; A = 0 ).
% Publication policy, identical to C st_case_active_strict: only known-unresolved cases count.
strict_active(R, C, S, A) :- ( S =\= 0 -> A = 0 ; ( R =\= 1 ; C =\= 1 ) -> A = 1 ; A = 0 ).

active_case(Id) :- case_codes(Id, R, C, S), kernel_active(R, C, S, 1).
active_case_strict(Id) :- case_codes(Id, R, C, S), strict_active(R, C, S, 1).
% Truth value of "this case is open": unknown whenever resolution is unknown.
active_case_tv(Id, V) :-
    flag(Id, resolved, Res), flag(Id, has_range_telemetry, Rg), flag(Id, corroborated, Co),
    tv_and(Rg, Co, Complete), tv_not(Complete, Incomplete), tv_not(Res, Open),
    tv_and(Open, Incomplete, V).

% Observations of a case: records with evidence contained (transitively) in the case.
observation_of(Obs, Case) :- contains_path(Case, Obs), field(Obs, evidence_sha256, _).
observation_of(Case, Case) :- field(Case, evidence_sha256, _).

% Corroboration is computed, not trusted: known_true needs two observations with independent
% origins. A record flag "corroborated": true without such a pair stays unknown (asserted but not
% established); known_false needs the flag false and no independent pair, or all pairs known_false
% from duplicated origins with no flag claiming otherwise.
corroboration_tv(Case, V) :-
    rec(Case, _),
    findall(O, observation_of(O, Case), Os0), sort(Os0, Os),
    findall(T, (member(A, Os), member(B, Os), A @< B, independent_origin_tv(A, B, T)), Ts),
    tv_any(Ts, Pair),
    flag(Case, corroborated, Flag),
    (   Pair == known_true -> V = known_true
    ;   Flag == known_false -> ( Pair == unknown -> V = unknown ; V = known_false )
    ;   Flag == known_true -> V = unknown
    ;   Ts \== [], Pair == known_false -> V = known_false
    ;   V = unknown ).
corroborated_case(Case) :- corroboration_tv(Case, known_true).

% Telemetry completeness: range telemetry present, calibration present, timestamp uncertainty known.
telemetry_tv(Id, V) :-
    flag(Id, has_range_telemetry, Rg), flag(Id, calibrated, Cal),
    rec(Id, R),
    (   get_dict(timestamp_uncertainty_ms, R, U), number(U) -> Tm = known_true ; Tm = unknown ),
    tv_all([Rg, Cal, Tm], V).
telemetry_complete(Id) :- telemetry_tv(Id, known_true).

% Physical identifiability of a quantity for a case.
%   bearing      : needs an observation with calibration.
%   angular_rate : bearing plus known timestamp uncertainty.
%   range        : range telemetry with calibration, or two calibrated observations with
%                  independent origins (triangulation). Never inferred from a single bearing track.
%   speed        : range plus known timestamps.
identifiable_tv(Case, Q, V) :-
    rec(Case, _),
    findall(O, observation_of(O, Case), Os0), sort(Os0, Os),
    ident(Q, Case, Os, V).
ident(bearing, _, Os, V) :- findall(C, (member(O, Os), flag(O, calibrated, C)), Cs), tv_any(Cs, V).
ident(angular_rate, C, Os, V) :- ident(bearing, C, Os, B), timestamps(Os, T), tv_and(B, T, V).
ident(range, _, Os, V) :-
    findall(T, (member(O, Os), flag(O, has_range_telemetry, Rg), flag(O, calibrated, Ca), tv_and(Rg, Ca, T)), Direct),
    findall(T, (member(A, Os), member(B, Os), A @< B, flag(A, calibrated, CA), flag(B, calibrated, CB),
                independent_origin_tv(A, B, I), tv_all([CA, CB, I], T)), Tri),
    append(Direct, Tri, All), tv_any(All, V).
ident(speed, C, Os, V) :- ident(range, C, Os, R), timestamps(Os, T), tv_and(R, T, V).
timestamps(Os, V) :-
    findall(T, (member(O, Os), rec(O, R), ( get_dict(timestamp_uncertainty_ms, R, U), number(U) -> T = known_true ; T = unknown )), Ts),
    tv_any(Ts, V).
physically_identifiable(Case, Q) :- nonvar(Q), memberchk(Q, [bearing, angular_rate, range, speed]), identifiable_tv(Case, Q, known_true).
physically_identifiable(Case, Q) :- var(Q), member(Q, [bearing, angular_rate, range, speed]), identifiable_tv(Case, Q, known_true).

% A valid provenance path runs from a record back through derived_from to a primary record with a
% known recording origin; every node carries an evidence digest and no node repeats. Bounded by the
% number of records, so enumeration terminates even on corrupted (cyclic) fact sets.
valid_provenance_path(Id, Path) :-
    record_count(N),
    prov_walk(Id, [Id], N, Rev), reverse(Rev, Path).
prov_walk(Id, Seen, _, Seen) :-
    primary(Id), field(Id, recording_origin_id, _), field(Id, evidence_sha256, _).
prov_walk(Id, Seen, N, Path) :-
    length(Seen, L), L < N,
    field(Id, evidence_sha256, _),
    edge_derived(Id, S), \+ memberchk(S, Seen),
    prov_walk(S, [S|Seen], N, Path).

% ------------------------------------------------------------------ claims
claim(Cid, Subject, Attribute, Value, Evidence) :- claim_fact(Cid, Subject, Attribute, Value, Evidence).
% Two claims conflict when they assign different values to the same attribute of the same subject.
conflicting_observation(C1, C2) :-
    claim_fact(C1, S, A, V1, _), claim_fact(C2, S, A, V2, _), C1 @< C2, V1 \== V2.
