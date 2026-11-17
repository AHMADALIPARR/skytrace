// SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
// Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
#ifndef SKYTRACE_CLI_H
#define SKYTRACE_CLI_H
#include "skytrace/error.h"
st_code cli_sources(const char *root, st_error *err);
st_code cli_cases(const char *root, st_error *err);
st_code cli_verify(const char *root, st_error *err);
st_code cli_ingest(const char *root, int argc, char **argv, st_error *err);
st_code cli_analyze(st_error *err);
st_code cli_records(const char *path, st_error *err);
st_code cli_braid_check(st_error *err);
st_code cli_braid_equiv(const char *a, const char *b, st_error *err);
st_code cli_quantum_check(st_error *err);
st_code cli_fit(const char *reference_path, const char *python_results_path, st_error *err);
st_code cli_export(const char *dir, st_error *err);
st_code cli_research(const char *study, const char *root, st_error *err);
st_code cli_research_check(const char *study, const char *root, st_error *err);
st_code cli_port(st_error *err);
st_code cli_research_export(const char *dir, const char *root, st_error *err);
#endif
