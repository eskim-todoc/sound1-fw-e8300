import fs from 'node:fs/promises';
import path from 'node:path';
import crypto from 'node:crypto';
import {FileBlob,SpreadsheetFile} from '@oai/artifact-tool';
const root=import.meta.dirname;
const plan=JSON.parse(await fs.readFile(path.join(root,'plan.json'),'utf8'));
await fs.mkdir(path.join(root,'authored'),{recursive:true});
const records=[];
for(const b of plan.books){
 const hash=crypto.createHash('sha256').update(await fs.readFile(b.path)).digest('hex').toUpperCase();
 if(hash!==b.sourceSHA256)throw new Error('Source hash mismatch');
 const wb=await SpreadsheetFile.importXlsx(await FileBlob.load(b.path));
 for(const g of b.groups){
 const main=wb.worksheets.getItem(g.main.name),compare=wb.worksheets.getItem(g.compare.name);
 for(const e of g.edits){main.getRange(e.cell).values=[[e.after]];compare.getRange(e.cell).values=[[e.compareAfter]];}
 for(const [row,height] of Object.entries(g.compareHeights))compare.getRange(`B${row}:L${row}`).format.rowHeight=height;
 for(const range of g.compareMerges??[])compare.getRange(range).merge();
 }
 wb.recalculate();
 for(const g of b.groups){for(const e of g.edits){if((wb.worksheets.getItem(g.main.name).getRange(e.cell).values[0][0]??'')!==e.after)throw new Error('Main value mismatch');if(wb.worksheets.getItem(g.compare.name).getRange(e.cell).values[0][0]!==e.compareAfter)throw new Error('Compare value mismatch');}}
 const record={code:b.code,sourceSHA256:hash,allRequestedValuesMatched:true};
 record.inspect=await wb.inspect({kind:'table',range:`'${b.groups[0].main.name}'!${b.code==='SRS'?'F93:I94':'E285:L285'}`,include:'values,formulas',tableMaxRows:2,tableMaxCols:8,maxChars:5000});
 record.output=path.join(root,'authored',`${b.code}.xlsx`);
 await(await SpreadsheetFile.exportXlsx(wb)).save(record.output);
 records.push(record);await fs.writeFile(path.join(root,'artifact-authoring.json'),JSON.stringify(records,null,2));
 console.log(JSON.stringify({code:b.code,allRequestedValuesMatched:true,exported:true}));
}
