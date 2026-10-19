import fs from 'node:fs/promises';
import {FileBlob, SpreadsheetFile} from '@oai/artifact-tool';

const plan = JSON.parse(await fs.readFile('plan.json', 'utf8'));
const workbook = await SpreadsheetFile.importXlsx(await FileBlob.load('before.xlsx'));
const sheet = workbook.worksheets.getItem(plan.sheet);
const before = await workbook.render({sheetName:plan.sheet, range:'B34:D40', scale:1});
await fs.writeFile('before.png', new Uint8Array(await before.arrayBuffer()));
if (process.argv.includes('--preview-only')) {
  console.log(JSON.stringify({status:'read-only preview prepared', blocks:plan.blocks.length}));
} else {
  if (plan.authoringState !== 'AUTHORIZED_WHITE_GRAY_EXCEPT_ORIGINAL_PINK_ID_MERGES') throw new Error('Await root final color decision');
  const preserved = new Set(plan.preserveAddresses);
  for (const block of plan.blocks) {
    sheet.getRange(`C${block.start}:J${block.end}`).format.fill = '#'+block.color;
    for (let row=block.start; row<=block.end; row++) {
      if (!preserved.has('B'+row)) sheet.getRange('B'+row).format.fill = '#'+block.color;
    }
  }
  workbook.recalculate();
  const after = await workbook.render({sheetName:plan.sheet, range:'B34:D40', scale:1});
  await fs.writeFile('after.png', new Uint8Array(await after.arrayBuffer()));
  await fs.writeFile('artifact-inspection.json', JSON.stringify(await workbook.inspect({kind:'computedStyle',sheetId:plan.sheet,range:'B34:D40',maxChars:5000}),null,2));
  const output = await SpreadsheetFile.exportXlsx(workbook);
  await output.save('authored.xlsx');
  console.log(JSON.stringify({status:'Artifact Tool fills authored', blocks:plan.blocks.length}));
}
