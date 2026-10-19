import fs from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
import {FileBlob,SpreadsheetFile} from '@oai/artifact-tool';
const task=fileURLToPath(new URL('./',import.meta.url));
const qa=`${task}/qa/integration-system-rev0`;
const baseline=JSON.parse(await fs.readFile(`${qa}/baseline.json`,'utf8'));
const mode=process.argv[2]??'before';
const key=process.argv[3]??'unit';
const book=baseline.books[key];
const reportKey=key.replace('Criterion','');
const sourceFile=mode==='review'?`${qa}/${key}-candidate.xlsx`:(key.endsWith('Criterion')?`${qa}/${reportKey}-candidate.xlsx`:book.backup);
const wb=await SpreadsheetFile.importXlsx(await FileBlob.load(sourceFile));
const main=Object.keys(book.sheets).find(n=>/^(보고서_|기준서_)/.test(n));
console.log((await wb.inspect({kind:'sheet',include:'id,name',maxChars:1800})).ndjson);
if(mode==='before') {
  const views=key==='unit'?['J30:M31','D30:I31']:key==='integration'?['M27:O29','B23:I26']:['J34:L36','B32:F35'];
  for(let i=0;i<views.length;i++) {
    const png=await wb.render({sheetName:main,range:views[i],scale:1,format:'png'});
    await fs.writeFile(`${qa}/${key}-before-${i}.png`,new Uint8Array(await png.arrayBuffer()));
  }
} else if(mode==='build' && key.endsWith('Criterion')) {
  const reportMain=Object.keys(baseline.books[reportKey].sheets).find(n=>n.startsWith('보고서_'));
  const s=wb.worksheets.getItem(reportMain);
  const p=JSON.parse(await fs.readFile(`${qa}/${reportKey}-plan.json`,'utf8'));
  const firstDelete=reportKey==='integration'?'N':'K';
  const lastDelete=reportKey==='integration'?'O':'L';
  // 삭제할 두 열의 셀과 병합을 먼저 Artifact Tool에서 제거한다.
  // 원본 기준서의 표지/이력과 실제 열 구조 보존은 native 패키지 단계에서 처리한다.
  const nativePlan=JSON.parse(await fs.readFile(`${qa}/${key}-plan.json`,'utf8'));
  for(const m of nativePlan.unmerges)s.unmergeCells(m);
  s.getRange(`${firstDelete}1:${lastDelete}${p.last}`).clear({applyTo:'all'});
  s.getRange('B2').values=[[nativePlan.title]];
  wb.recalculate();
  console.log((await wb.inspect({kind:'match',searchTerm:'#REF!|#DIV/0!|#VALUE!|#NAME\\?|#NUM!',options:{useRegex:true,maxResults:12},maxChars:1200})).ndjson);
  await(await SpreadsheetFile.exportXlsx(wb)).save(`${qa}/${key}-authored.xlsx`);
} else if(mode==='build' && key==='integration') {
  const p=JSON.parse(await fs.readFile(`${qa}/integration-plan.json`,'utf8'));
  const s=wb.worksheets.getItem(main);
  const old=book.sheets[main];
  for(const m of old.merges)if(Number(m.match(/\d+/)[0])>=p.first)s.unmergeCells(m);
  s.getRange(`B${p.first}:O${Math.max(old.rows,p.last)}`).clear({applyTo:'all'});
  const body=s.getRange(`B${p.first}:O${p.last}`);
  body.values=p.rows;
  body.format={font:{name:'Calibri',size:11,color:'#000000',bold:false},fill:'#FFFFFF',wrapText:true,
    horizontalAlignment:'left',verticalAlignment:'top',borders:{preset:'all',style:'thin',color:'#000000'}};
  s.getRange(`N${p.first}:O${p.last}`).format.font={name:'맑은 고딕',size:10,color:'#000000',bold:false};
  for(const [col,width] of Object.entries(p.widths))s.getRange(`${col}1:${col}${p.last}`).format.columnWidth=width;
  for(const h of p.heights)s.getRange(`B${h.row}:O${h.row}`).format.rowHeight=h.height;
  for(const t of p.tests){
    if(t.id%2===0)s.getRange(`B${t.start}:O${t.end}`).format.fill='#F5F5F5';
    for(const col of ['B','C','D','F','I','O'])s.getRange(`${col}${t.start}:${col}${t.end}`).format.horizontalAlignment='center';
  }
  for(const m of p.merges)s.mergeCells(m);
  for(const [a,v] of Object.entries(p.front)){s.getRange(a).values=[[v]];s.getRange(a).format.font={name:'맑은 고딕',size:10,bold:false,color:'#000000'};}
  s.getRange('C19:M19').format.font={name:'맑은 고딕',size:10,bold:true,color:'#C00000'};
  s.showGridLines=false;
  s.freezePanes.freezeRows(24);
  wb.recalculate();
  console.log((await wb.inspect({kind:'match',searchTerm:'#REF!|#DIV/0!|#VALUE!|#NAME\\?|#NUM!',options:{useRegex:true,maxResults:12},maxChars:1400})).ndjson);
  await(await SpreadsheetFile.exportXlsx(wb)).save(`${qa}/${key}-authored.xlsx`);
} else if(mode==='build' && key==='system') {
  const p=JSON.parse(await fs.readFile(`${qa}/system-plan.json`,'utf8'));
  const s=wb.worksheets.getItem(main);
  for(const m of book.sheets[main].merges)if(Number(m.match(/\d+/)[0])>=p.first)s.unmergeCells(m);
  s.getRange(`B${p.first}:L${Math.max(p.last,book.sheets[main].rows)}`).clear({applyTo:'all'});
  s.getRange(`B${p.first}:L${p.last}`).values=p.rows;
  s.getRange(`B${p.first}:L${p.bodyLast}`).format={font:{name:'Calibri',size:11,bold:false,color:'#000000'},fill:'#FFFFFF',wrapText:true,
    horizontalAlignment:'left',verticalAlignment:'top',borders:{preset:'all',style:'thin',color:'#000000'}};
  s.getRange(`K${p.first}:L${p.bodyLast}`).format.font={name:'맑은 고딕',size:10,bold:false,color:'#000000'};
  for(const [col,width] of Object.entries(p.widths))s.getRange(`${col}1:${col}${p.last}`).format.columnWidth=width;
  for(const h of p.heights)s.getRange(`B${h.row}:L${h.row}`).format.rowHeight=h.height;
  for(const t of p.tests){
    if(t.id%2===0)s.getRange(`B${t.start}:L${t.end}`).format.fill='#F5F5F5';
    for(const c of ['B','D','E','F','L'])s.getRange(`${c}${t.start}:${c}${t.end}`).format.horizontalAlignment='center';
    for(const c of t.cases){
      for(let r=c.start;r<c.end;r++)s.getRange(`J${r}:K${r}`).format.borders={bottom:{style:'thin',color:'#FFFFFF'}};
      for(let r=c.start+1;r<=c.end;r++)s.getRange(`J${r}:K${r}`).format.borders={top:{style:'thin',color:'#FFFFFF'}};
    }
  }
  for(const m of p.merges)s.mergeCells(m);
  for(const [a,v] of Object.entries(p.front)){s.getRange(a).values=[[v]];s.getRange(a).format.font={name:'맑은 고딕',size:10,bold:false,color:'#000000'};}
  for(const c of p.helperChanges){
    const sh=wb.worksheets.getItem(c.sheet);sh.getRange(c.cell).values=[[c.after]];
    sh.getRange(c.cell).format.font={name:'맑은 고딕',size:10,bold:false,color:'#000000'};
  }
  for(const h of p.helperHeights??[])wb.worksheets.getItem('시험 보조자료').getRange(`A${h.row}:D${h.row}`).format.rowHeight=h.height;
  s.showGridLines=false;s.freezePanes.freezeRows(33);
  wb.recalculate();
  console.log((await wb.inspect({kind:'match',searchTerm:'#REF!|#DIV/0!|#VALUE!|#NAME\\?|#NUM!',options:{useRegex:true,maxResults:12},maxChars:1400})).ndjson);
  await(await SpreadsheetFile.exportXlsx(wb)).save(`${qa}/${key}-authored.xlsx`);
} else if(mode==='review') {
  const p=JSON.parse(await fs.readFile(`${qa}/${key}-plan.json`,'utf8'));
  for(const v of p.views){
    const png=await wb.render({sheetName:v.sheet,range:v.range,scale:1,format:'png'});
    await fs.writeFile(`${qa}/${key}-after-${v.name}.png`,new Uint8Array(await png.arrayBuffer()));
  }
}
