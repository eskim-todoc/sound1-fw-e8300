import fs from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
import {FileBlob, SpreadsheetFile} from '@oai/artifact-tool';

const followUp = process.argv.includes('--dates-format');
const confirmed = process.argv.includes('--confirmed-results');
const reviewed = process.argv.includes('--review-complete');
const qa = new URL(reviewed ? 'qa/unit-review-color-criteria-sync-20261012/' : confirmed ? 'qa/audit-report-confirmed-results-20261012/' : followUp ? 'qa/audit-report-dates-format-20261012/' : 'qa/audit-expected-report-20261012/', import.meta.url);
const before = process.argv.includes('--before');
const final = process.argv.includes('--final');
const filename = final ? 'report.xlsx' : 'report-before.xlsx';
const wb = await SpreadsheetFile.importXlsx(await FileBlob.load(fileURLToPath(new URL(filename,qa))));
const name = '보고서_유닛 시험';
const sheet = wb.worksheets.getItem(name);
console.log((await wb.inspect({kind:'sheet',include:'id,name',maxChars:800})).ndjson);
if (!before && !final) {
  const plan = JSON.parse(await fs.readFile(new URL('sheet-edits.json',qa),'utf8'));
  for (const [address, value] of Object.entries(plan.edits)) sheet.getRange(address).values = [[value]];
  for (const address of plan.fills ?? []) sheet.getRange(address).format.fill = '#FFF2CC';
  for (const item of plan.restore_fills ?? []) sheet.getRange(item.cell).format.fill = item.color;
  for (const address of plan.clear_marker_fonts ?? []) {
    sheet.getRange(address).format.font.color = '#000000';
    sheet.getRange(address).format.font.bold = false;
  }
  for (const [row, height] of Object.entries(plan.heights)) sheet.getRange(`A${row}:M${row}`).format.rowHeight = height;
  wb.recalculate();
  const errors = await wb.inspect({kind:'match',searchTerm:'#REF!|#DIV/0!|#VALUE!|#NAME\\?|#N/A|#NUM!|#NULL!|#SPILL!|#CALC!',options:{useRegex:true,maxResults:20},summary:'formula error scan'});
  await fs.writeFile(new URL('artifact-errors.json',qa),JSON.stringify(errors));
  await (await SpreadsheetFile.exportXlsx(wb)).save(fileURLToPath(new URL('report-artifact.xlsx',qa)));
}
for (const range of before ? ['K809:M811'] : ['K790:M792','K809:M811','K1212:M1213']) {
  const image = await wb.render({sheetName:name,range,scale:1,format:'png'});
  await fs.writeFile(new URL(`${before?'before':final?'final':'authored'}-${range.replace(':','_')}.png`,qa),new Uint8Array(await image.arrayBuffer()));
}
console.log(before?'baseline rendered':final?'saved workbook rendered':'changed cells authored and inspected');
