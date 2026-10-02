import fs from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
import {Workbook,SpreadsheetFile} from '@oai/artifact-tool';
const dir=new URL('./qa/unit-reconciliation/',import.meta.url);
const plan=JSON.parse(await fs.readFile(new URL('tables-plan.json',dir),'utf8'));
const w=Workbook.create();
for(const [name,data] of [
 ['본문',plan.suvc.physical],['비교',plan.suvc.diffphysical],['원본 대응표',plan.suvc.original_rows],
 ['1차본 대응표',plan.suvc.first_draft_rows],['유닛 목록',plan.suvc.code_rows],
 ['SITC 참조',plan.sitc.map(x=>[x.sheet,x.cell,x.new])]]){
 const s=w.worksheets.add(name);s.getRange(`A1:${String.fromCharCode(64+data[0].length)}${data.length}`).values=data;
}
w.recalculate();
console.log((await w.inspect({kind:'table',range:'본문!B1:K2',include:'values',tableMaxRows:2,tableMaxCols:10})).ndjson);
console.log((await w.inspect({kind:'match',searchTerm:'#REF!|#DIV/0!|#VALUE!|#NAME\\?',options:{useRegex:true,maxResults:20},summary:'formula error scan'})).ndjson);
await (await SpreadsheetFile.exportXlsx(w)).save(fileURLToPath(new URL('units-authored.xlsx',dir)));
console.log('Unit data authored and exported');
