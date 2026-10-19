import fs from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
import {FileBlob, SpreadsheetFile, Workbook} from '@oai/artifact-tool';
const dir = new URL('.', import.meta.url).pathname.replace(/^\/([A-Za-z]:)/, '$1');
const prefix='G:/공유 드라이브/20_프로젝트별/01_project sound1/08_DHF/04_설계 검증/SW/외부기/';
const mode=process.argv[2] || 'inspect';
if(mode==='inspect') {
  for(const [key,name] of [['personal','[김은수 전용] SSTR-FW-TD24A 소프트웨어 시스템 시험 보고서.xlsx'],['qcc','[E8300-QCC] SSTR-FW-TD24A 소프트웨어 시스템 시험 보고서.xlsx']]) {
    const wb=await SpreadsheetFile.importXlsx(await FileBlob.load(prefix+name));
    console.log(key,(await wb.inspect({kind:'sheet',include:'id,name',maxChars:2500})).ndjson);
    if(key==='qcc') console.log(wb.help('range.richText', {include:'index,examples,notes',maxChars:2500}).ndjson);
    const png=await wb.render({sheetName:'보고서_시스템 시험',range:key==='personal'?'J34:L38':'J34:L37',scale:1});
    await fs.writeFile(new URL(`${key}-before.png`,import.meta.url),new Uint8Array(await png.arrayBuffer()));
  }
}
if(mode==='author') {
  const plan=JSON.parse(await fs.readFile(new URL('plan.json',import.meta.url),'utf8'));
  const wb=Workbook.create();const sh=wb.worksheets.add('보고서_시스템 시험');const co=wb.worksheets.add('보고서_시스템 시험_비교');
  for(const sheet of [sh,co]) {
    for(const [col,width] of Object.entries({A:2.875,B:27,C:54.625,D:6,E:24,F:24,G:42,H:32,I:48,J:64.625,K:70,L:13.875})) sheet.getRange(`${col}1`).format.columnWidth=width;
  }
  for(const r of plan.rows) sh.getRange(`A${r.new}:L${r.new}`).format.rowHeight=r.height;
  for(const entry of Object.values(plan.changes)) sh.getRange(entry.newAddress).values=[[entry.newText]];
  for(const [sheet,items] of Object.entries(plan.otherChanges||{})) {
    const extra=wb.worksheets.add(sheet);
    for(const entry of Object.values(items))extra.getRange(entry.newAddress).values=[[entry.newText]];
  }
  for(const p of plan.photos) {
    const bytes=await fs.readFile(p.path);
    sh.images.add({dataUrl:`data:image/jpeg;base64,${bytes.toString('base64')}`,anchor:{from:{row:p.row-1,col:10,rowOffsetPx:p.offsetPx,colOffsetPx:8},extent:{widthPx:p.widthPx,heightPx:p.heightPx}}});
  }
  co.getRange('B305:L305').merge();co.getRange('B305').values=[['2026-10-16 자료첨부 반영 비교']];
  co.getRange('B305:L305').format.font={name:'맑은 고딕',size:12,bold:true};co.getRange('B305:L305').format.rowHeight=28;
  co.getRange('B306:D306').values=[['시험 ID','수정 항목과 기존 셀','반영 셀']];co.getRange('E306:L306').merge();co.getRange('E306').values=[['변경 전후']];co.getRange('B306:L306').format.rowHeight=25;
  co.getRange('B306:L306').format.fill='#E7E6E6';co.getRange('B306:L306').format.font={name:'맑은 고딕',size:10,bold:true};
  for(const c of plan.comparisons) {
    const row=c.row;co.getRange(`B${row}:D${row}`).values=[[c.test?`SW-FW-SYS-Test-ID-${c.test}`:'관련 자료 정리',`${c.reason}\n기존 ${c.beforeCell}`,c.afterCell]];
    co.getRange(`E${row}:L${row}`).merge();co.getRange(`E${row}`).values=[[`\n[-] ${c.old.trim()}\n[+] ${c.new.trim()||'(삭제)'}\n`]];
    const range=co.getRange(`B${row}:L${row}`);range.format.font={name:'맑은 고딕',size:10};range.format.wrapText=true;range.format.verticalAlignment='center';
    range.format.rowHeight=Math.min(409.5,Math.max(55,((c.old.trim()+'\n'+c.new.trim()).split('\n').length+2)*15));
    range.format.borders={preset:'all',style:'thin',color:'#808080'};
  }
  wb.recalculate();
  console.log((await wb.inspect({kind:'match',searchTerm:'#REF!|#DIV/0!|#VALUE!',options:{useRegex:true,maxResults:5},maxChars:2000})).ndjson);
  const x=await SpreadsheetFile.exportXlsx(wb);await x.save(fileURLToPath(new URL('authored.xlsx',import.meta.url)));
  console.log(JSON.stringify({photos:plan.photos.length,changes:Object.keys(plan.changes).length,comparisons:plan.comparisons.length}));
}
if(mode==='render') {
  const wb=await SpreadsheetFile.importXlsx(await FileBlob.load(fileURLToPath(new URL('candidate.xlsx',import.meta.url))));
  const plan=JSON.parse(await fs.readFile(new URL('plan.json',import.meta.url),'utf8'));
  for(const [key,start,end] of [['first',34,40],['test10',54,62],['partial',81,81],['last',282,285]]) {
    const a=plan.rowMap[String(start)]?.first,b=plan.rowMap[String(end)]?.last;
    if(!a||!b)continue;
    const png=await wb.render({sheetName:'보고서_시스템 시험',range:`J${a}:L${b}`,scale:1});
    await fs.writeFile(new URL(`after-${key}.png`,import.meta.url),new Uint8Array(await png.arrayBuffer()));
  }
}
