import fs from 'node:fs/promises';
import path from 'node:path';
import { FileBlob, SpreadsheetFile, Workbook } from '@oai/artifact-tool';
const h=import.meta.dirname,p=JSON.parse(await fs.readFile(path.join(h,'date-plan.json'),'utf8'));
for(const [key,b] of Object.entries(p)){
 const current=await SpreadsheetFile.importXlsx(await FileBlob.load(b.path));
 await current.inspect({kind:'sheet',include:'id,name',maxChars:1000});
 const stage=Workbook.create();const sh=stage.worksheets.add(b.sheet);
 for(const [a,c] of Object.entries(b.changes))sh.getRange(a).values=[[c.display]];
 stage.recalculate();await (await SpreadsheetFile.exportXlsx(stage)).save(path.join(h,key+'-date-authored.xlsx'));
 console.log(JSON.stringify({key,cells:Object.keys(b.changes),exported:true}));
}
