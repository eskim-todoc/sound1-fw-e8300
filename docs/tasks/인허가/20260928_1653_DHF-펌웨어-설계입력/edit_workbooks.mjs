import fs from 'node:fs/promises';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {FileBlob,SpreadsheetFile} from '@oai/artifact-tool';
const task=path.dirname(fileURLToPath(import.meta.url));
const jobs=JSON.parse(await fs.readFile(path.join(task,'workbook-plan.json'),'utf8'));
for(const job of jobs){
 const wb=await SpreadsheetFile.importXlsx(await FileBlob.load(job.staged));
 console.log((await wb.inspect({kind:'sheet',include:'id,name',maxChars:1000})).ndjson);
 for(const [name,edits] of Object.entries(job.edits)){
  const sh=wb.worksheets.getItem(name);
  for(const [addr,value] of Object.entries(edits)){
   if(typeof value==='string'&&value.startsWith('='))sh.getRange(addr).formulas=[[value]];
   else sh.getRange(addr).values=[[value]];
  }
 }
 wb.recalculate();
 const out=path.join(task,'build','artifact-'+job.name);
 await(await SpreadsheetFile.exportXlsx(wb)).save(out);
 console.log('Edited',out);
}
