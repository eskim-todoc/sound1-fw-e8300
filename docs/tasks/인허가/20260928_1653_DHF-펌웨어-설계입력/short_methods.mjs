import fs from 'node:fs/promises';import path from 'node:path';import {fileURLToPath} from 'node:url';
import {FileBlob,SpreadsheetFile} from '@oai/artifact-tool';
const t=path.dirname(fileURLToPath(import.meta.url)),q=path.join(t,'short-methods-20260929');
const target=path.resolve(t,'../../../참고/인허가/사운드1/DHF/02_설계 입력/SW/펌웨어/SSTC-FW-TD24A 소프트웨어 시스템 시험 기준서.xlsx');
await fs.mkdir(q,{recursive:true});await fs.copyFile(target,path.join(q,'before.xlsx'));
const w=await SpreadsheetFile.importXlsx(await FileBlob.load(target)),edits=[];
const map={Test:'T',Demonstration:'D',Inspection:'I',Analysis:'A'};
for(let i=1;i<=2;i++){
 const s=w.worksheets.getItemAt(i),v=s.getRange('B25:D250').values;
 for(let k=0;k<v.length;k++){
  if(!String(v[k][0]).startsWith('SW-FW-SYS-Test-ID-'))continue;
  const old=v[k][2];if(typeof old!=='string')continue;
  const value=old.split('\n').map(x=>map[x.trim()]??x).join('\n');
  if(value!==old){const addr='D'+(k+25);s.getRange(addr).values=[[value]];edits.push({sheet:s.name,index:i+1,addr,old,value});}
 }
}
w.recalculate();await(await SpreadsheetFile.exportXlsx(w)).save(path.join(q,'authored.xlsx'));
await fs.writeFile(path.join(q,'plan.json'),JSON.stringify({target,edits},null,2));
const img=await w.render({sheetName:w.worksheets.getItemAt(1).name,range:'B25:F28',scale:1.3});await fs.writeFile(path.join(q,'preview.png'),new Uint8Array(await img.arrayBuffer()));
console.log('Method abbreviations:',edits.length);
