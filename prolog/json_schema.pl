% SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
% Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
/*  JSON Schema (draft 2020-12) validator for the subset of keywords used by spec/schemas.

    Supported: $schema $id title description (annotations), type (incl. lists), const, enum,
    properties, required, additionalProperties (boolean), items, minItems, uniqueItems, minLength,
    pattern (PCRE), minimum, maximum, format (date-time, uri), $ref to #/$defs/..., oneOf, allOf,
    if/then/else.

    check_schema/1 rejects any keyword outside that subset instead of ignoring it, so a schema can
    never silently lose a constraint here. validate/3 collects every violation as
    violation(JsonPointer, Keyword, Detail); valid/2 succeeds iff there are none.
    Integers follow JSON Schema semantics: 1.0 is an integer.
*/
:- module(json_schema, [load_json/2, check_schema/1, validate/3, valid/2]).
:- use_module(library(http/json)).
:- use_module(library(pcre)).
:- use_module(library(lists)).
:- use_module(library(apply)).

load_json(File, D) :-
    setup_call_cleanup(open(File, read, In, [encoding(utf8)]),
                       json_read_dict(In, D, [null(null), true(true), false(false), value_string_as(string), tag('')]),
                       close(In)).

known_keyword('$schema'). known_keyword('$id'). known_keyword(title). known_keyword(description).
known_keyword('$defs'). known_keyword(type). known_keyword(const). known_keyword(enum).
known_keyword(properties). known_keyword(required). known_keyword(additionalProperties).
known_keyword(items). known_keyword(minItems). known_keyword(uniqueItems). known_keyword(minLength).
known_keyword(pattern). known_keyword(minimum). known_keyword(maximum). known_keyword(format).
known_keyword('$ref'). known_keyword(oneOf). known_keyword(allOf). known_keyword(if).
known_keyword(then). known_keyword(else).

check_schema(S) :- check_schema(S, "#").
check_schema(S, _) :- S == true, !.
check_schema(S, _) :- S == false, !.
check_schema(S, P) :-
    ( is_dict(S) -> true ; throw(error(skytrace_error('E_SCHEMA', schema_not_object(P)), _)) ),
    dict_pairs(S, _, KVs),
    forall(member(K-V, KVs), check_kw(K, V, P)).
check_kw(K, _, P) :- \+ known_keyword(K), !, throw(error(skytrace_error('E_SCHEMA', unsupported_keyword(P, K)), _)).
check_kw(type, T, P) :- !, ( is_list(T) -> L = T ; L = [T] ), forall(member(X, L), ( memberchk(X, ["object","array","string","number","integer","boolean","null"]) -> true ; throw(error(skytrace_error('E_SCHEMA', bad_type(P, X)), _)) )).
check_kw(K, V, P) :- memberchk(K, [properties, '$defs']), !, dict_pairs(V, _, KVs), forall(member(N-S, KVs), (atomic_list_concat([P, '/', K, '/', N], Q), check_schema(S, Q))).
check_kw(K, V, P) :- memberchk(K, [items, if, then, else]), !, atomic_list_concat([P, '/', K], Q), check_schema(V, Q).
check_kw(K, V, P) :- memberchk(K, [oneOf, allOf]), !, ( is_list(V), V \== [] -> true ; throw(error(skytrace_error('E_SCHEMA', empty_combinator(P, K)), _)) ), forall(nth0(I, V, S), (atomic_list_concat([P, '/', K, '/', I], Q), check_schema(S, Q))).
check_kw(pattern, V, P) :- !, ( string(V), catch(re_compile(V, _, []), _, fail) -> true ; throw(error(skytrace_error('E_SCHEMA', bad_pattern(P)), _)) ).
check_kw(required, V, P) :- !, ( is_list(V), maplist(string, V) -> true ; throw(error(skytrace_error('E_SCHEMA', bad_required(P)), _)) ).
check_kw(format, V, P) :- !, ( memberchk(V, ["date-time", "uri"]) -> true ; throw(error(skytrace_error('E_SCHEMA', unsupported_format(P, V)), _)) ).
check_kw(additionalProperties, V, P) :- !, ( memberchk(V, [true, false]) -> true ; throw(error(skytrace_error('E_SCHEMA', unsupported_additionalProperties(P)), _)) ).
check_kw(_, _, _).

valid(Schema, Value) :- validate(Schema, Value, []).
validate(Schema, Value, Violations) :- findall(V, violation(Schema, Schema, Value, "", V), Violations).

violation(_, S, _, _, _) :- S == true, !, fail.
violation(_, S, _, P, violation(P, false, schema_false)) :- S == false, !.
violation(Root, S, X, P, V) :- dict_pairs(S, _, KVs), member(K-Arg, KVs), kw(K, Arg, S, Root, X, P, V).

json_type(X, "null") :- X == null.
json_type(X, "boolean") :- ( X == true ; X == false ).
json_type(X, "string") :- string(X).
json_type(X, "number") :- number(X).
json_type(X, "integer") :- integer(X).
json_type(X, "integer") :- float(X), X =:= truncate(X).
json_type(X, "array") :- is_list(X).
json_type(X, "object") :- is_dict(X).

json_equal(A, B) :- number(A), number(B), !, A =:= B.
json_equal(A, B) :- is_list(A), is_list(B), !, maplist(json_equal, A, B).
json_equal(A, B) :- is_dict(A), is_dict(B), !, dict_pairs(A, _, PA), dict_pairs(B, _, PB),
    pairs_keys(PA, K), pairs_keys(PB, K), pairs_values(PA, VA), pairs_values(PB, VB), maplist(json_equal, VA, VB).
json_equal(A, B) :- A == B.

kw(type, T, _, _, X, P, violation(P, type, T)) :- ( is_list(T) -> L = T ; L = [T] ), \+ (member(Ty, L), json_type(X, Ty)).
kw(const, C, _, _, X, P, violation(P, const, C)) :- \+ json_equal(C, X).
kw(enum, E, _, _, X, P, violation(P, enum, E)) :- \+ (member(C, E), json_equal(C, X)).
kw(required, Req, _, _, X, P, violation(P, required, K)) :- is_dict(X), member(K, Req), atom_string(A, K), \+ get_dict(A, X, _).
kw(properties, Ps, _, Root, X, P, V) :- is_dict(X), dict_pairs(Ps, _, KVs), member(K-S, KVs), get_dict(K, X, Y), child(P, K, Q), violation(Root, S, Y, Q, V).
kw(additionalProperties, false, S, _, X, P, violation(Q, additionalProperties, K)) :-
    is_dict(X), dict_pairs(X, _, KVs), member(K-_, KVs),
    \+ ( get_dict(properties, S, Ps), get_dict(K, Ps, _) ), child(P, K, Q).
kw(items, S, _, Root, X, P, V) :- is_list(X), nth0(I, X, Y), child(P, I, Q), violation(Root, S, Y, Q, V).
kw(minItems, N, _, _, X, P, violation(P, minItems, N)) :- is_list(X), length(X, L), L < N.
kw(uniqueItems, true, _, _, X, P, violation(P, uniqueItems, duplicate)) :- is_list(X), append(_, [A|T], X), member(B, T), json_equal(A, B), !.
kw(minLength, N, _, _, X, P, violation(P, minLength, N)) :- string(X), string_length(X, L), L < N.
kw(pattern, Re, _, _, X, P, violation(P, pattern, Re)) :- string(X), \+ re_match(Re, X).
kw(minimum, M, _, _, X, P, violation(P, minimum, M)) :- number(X), X < M.
kw(maximum, M, _, _, X, P, violation(P, maximum, M)) :- number(X), X > M.
kw(format, "date-time", _, _, X, P, violation(P, format, "date-time")) :- string(X), \+ rfc3339(X).
kw(format, "uri", _, _, X, P, violation(P, format, "uri")) :- string(X), \+ re_match("^[A-Za-z][A-Za-z0-9+.-]*:[^\\s]*$", X).
kw('$ref', Ref, _, Root, X, P, V) :- resolve(Root, Ref, S), violation(Root, S, X, P, V).
kw(allOf, Ss, _, Root, X, P, V) :- member(S, Ss), violation(Root, S, X, P, V).
kw(oneOf, Ss, _, Root, X, P, violation(P, oneOf, matched(N))) :-
    aggregate_all(count, (member(S, Ss), \+ violation(Root, S, X, P, _)), N), N =\= 1.
kw(if, If, S, Root, X, P, V) :-
    (   \+ violation(Root, If, X, P, _)
    ->  get_dict(then, S, Th), violation(Root, Th, X, P, V)
    ;   get_dict(else, S, El), violation(Root, El, X, P, V) ).

child(P, K, Q) :- format(string(Q), "~w/~w", [P, K]).

resolve(Root, Ref, S) :-
    (   sub_string(Ref, 0, _, A, "#/$defs/"), sub_string(Ref, _, A, 0, Name), atom_string(N, Name),
        get_dict('$defs', Root, Defs), get_dict(N, Defs, S0)
    ->  S = S0
    ;   throw(error(skytrace_error('E_SCHEMA', unresolvable_ref(Ref)), _)) ).

% RFC 3339 date-time with calendar validation (month lengths, leap years, leap second 60).
rfc3339(S) :-
    re_matchsub("^(\\d{4})-(\\d{2})-(\\d{2})[Tt](\\d{2}):(\\d{2}):(\\d{2})(\\.\\d+)?([Zz]|[+-](\\d{2}):(\\d{2}))$", S, M, []),
    maplist([K, N]>>(get_dict(K, M, V), number_string(N, V)), [1, 2, 3, 4, 5, 6], [Y, Mo, D, H, Mi, Se]),
    between(1, 12, Mo), month_days(Y, Mo, MD), between(1, MD, D),
    between(0, 23, H), between(0, 59, Mi), between(0, 60, Se),
    (   get_dict(9, M, OH) -> number_string(OHn, OH), number_string(OMn, M.10), between(0, 23, OHn), between(0, 59, OMn) ; true ).
month_days(Y, 2, D) :- !, ( (Y mod 4 =:= 0, Y mod 100 =\= 0 ; Y mod 400 =:= 0) -> D = 29 ; D = 28 ).
month_days(_, M, 30) :- memberchk(M, [4, 6, 9, 11]), !.
month_days(_, _, 31).
