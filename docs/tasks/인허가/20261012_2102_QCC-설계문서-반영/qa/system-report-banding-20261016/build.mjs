import fs from 'node:fs/promises';
import { createRequire } from 'node:module';
import { pathToFileURL } from 'node:url';
const req = createRequire('file:///C:/Users/EUNSU/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/artifact-loader.js');
const { FileBlob, SpreadsheetFile } = await import(pathToFileURL(req.resolve('@oai/artifact-tool')).href);
const plan = JSON.parse(await fs.readFile('plan.json', 'utf8'));
const wb = await SpreadsheetFile.importXlsx(await FileBlob.load(plan.authorInput));
const sh = wb.worksheets.getItem(plan.sheet);
const preserve = new Set(plan.preserveAddresses);
let ranges = 0;
for (const block of plan.blocks) {
  sh.getRange(`C${block.start}:L${block.end}`).format.fill = '#' + block.color;
  ranges++;
  if (!preserve.has(`B${block.start}`)) {
    sh.getRange(`B${block.start}:B${block.end}`).format.fill = '#' + block.color;
    ranges++;
  }
}
const out = await SpreadsheetFile.exportXlsx(wb);
await out.save('authored.xlsx');
console.log(JSON.stringify({ status: 'authored', tests: plan.blocks.length, ranges, preservedPinkTESTs: plan.pinkIDs.length }));
