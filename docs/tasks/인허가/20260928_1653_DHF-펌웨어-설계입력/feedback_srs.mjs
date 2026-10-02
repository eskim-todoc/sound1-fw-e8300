import fs from 'node:fs/promises';import path from 'node:path';import {fileURLToPath} from 'node:url';
import {Workbook,SpreadsheetFile} from '@oai/artifact-tool';
const q=path.join(path.dirname(fileURLToPath(import.meta.url)),'feedback-srs-20260929'),j=JSON.parse(await fs.readFile(path.join(q,'plan.json'),'utf8'));
const w=Workbook.create(),s=w.worksheets.add('본문'),d=w.worksheets.add('비교');
s.getRange('B6').write(j.body.map(x=>x.values));
d.getRange('A1').write(j.entries.map(x=>[x.mark,...x.values]));
j.entries.forEach((e,i)=>{if(e.mark)d.getRange(`A${i+1}:I${i+1}`).format.fill=e.mark==='[-]'?'#FCE4D6':'#E2F0D9'});
w.recalculate();await(await SpreadsheetFile.exportXlsx(w)).save(path.join(q,'authored.xlsx'));
