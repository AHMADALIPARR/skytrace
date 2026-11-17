#!/usr/bin/env node
import { readFile, writeFile, mkdir, readdir, copyFile } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { resolve, join, basename, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const sourcePath = join(root, 'data/sources.json');
const caseDir = join(root, 'data/cases');
const rawDir = join(root, 'data/raw');
const hash = buffer => createHash('sha256').update(buffer).digest('hex');
const readJSON = async p => JSON.parse(await readFile(p, 'utf8'));
const required = (ok, message) => { if (!ok) throw new Error(message); };
const stableCaseId = (sourceId, sourceRecordId) => 'ST-' + hash(Buffer.from(`${sourceId}\0${sourceRecordId}`)).slice(0,16).toUpperCase();
function validateCase(c) {
  required(c && typeof c === 'object', 'Case must be an object');
  for (const k of ['case_id','source_id','source_record_id','observed_at','time_precision','classification','evidence','claims']) required(Object.hasOwn(c,k),`Missing ${k}`);
  required(c.case_id === stableCaseId(c.source_id,c.source_record_id), 'Case ID mismatch');
  required(c.observed_at === null || !Number.isNaN(Date.parse(c.observed_at)), 'Bad observed_at');
  required(['exact','minute','hour','day','month','year','unknown'].includes(c.time_precision),'Bad precision');
  required(['unassessed','identified','insufficient_data','unresolved_after_analysis'].includes(c.classification), 'Bad classification');
  required(Array.isArray(c.evidence) && Array.isArray(c.claims), 'Evidence/claims must be arrays');
  for (const e of c.evidence) {
    required(typeof e.sha256==='string' && /^[0-9a-f]{64}$/.test(e.sha256),'Bad evidence hash');
    required(typeof e.relative_path==='string' && /^data\/raw\/[0-9a-f]{64}\.[a-z0-9]+$/.test(e.relative_path),'Bad evidence path');
  }
  return c;
}
async function cases() {
  const names = (await readdir(caseDir)).filter(x=>x.endsWith('.json')).sort();
  for (const name of names) {const c=validateCase(await readJSON(join(caseDir,name))); console.log(`${c.case_id}\t${c.source_id}\t${c.classification}\t${c.observed_at ?? 'unknown'}`); }
  console.log(`Total: ${names.length}`);
}
async function ingest(sourceId, sourceRecordId, file, url) {
  const registry=await readJSON(sourcePath);
  required(registry.sources.some(s=>s.id===sourceId),'Unknown source ID');
  required(sourceRecordId && file && url && /^https?:\/\//.test(url),'Usage: ingest <source-id> <source-record-id> <local-file> <original-url>');
  const bytes=await readFile(resolve(file));
  const digest=hash(bytes); const ext=(basename(file).split('.').pop() || 'bin').toLowerCase();
  required(/^[a-z0-9]{1,8}$/.test(ext),'Unsupported file extension');
  const relativePath=`data/raw/${digest}.${ext}`;
  await mkdir(rawDir,{recursive:true});
  await copyFile(resolve(file),join(root,relativePath));
  const id=stableCaseId(sourceId,sourceRecordId); const filePath=join(caseDir,id+'.json');
  let c;
  try {c=await readJSON(filePath);} catch(err) {if(err.code!=='ENOENT')throw err; c={case_id:id,source_id:sourceId,source_record_id:sourceRecordId,observed_at:null,time_precision:'unknown',classification:'unassessed',evidence:[],claims:[]};}
  required(!c.evidence.some(e=>e.relative_path===relativePath),'Evidence already linked to case');
  c.evidence.push({sha256:digest,relative_path:relativePath,source_url:url,ingested_at:new Date().toISOString(),bytes:bytes.byteLength});
  await mkdir(caseDir,{recursive:true}); await writeFile(filePath,JSON.stringify(validateCase(c),null,2)+'\n',{flag:'w'});
  console.log(JSON.stringify({case_id:id,sha256:digest,case_file:`data/cases/${id}.json`,evidence_file:relativePath},null,2));
}
async function verify() {
  let count=0;
  const names=(await readdir(caseDir)).filter(n=>n.endsWith('.json'));
  for (const name of names) {const c=validateCase(await readJSON(join(caseDir,name))); for (const e of c.evidence) {
    const bytes=await readFile(join(root,e.relative_path));
    required(hash(bytes)===e.sha256,`Hash mismatch ${e.relative_path}`);
    required(bytes.byteLength===e.bytes,`Byte-count mismatch ${e.relative_path}`); count++;
  }}
  console.log(`PASS: ${names.length} cases, ${count} evidence files independently checked`);
}
async function main(args) {
  const command=args[0];
  if(command==='sources') {const r=await readJSON(sourcePath); for (const s of r.sources) console.log(`${s.id}\t${s.tier}\t${s.url}`);}
  else if(command==='ingest') await ingest(...args.slice(1));
  else if(command==='cases') await cases();
  else if(command==='verify') await verify();
  else throw new Error('Commands: sources | ingest <source-id> <record-id> <local-file> <original-url> | cases | verify');
}
if(process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) main(process.argv.slice(2)).catch(err=>{console.error('ERROR:',err.message); process.exitCode=1;});
export { stableCaseId, validateCase, hash };
