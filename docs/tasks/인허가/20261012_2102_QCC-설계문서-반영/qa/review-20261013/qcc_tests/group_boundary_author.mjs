import fs from 'node:fs/promises';
import path from 'node:path';
import {FileBlob, SpreadsheetFile, Workbook} from '@oai/artifact-tool';
const h=import.meta.dirname;
const plan=JSON.parse(await fs.readFile(path.join(h,'group-boundary-plan.json'),'utf8'));
for (const [key,b] of Object.entries(plan)) {
  const current=await SpreadsheetFile.importXlsx(await FileBlob.load(b.path));
  await current.inspect({kind:'sheet',include:'id,name',maxChars:1000});
  // Artifact authoring is limited to the alignment it supports. Manual print
  // breaks and untouched package features are integrated selectively below.
  const stage=Workbook.create(); const sh=stage.worksheets.add(b.sheet);
  for (const a of b.top_alignment_cells) {
    sh.getRange(a).format.horizontalAlignment='left';
    sh.getRange(a).format.verticalAlignment='top';
  }
  stage.recalculate();
  await (await SpreadsheetFile.exportXlsx(stage)).save(path.join(h,key+'-group-boundary-authored.xlsx'));
  console.log(JSON.stringify({key,imported:true,alignment:b.top_alignment_cells,exported:true}));
}
