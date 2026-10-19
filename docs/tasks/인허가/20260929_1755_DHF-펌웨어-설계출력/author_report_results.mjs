// 좁은 셀 편집만 수행한다. Rich text/기존 native 객체는 검증 후 패키지 보존 단계에 결합한다.
import fs from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
import {FileBlob, SpreadsheetFile} from '@oai/artifact-tool';
const task = fileURLToPath(new URL('./', import.meta.url));
const qa = `${task}/qa/${process.env.TDC_REPORT_QA ?? 'report-result-format'}`;
const base = JSON.parse(await fs.readFile(`${qa}/baseline.json`, 'utf8'));
if (process.argv[2] === 'help-rows') {
  const wb = await SpreadsheetFile.importXlsx(await FileBlob.load(base.books[0].backup));
  console.log(wb.help('*', {search:'insertRows|rows.insert|insert.*row',include:'index,examples,notes',maxChars:5500}).ndjson);
  process.exit(0);
}
const plan = JSON.parse(await fs.readFile(`${qa}/plan.json`, 'utf8'));
const mode = process.argv[2] ?? 'build';
const keys = process.argv.slice(3);
for (const b of base.books.filter(b => !keys.length || keys.includes(b.key))) {
  const wb = await SpreadsheetFile.importXlsx(await FileBlob.load(
    mode === 'review' ? `${qa}/${b.key}-candidate.xlsx` :
      mode === 'delta' ? `${qa}/${b.key}-authored.xlsx` : (b.authorInput ?? b.backup)));
  if (mode === 'build' || mode === 'delta') {
    if (mode === 'build') {
      const before = await wb.render({sheetName:b.sheet,range:b.views[0],scale:1,format:'png'});
      await fs.writeFile(`${qa}/${b.key}-before.png`,new Uint8Array(await before.arrayBuffer()));
    }
    for (const m of (plan.unmerges ?? []).filter(m => m.key === b.key)) {
      wb.worksheets.getItem(m.sheet).unmergeCells(m.range);
    }
    for (const p of (plan.clears ?? []).filter(p => p.key === b.key)) {
      wb.worksheets.getItem(p.sheet).getRange(p.range).clear({applyTo:'contents'});
    }
    for (const p of plan.changes.filter(p => p.key === b.key)) {
      wb.worksheets.getItem(p.sheet).getRange(p.cell).values = [[p.value]];
    }
    for (const p of (plan.wraps ?? []).filter(p => p.key === b.key)) {
      wb.worksheets.getItem(p.sheet).getRange(p.range).format.wrapText = true;
    }
    // 한 조건의 여러 물리 행을 한 셀로 표시한다. 값은 위쪽 첫 셀에만 둔다.
    for (const m of (plan.merges ?? []).filter(m => m.key === b.key)) {
      wb.worksheets.getItem(m.sheet).mergeCells(m.range);
    }
    for (const h of (plan.heights ?? []).filter(h => h.key === b.key)) {
      wb.worksheets.getItem(h.sheet).getRange(`A${h.row}:M${h.row}`).format.rowHeight = h.height;
    }
    for (const p of (plan.textColors ?? []).filter(p => p.key === b.key)) {
      wb.worksheets.getItem(p.sheet).getRange(p.cell).format.font.color = p.color;
    }
    for (const p of (plan.borders ?? []).filter(p => p.key === b.key)) {
      wb.worksheets.getItem(p.sheet).getRange(p.range).format.borders = p.edges;
    }
    for (const sh of plan.shades.filter(sh => sh.key === b.key)) {
      const range = `${String.fromCharCode(64+sh.firstcol)}${sh.start}:${String.fromCharCode(64+sh.lastcol)}${sh.end}`;
      wb.worksheets.getItem(sh.sheet).getRange(range).format.fill = '#F5F5F5';
    }
    wb.recalculate();
    const errors = await wb.inspect({kind:'match', searchTerm:'#REF!|#DIV/0!|#VALUE!|#NAME\\?',
      options:{useRegex:true,maxResults:10},maxChars:1500});
    await fs.writeFile(`${qa}/${b.key}-inspect.ndjson`,errors.ndjson);
    await (await SpreadsheetFile.exportXlsx(wb)).save(`${qa}/${b.key}-authored.xlsx`);
  }
  for (let i=0; mode !== 'delta' && i<b.views.length; i++) {
    const image = await wb.render({sheetName:mode === 'review' ? (b.reviewSheet ?? b.sheet) : b.sheet,
      range:b.views[i], scale:1,format:'png'});
    await fs.writeFile(`${qa}/${b.key}-${mode}-${i+1}.png`,new Uint8Array(await image.arrayBuffer()));
  }
  console.log(b.key,mode,'complete');
}
