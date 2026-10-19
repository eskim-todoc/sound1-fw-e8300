import fs from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
import {FileBlob, SpreadsheetFile} from '@oai/artifact-tool';

const task = fileURLToPath(new URL('./', import.meta.url));
const qa = `${task}/qa/system-readable-rev0`;
const mode = process.argv[2] ?? 'before';
const wb = await SpreadsheetFile.importXlsx(await FileBlob.load(`${qa}/system-${mode === 'review' ? 'candidate' : 'before'}.xlsx`));
if (mode === 'build') {
  const plan = JSON.parse(await fs.readFile(`${qa}/plan.json`, 'utf8'));
  for (const item of plan.changes) {
    wb.worksheets.getItem(item.sheet).getRange(item.cell).values = [[item.after]];
  }
  for (const address of ['C18','D18']) {
    wb.worksheets.getItem('보고서_시스템 시험').getRange(address).format.wrapText = true;
  }
  wb.recalculate();
  const inspected = await wb.inspect({kind:'region', sheetId:wb.worksheets.getItem('보고서_시스템 시험').id, range:'J65:L66', maxChars:2800});
  console.log(inspected.ndjson);
  await (await SpreadsheetFile.exportXlsx(wb)).save(`${qa}/system-authored.xlsx`);
} else {
  const views = mode === 'before' ? [
    ['equipment', '보고서_시스템 시험', 'C18:F18'],
    ['current', '보고서_시스템 시험', 'I41:J43'],
    ['nofm', '보고서_시스템 시험', 'G64:J67'],
  ] : [
    ['equipment', '보고서_시스템 시험', 'C18:F18'],
    ['purpose', '보고서_시스템 시험', 'C34:I34'],
    ['current', '보고서_시스템 시험', 'I41:J43'],
    ['nofm', '보고서_시스템 시험', 'G64:J67'],
    ['led', '보고서_시스템 시험', 'I98:J99'],
    ['deployment', '보고서_시스템 시험', 'C204:J205'],
    ['security', '사이버보안 시험 연계', 'I24:J28'],
    ['helper', '시험 보조자료', 'A43:D43'],
  ];
  for (const [name, sheet, range] of views) {
    const png = await wb.render({sheetName:sheet, range, scale:1, format:'png'});
    await fs.writeFile(`${qa}/${mode}-${name}.png`, new Uint8Array(await png.arrayBuffer()));
  }
  console.log(`${mode}: rendered ${views.length} relevant views.`);
}
