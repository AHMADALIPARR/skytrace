/* skytrace: single native entry point. Root directory = $SKYTRACE_ROOT or the current directory. */
#define _POSIX_C_SOURCE 200809L
#include "cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *USAGE =
  "Commands: sources | ingest <source-id> <record-id> <local-file> <original-url> | cases | verify"
  " | analyze (NDJSON stdin) | records <batch.json> | braid-check | braid-equiv <w1> <w2>"
  " | quantum-check | fit <physical_fixture.json> <release_22 results.json> | export <dir>";

int main(int argc, char **argv) {
  const char *root = getenv("SKYTRACE_ROOT");
  if (!root || !*root) root = ".";
  st_error err; st_error_clear(&err);
  st_code rc;
  const char *cmd = argc > 1 ? argv[1] : "";
  if (!strcmp(cmd, "sources")) rc = cli_sources(root, &err);
  else if (!strcmp(cmd, "ingest")) rc = cli_ingest(root, argc - 2, argv + 2, &err);
  else if (!strcmp(cmd, "cases")) rc = cli_cases(root, &err);
  else if (!strcmp(cmd, "verify")) rc = cli_verify(root, &err);
  else if (!strcmp(cmd, "analyze")) rc = cli_analyze(&err);
  else if (!strcmp(cmd, "records") && argc == 3) rc = cli_records(argv[2], &err);
  else if (!strcmp(cmd, "braid-check")) rc = cli_braid_check(&err);
  else if (!strcmp(cmd, "braid-equiv") && argc == 4) rc = cli_braid_equiv(argv[2], argv[3], &err);
  else if (!strcmp(cmd, "quantum-check")) rc = cli_quantum_check(&err);
  else if (!strcmp(cmd, "fit") && argc == 4) rc = cli_fit(argv[2], argv[3], &err);
  else if (!strcmp(cmd, "export") && argc == 3) rc = cli_export(argv[2], &err);
  else rc = st_fail(&err, ST_E_ARGUMENT, -1, "%s", USAGE);
  fflush(stdout);
  if (rc) { fprintf(stderr, "ERROR: %s\n", err.message); return 1; }
  return 0;
}
