import fs from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
import {Workbook,SpreadsheetFile,FileBlob} from '@oai/artifact-tool';
const legendOnly=process.argv.includes('--legend-only');
const q=new URL(legendOnly?'./qa/method-legend-cleanup/':'./qa/method-definitions/',import.meta.url);
const p=JSON.parse(await fs.readFile(new URL('plan.json',q),'utf8'));
for(const b of p.books){
 const w=Workbook.create();
 const datasets=legendOnly?[['정의',[[p.legend]]],['유닛 안내',[[p.unitNote]]]]:[['변경 셀',b.changes.map(x=>[x.row,x.col,x.value])],['비교',b.diff],['정의',[[p.legend]]]];
 for(const [name,data] of datasets){
  const s=w.worksheets.add(name);
  const r=s.getRange(`A1:${String.fromCharCode(64+data[0].length)}${data.length}`);r.values=data;
  r.format.font.name='맑은 고딕';r.format.font.size=11;r.format.wrapText=true;
 }
 w.recalculate();
 console.log(b.key,(await w.inspect({kind:'table',range:legendOnly?'정의!A1':'변경 셀!A1:C3',include:'values',tableMaxRows:3,tableMaxCols:3})).ndjson);
 await (await SpreadsheetFile.exportXlsx(w)).save(fileURLToPath(new URL(b.key+'/authored.xlsx',q)));
}
