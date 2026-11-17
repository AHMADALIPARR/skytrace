#ifndef SKYTRACE_H
#define SKYTRACE_H
#include <stddef.h>
#include <stdint.h>
/* -1 unknown; 0 absent; 1 present. */
typedef struct { const char *key; int parent; int has_range; int corroborated; int resolution; } st_node;
typedef enum { ST_OK=0, ST_DUPLICATE=1, ST_BAD_PARENT=2, ST_CYCLE=3, ST_BAD_VALUE=4, ST_NON_UNIQUE_PATH=5 } st_status;
/* Resolution: 0 unresolved; 1 resolved; -1 unknown. Unknowns remain reviewable. */
int32_t st_active(int32_t range, int32_t corroborated, int32_t resolution);
int32_t st_incomplete(int32_t range, int32_t corroborated);
st_status st_validate(const st_node *nodes, size_t n);
size_t st_active_indexes(const st_node *nodes, size_t n, size_t *out, size_t capacity);
#endif
