import fs from 'node:fs/promises';
import path from 'node:path';
import { FileBlob, SpreadsheetFile } from '@oai/artifact-tool';

const dir = path.dirname(new URL(import.meta.url).pathname).replace(/^\/(?=[A-Za-z]:)/, '');
const qa = path.join(decodeURIComponent(dir), 'qa/system-upper-method-alignment-20261012');
const plan = JSON.parse(await fs.readFile(path.join(qa, 'plan.json'), 'utf8'));
const before = process.argv.includes('--before');
for (const p of plan.books) {
  const wb = await SpreadsheetFile.importXlsx(await FileBlob.load(p.before));
  if (before) {
    const preview = await wb.render({sheetName:p.main, range:'H53:J53', scale:1.4, format:'png'});
    await fs.writeFile(path.join(qa,p.code.toLowerCase()+'-before.png'),new Uint8Array(await preview.arrayBuffer()));
    console.log(p.code, 'baseline rendered');
    continue;
  }
  for (const edit of p.edits) wb.worksheets.getItem(edit.sheet).getRange(edit.cell).values = [[edit.after]];
  wb.recalculate();
  const checks=[];
  for (const [sheet, addresses] of Object.entries(Object.groupBy(p.edits, e=>e.sheet))) {
    for (const edit of addresses) {
      const value=wb.worksheets.getItem(sheet).getRange(edit.cell).values[0][0];
      if(value!==edit.after) throw new Error(`Text mismatch ${p.code}/${sheet}/${edit.cell}`);
    }
  }
  const scan=await wb.inspect({kind:'match',searchTerm:'#REF!|#DIV/0!|#VALUE!|#NAME\\?|#NUM!|#SPILL!',options:{useRegex:true,maxResults:15},maxChars:1600});
  checks.push(scan.ndjson);
  const spot=await wb.inspect({kind:'table',range:`'${p.main}'!E34:J37`,include:'values,formulas',tableMaxRows:4,tableMaxCols:6,maxChars:2500});
  checks.push(spot.ndjson);
  await fs.writeFile(path.join(qa,p.code.toLowerCase()+'-artifact-check.json'), JSON.stringify(checks,null,2));
  const xlsx=await SpreadsheetFile.exportXlsx(wb);
  await xlsx.save(p.artifact);
  console.log(p.code, 'authored',p.edits.length,'cells');
}
