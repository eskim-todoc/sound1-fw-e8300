import fs from 'node:fs/promises';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {Workbook,SpreadsheetFile} from '@oai/artifact-tool';
const dir=path.dirname(fileURLToPath(import.meta.url));
const plans=JSON.parse(await fs.readFile(path.join(dir,'test_plan.json'),'utf8'));
for(const p of plans){
 const w=Workbook.create();const s=w.worksheets.add(p.sheet);const d=w.worksheets.add(p.sheet+'_변경 비교');
 s.getRange('A1').write(p.physical);
 d.getRange('A1').write(p.diffphysical.map(e=>[e.mark,...e.values.slice(1)]));
 p.diffphysical.forEach((e,i)=>{if(e.mark)d.getRange(`A${i+1}:M${i+1}`).format.fill=e.mark==='[-]'?'#FCE4D6':'#E2F0D9'});
 w.recalculate();
 console.log((await w.inspect({kind:'table',range:`'${p.sheet}'!A1:K2`,include:'values',tableMaxRows:2,tableMaxCols:11})).ndjson);
 await(await SpreadsheetFile.exportXlsx(w)).save(path.join(dir,p.kind+'-authored.xlsx'));
}
