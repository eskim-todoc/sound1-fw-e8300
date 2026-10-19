import fs from 'node:fs/promises';
import path from 'node:path';
import {FileBlob, SpreadsheetFile, Workbook} from '@oai/artifact-tool';
const h=import.meta.dirname;
const plan=JSON.parse(await fs.readFile(path.join(h,'system-height-plan.json'),'utf8'));
for (const [key,b] of Object.entries(plan.books)) {
  const current=await SpreadsheetFile.importXlsx(await FileBlob.load(b.path));
  await current.inspect({kind:'sheet',include:'id,name',maxChars:1000});
  const stage=Workbook.create(); const sh=stage.worksheets.add(b.sheet);
  for (const [row,height] of Object.entries(b.heights)) {
    sh.getRange(`B${row}:L${row}`).format.rowHeight=height;
  }
  stage.recalculate();
  await (await SpreadsheetFile.exportXlsx(stage)).save(path.join(h,key+'-system-height-authored.xlsx'));
  console.log(JSON.stringify({key,imported:true,heightRows:Object.keys(b.heights).length,exported:true}));
}
