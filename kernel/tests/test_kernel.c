// SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
// Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
#include "../src/skytrace.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
  st_node valid[]={{"agency",-1,-1,-1,0},{"release",0,-1,-1,0},{"record",1,0,-1,0}};
  assert(st_validate(valid,3)==ST_OK);
  size_t ids[3]; assert(st_active_indexes(valid,3,ids,3)==3 && ids[2]==2);
  for(int a=-1;a<=1;a++) for(int b=-1;b<=1;b++) for(int r=-1;r<=1;r++) {
    assert(st_active(a,b,r)==(r!=1 && (a!=1||b!=1)));
    if(st_active(a,b,r)) assert(st_incomplete(a,b));
  }
  valid[2].parent=2; assert(st_validate(valid,3)==ST_CYCLE);
  valid[2].parent=1; valid[2].key="release"; assert(st_validate(valid,3)==ST_DUPLICATE);
  valid[2].key="record"; valid[2].parent=99; assert(st_validate(valid,3)==ST_BAD_PARENT);
  puts("kernel: all assertions passed");
}
