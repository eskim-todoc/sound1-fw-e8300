import fs from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
import {FileBlob,SpreadsheetFile} from '@oai/artifact-tool';
const task=fileURLToPath(new URL('./',import.meta.url));
const qa=`${task}/qa/integration-readable-frames`;
const p=JSON.parse(await fs.readFile(`${qa}/plan.json`,'utf8'));
const mode=process.argv[2]??'build';
for(const [key,b] of Object.entries(p.books)) {
  const path=mode==='review'?`${qa}/${key}-candidate.xlsx`:b.backup;
  const wb=await SpreadsheetFile.importXlsx(await FileBlob.load(path));
  console.log((await wb.inspect({kind:'sheet',include:'id,name',maxChars:1200})).ndjson);
  const sh=wb.worksheets.getItem(b.sheet);
  if(mode==='build') {
    const before=await wb.render({sheetName:b.sheet,range:'M107:N107',scale:1,format:'png'});
    await fs.writeFile(`${qa}/${key}-before.png`,new Uint8Array(await before.arrayBuffer()));
    for(const c of b.changes){sh.getRange(c.cell).values=[[c.after]];sh.getRange(`M${c.row}:N${c.row}`).format.rowHeight=c.height;}
    wb.recalculate();
    console.log((await wb.inspect({kind:'formula',sheetId:b.sheet,range:'A1:O130',maxChars:800})).ndjson);
    await(await SpreadsheetFile.exportXlsx(wb)).save(`${qa}/${key}-authored.xlsx`);
  } else {
    console.log((await wb.inspect({kind:'region',sheetId:b.sheet,range:'M107:N110',tableMaxCellChars:160,tableMaxRows:4,maxChars:1600})).ndjson);
    for(const row of [107,108,109,110]) {
      const png=await wb.render({sheetName:b.sheet,range:`M${row}:${key==='report'?'N':'M'}${row}`,scale:1,format:'png'});
      await fs.writeFile(`${qa}/${key}-after-${row}.png`,new Uint8Array(await png.arrayBuffer()));
    }
  }
}
