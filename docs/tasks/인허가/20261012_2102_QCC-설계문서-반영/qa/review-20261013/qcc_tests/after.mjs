import fs from 'node:fs/promises';
import path from 'node:path';
import { FileBlob, SpreadsheetFile } from '@oai/artifact-tool';
const h=import.meta.dirname;
const p=JSON.parse(await fs.readFile(path.join(h,'author-payload.json'),'utf8'));
const out=[];
for(const [key,b] of Object.entries(p.books)){
 const wb=await SpreadsheetFile.importXlsx(await FileBlob.load(b.path));
 const range=key.startsWith('SU')?(key==='SUVC'?'B1737:K1737':'B1737:M1737'):key.startsWith('SI')?(key==='SITC'?'B136:M136':'B136:O136'):(key==='SSTC'?'B282:J283':'B282:L283');
 const img=await wb.render({sheetName:b.sheet,range,scale:1,format:'png'});
 const saved=path.join(h,`${key}-after.png`);
 await fs.writeFile(saved,new Uint8Array(await img.arrayBuffer()));
 out.push({key,path:saved,sheet:b.sheet,range});
 console.log(JSON.stringify(out.at(-1)));
}
await fs.writeFile(path.join(h,'artifact-render.json'),JSON.stringify({scope:'Saved current files: selected cell preview only, native full print remains root task',files:out},null,2));
