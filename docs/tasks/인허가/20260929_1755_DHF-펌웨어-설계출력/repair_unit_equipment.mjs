import fs from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
import {FileBlob,SpreadsheetFile} from '@oai/artifact-tool';
const task=fileURLToPath(new URL('./',import.meta.url));
const qa=`${task}/qa/unit-equipment-repair`;
const p=JSON.parse(await fs.readFile(`${qa}/plan.json`,'utf8'));
const mode=process.argv[2]??'build';
if(mode==='build') {
  for(const [key,path,sheet] of [['criterion',`${qa}/criterion-before.xlsx`,p.sheet],['report',`${qa}/report-before.xlsx`,p.sourceSheet]]) {
    const wb=await SpreadsheetFile.importXlsx(await FileBlob.load(path));
    const pic=await wb.render({sheetName:sheet,range:'B13:F24',scale:1,format:'png'});
    await fs.writeFile(`${qa}/${key}-before.png`,new Uint8Array(await pic.arrayBuffer()));
    if(key==='report')continue;
    const s=wb.worksheets.getItem(p.sheet);
    for(const c of p.changes){s.getRange(c.cell).values=[[c.value]];s.getRange(c.cell).format=c.format;}
    for(const [row,height] of Object.entries(p.heights))if(height!==null)s.getRange(`B${row}:F${row}`).format.rowHeight=height;
    wb.recalculate();
    console.log((await wb.inspect({kind:'region',sheetId:p.sheet,range:'C15:F24',tableMaxCellChars:60,tableMaxRows:4,maxChars:1200})).ndjson);
    await(await SpreadsheetFile.exportXlsx(wb)).save(`${qa}/criterion-authored.xlsx`);
  }
} else {
  const wb=await SpreadsheetFile.importXlsx(await FileBlob.load(`${qa}/criterion-candidate.xlsx`));
  const pic=await wb.render({sheetName:p.sheet,range:'B13:F24',scale:1,format:'png'});
  await fs.writeFile(`${qa}/criterion-after.png`,new Uint8Array(await pic.arrayBuffer()));
}
