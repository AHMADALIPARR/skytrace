import test from 'node:test';
import assert from 'node:assert/strict';
import { stableCaseId, validateCase, hash } from '../bin/skytrace.mjs';
test('deterministic ID stable and namespace-separated',()=>{
 assert.equal(stableCaseId('aaro','PR-018'),stableCaseId('aaro','PR-018'));
 assert.notEqual(stableCaseId('aaro','PR-018'),stableCaseId('pursue','PR-018'));
});
test('sha256 known test vector',()=> assert.equal(hash(Buffer.from('abc')),'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad'));
test('valid empty-case schema accepted',()=>{
 const c={case_id:stableCaseId('aaro','PR-018'),source_id:'aaro',source_record_id:'PR-018',observed_at:null,time_precision:'unknown',classification:'unassessed',evidence:[],claims:[]};
 assert.equal(validateCase(c),c);
});
test('reject corrupt ID',()=>{
 assert.throws(()=>validateCase({case_id:'wrong',source_id:'aaro',source_record_id:'PR-018',observed_at:null,time_precision:'unknown',classification:'unassessed',evidence:[],claims:[]}));
});
test('reject evidence directory traversal',()=>{
 assert.throws(()=>validateCase({case_id:stableCaseId('aaro','PR-018'),source_id:'aaro',source_record_id:'PR-018',observed_at:null,time_precision:'unknown',classification:'unassessed',evidence:[{sha256:'0'.repeat(64),relative_path:'../../private'}],claims:[]}));
});
