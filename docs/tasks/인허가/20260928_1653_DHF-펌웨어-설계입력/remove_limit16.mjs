import fs from 'node:fs/promises';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {FileBlob,SpreadsheetFile} from '@oai/artifact-tool';
const t=path.dirname(fileURLToPath(import.meta.url)),q=path.join(t,'remove-limit16-20260929');
const target=path.resolve(t,'../../../참고/인허가/사운드1/DHF/02_설계 입력/SW/펌웨어/SSTC-FW-TD24A 소프트웨어 시스템 시험 기준서.xlsx');
await fs.mkdir(q,{recursive:true});await fs.copyFile(target,path.join(q,'before.xlsx'));
const w=await SpreadsheetFile.importXlsx(await FileBlob.load(target)),edits=[];
for(let i=1;i<=2;i++){
 const s=w.worksheets.getItemAt(i),v=s.getRange('A25:J173').values;
 for(let k=0;k<v.length;k++){
  if(v[k][1]!=='SW-FW-SYS-Test-ID-16'||(i===2&&v[k][0]==='[-]'))continue;
  const addr='H'+(k+25);
  const old=s.getRange(addr).values[0][0];
  if(!old)throw Error('Expected limit not found');
  if(i===1){const b=await w.render({sheetName:s.name,range:`G${k+25}:J${k+25}`,scale:1.5});await fs.writeFile(path.join(q,'before.png'),new Uint8Array(await b.arrayBuffer()));}
  s.getRange(addr).clear({applyTo:'contents'});edits.push({sheet:s.name,index:i+1,addr,value:null});
 }
}
if(edits.length!==2)throw Error('Expected exactly two cells');
w.recalculate();
console.log((await w.inspect({kind:'table',range:'기준서_시스템 시험!G40:J40',include:'values',tableMaxRows:1,tableMaxCols:4,maxChars:1000})).ndjson);
await(await SpreadsheetFile.exportXlsx(w)).save(path.join(q,'authored.xlsx'));
const b=await w.render({sheetName:w.worksheets.getItemAt(1).name,range:'G40:J40',scale:1.5});await fs.writeFile(path.join(q,'after.png'),new Uint8Array(await b.arrayBuffer()));
await fs.writeFile(path.join(q,'plan.json'),JSON.stringify({target,edits},null,2));
