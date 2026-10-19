import fs from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
import {FileBlob, SpreadsheetFile} from '@oai/artifact-tool';

const task = fileURLToPath(new URL('./', import.meta.url));
const qa = `${task}/qa/system-advertisement-gate`;
const mode = process.argv[2] ?? 'before';
const wb = await SpreadsheetFile.importXlsx(await FileBlob.load(`${qa}/system-${mode === 'review' ? 'candidate' : 'before'}.xlsx`));
if (mode === 'build') {
  const plan = JSON.parse(await fs.readFile(`${qa}/plan.json`, 'utf8'));
  for (const item of plan.changes) {
    wb.worksheets.getItem(item.sheet).getRange(item.cell).values = [[item.after]];
  }
  for (const item of plan.heights) {
    const last = item.sheet === '시험 보조자료' ? 'D' : 'L';
    wb.worksheets.getItem(item.sheet).getRange(`A${item.row}:${last}${item.row}`).format.rowHeight = item.height;
  }
  wb.recalculate();
  console.log((await wb.inspect({kind:'region', sheetId:wb.worksheets.getItem('보고서_시스템 시험').id, range:'J188:L189', maxChars:1600})).ndjson);
  await (await SpreadsheetFile.exportXlsx(wb)).save(`${qa}/system-authored.xlsx`);
} else {
  for (const [name, sheet, range] of [
    ['conditions', '보고서_시스템 시험', 'I188:J189'],
    ['results', '보고서_시스템 시험', 'K188:L189'],
    ['helper', '시험 보조자료', 'A188:D189'],
  ]) {
    const png = await wb.render({sheetName:sheet, range, scale:1, format:'png'});
    await fs.writeFile(`${qa}/${mode}-${name}.png`, new Uint8Array(await png.arrayBuffer()));
  }
  console.log(`${mode}: rendered three relevant views.`);
}
