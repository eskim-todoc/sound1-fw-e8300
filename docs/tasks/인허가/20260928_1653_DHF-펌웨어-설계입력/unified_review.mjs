import fs from 'node:fs/promises';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {Workbook,SpreadsheetFile} from '@oai/artifact-tool';
const q=path.join(path.dirname(fileURLToPath(import.meta.url)),'unified-20260929');
const jobs=JSON.parse(await fs.readFile(path.join(q,'plan.json'),'utf8'));
for(const j of jobs){
 const w=Workbook.create(),s=w.worksheets.add('재배치'),d=w.worksheets.add(j.diffname);
 for(const r of j.body){s.getRangeByIndexes(r.row-1,1,1,j.cols-1).values=[r.values];}
 d.getRangeByIndexes(0,0,j.entries.length,j.cols).values=j.entries.map(e=>[e.mark,...e.values]);
 for(let i=0;i<j.entries.length;i++){
  const e=j.entries[i];if(!e.mark)continue;
  d.getRangeByIndexes(i,0,1,j.cols).format.fill=e.mark==='[-]'?'#FCE4D6':'#E2F0D9';
 }
 w.recalculate();
 await(await SpreadsheetFile.exportXlsx(w)).save(path.join(q,j.kind+'-authored.xlsx'));
 console.log(j.kind,'authored');
}
