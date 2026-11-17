% SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
% Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
% Usage: swipl -q prolog/tests/run_tests.pl <source-dir>
% Runs every plunit suite; exits 0 only if all pass.
:- initialization(main, main).
:- use_module(library(plunit)).

main :-
    current_prolog_flag(argv, Argv),
    ( Argv = [Src|_] -> true ; format(user_error, "usage: run_tests.pl <source-dir>~n", []), halt(2) ),
    absolute_file_name(Src, Abs, [file_type(directory)]),
    setenv('SKYTRACE_SOURCE_DIR', Abs),
    atom_concat(Abs, '/prolog/tests/provenance.plt', P), atom_concat(Abs, '/prolog/tests/contracts.plt', C),
    load_files([P, C], [if(true)]),
    (   run_tests -> format("prolog: all plunit tests passed~n"), halt(0) ; halt(1) ).
