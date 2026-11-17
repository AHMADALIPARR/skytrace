#include "skytrace.h"
#include <string.h>
/* This 32-bit integer predicate is the SAW LLVM proof target. */
int32_t st_incomplete(int32_t range, int32_t corroborated) {
  return (range != 1 || corroborated != 1) ? 1 : 0;
}
int32_t st_active(int32_t range, int32_t corroborated, int32_t resolution) {
  if (resolution == 1) return 0;
  return st_incomplete(range, corroborated);
}
/* Parent links form a rooted forest: unlike a general DAG, there is only one path to a leaf. */
st_status st_validate(const st_node *nodes, size_t n) {
  if (n && !nodes) return ST_BAD_VALUE;
  for (size_t i=0;i<n;++i) {
    if (!nodes[i].key || !*nodes[i].key) return ST_BAD_VALUE;
    if (nodes[i].has_range < -1 || nodes[i].has_range > 1 ||
        nodes[i].corroborated < -1 || nodes[i].corroborated > 1 ||
        nodes[i].resolution < -1 || nodes[i].resolution > 1) return ST_BAD_VALUE;
    if (nodes[i].parent < -1 || (nodes[i].parent >= 0 && (size_t)nodes[i].parent >= n)) return ST_BAD_PARENT;
    for (size_t j=0;j<i;++j) if (!strcmp(nodes[i].key,nodes[j].key)) return ST_DUPLICATE;
    /* Functional parent relation: walking more than n edges implies a cycle. */
    int cur=(int)i;
    for(size_t steps=0;steps<=n;++steps) {
      if(cur == -1) break;
      if(steps == n) return ST_CYCLE;
      cur=nodes[cur].parent;
    }
  }
  return ST_OK;
}
size_t st_active_indexes(const st_node *nodes, size_t n, size_t *out, size_t capacity) {
  if (st_validate(nodes,n)!=ST_OK) return SIZE_MAX;
  size_t count=0;
  for(size_t i=0;i<n;++i) if(st_active(nodes[i].has_range,nodes[i].corroborated,nodes[i].resolution)) {
    if(count < capacity && out) out[count]=i;
    ++count;
  }
  return count;
}
