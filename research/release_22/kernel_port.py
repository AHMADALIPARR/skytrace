# SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
# Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
"""Bounded NDJSON process for OTP Port interoperability (one line => one reply).
This protocol process is testable without an Elixir runtime. Run:
python3 -u research/release_22/kernel_port.py
"""
import json,sys,traceback
from benchmark import physical_fixture,fit_physics,braid_matrix_features
MAX_LINE=65536

def handle(req):
    if not isinstance(req,dict) or req.get('operation')!='physical_fit':
        return {'status':'error','code':'E_OPERATION'}
    seed=req.get('seed')
    if type(seed) is not int or not 0<=seed<=1000000:
        return {'status':'error','code':'E_SEED'}
    case=physical_fixture(seed)
    return {'status':'ok','job_id':req.get('job_id'), 'known':fit_physics(case,True),
            'unknown':fit_physics(case,False)}

def serve(inp,out):
    for raw in inp.buffer:
        if len(raw)>MAX_LINE:
            out.write(json.dumps({'status':'error','code':'E_LINE_SIZE'})+'\n');out.flush();continue
        try:
            result=handle(json.loads(raw))
        except (ValueError,TypeError,KeyError,json.JSONDecodeError):
            result={'status':'error','code':'E_REQUEST'}
        except Exception:
            result={'status':'error','code':'E_KERNEL'}
        out.write(json.dumps(result,sort_keys=True,separators=(',',':'))+'\n');out.flush()
if __name__=='__main__':serve(sys.stdin,sys.stdout)
