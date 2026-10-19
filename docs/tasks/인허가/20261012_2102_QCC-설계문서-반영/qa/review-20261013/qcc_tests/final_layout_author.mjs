import fs from 'node:fs/promises';
import path from 'node:path';
import { FileBlob, SpreadsheetFile, Workbook } from '@oai/artifact-tool';
const h=import.meta.dirname,p=JSON.parse(await fs.readFile(path.join(h,'final-layout-plan.json'),'utf8'));
for(const [key,b] of Object.entries(p)){
 const current=await SpreadsheetFile.importXlsx(await FileBlob.load(b.path));
 await current.inspect({kind:'sheet',include:'id,name',maxChars:1000});
 const stage=Workbook.create();const sh=stage.worksheets.add(b.sheet);
 for(const [a,c] of Object.entries(b.changes))sh.getRange(a).values=[[c.display]];
 for(const [row,height] of Object.entries(b.heights))sh.getRange(`B${row}:M${row}`).format.rowHeight=height;
 for(const a of b.top_alignment_cells)sh.getRange(a).format.verticalAlignment='top';
 stage.recalculate();
 await stage.inspect({kind:'table',range:`'${b.sheet}'!F131:M141`,include:'values,formulas',tableMaxRows:11,tableMaxCols:8,maxChars:1000});
 await (await SpreadsheetFile.exportXlsx(stage)).save(path.join(h,key+'-final-layout-authored.xlsx'));
 console.log(JSON.stringify({key,cells:Object.keys(b.changes),heights:b.heights,topAlignment:b.top_alignment_cells,exported:true}));
}
