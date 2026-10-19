import fs from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
import {FileBlob,SpreadsheetFile} from '@oai/artifact-tool';
const task=fileURLToPath(new URL('./',import.meta.url));
const qa=`${task}/qa/system-cybersecurity-rev0`;
const mode=process.argv[2]??'before';
const path=mode==='review'?`${qa}/system-candidate.xlsx`:`${qa}/system-before.xlsx`;
const wb=await SpreadsheetFile.importXlsx(await FileBlob.load(path));
const main=wb.worksheets.getItem('보고서_시스템 시험');
console.log((await wb.inspect({kind:'sheet',include:'id,name',maxChars:2000})).ndjson);
if(mode==='before') {
  for(const [name,range] of [['main','B188:F194'],['security','J225:L229']]) {
    const p=await wb.render({sheetName:main.name,range,scale:1,format:'png'});
    await fs.writeFile(`${qa}/before-${name}.png`,new Uint8Array(await p.arrayBuffer()));
  }
} else if(mode==='build') {
  const p=JSON.parse(await fs.readFile(`${qa}/author-plan.json`,'utf8'));
  for(const c of p.changes) {
    main.getRange(c.cell).values=[[c.after]];
    if(c.height)main.getRange(`B${c.row}:L${c.row}`).format.rowHeight=c.height;
  }
  for(const spec of p.sheets) {
    const s=wb.worksheets.add(spec.name);
    s.getRange(`A1:${spec.lastCol}${spec.rows.length}`).values=spec.rows;
    s.getRange(`A1:${spec.lastCol}${spec.rows.length}`).format={font:{name:'맑은 고딕',size:10,color:'#000000'},wrapText:true,horizontalAlignment:'left',verticalAlignment:'top',borders:{preset:'all',style:'thin',color:'#000000'}};
    for(const [col,width] of Object.entries(spec.widths))s.getRange(`${col}1:${col}${spec.rows.length}`).format.columnWidth=width;
    for(const h of spec.heights)s.getRange(`A${h.row}:${spec.lastCol}${h.row}`).format.rowHeight=h.height;
    for(const m of spec.merges??[])s.mergeCells(m);
    for(const range of spec.grey??[])s.getRange(range).format.fill='#F5F5F5';
    for(const range of spec.red??[])s.getRange(range).format.font={name:'맑은 고딕',size:10,bold:true,color:'#C00000'};
    for(const range of spec.bold??[])s.getRange(range).format.font={name:'맑은 고딕',size:10,bold:true,color:'#000000'};
    s.getRange(`A4:${spec.lastCol}4`).format.horizontalAlignment='center';
    s.getRange(`A4:${spec.lastCol}4`).format.verticalAlignment='center';
    for(const col of ['A','B','F'])s.getRange(`${col}5:${col}${spec.rows.length}`).format.horizontalAlignment='center';
    s.showGridLines=false;s.freezePanes.freezeRows(spec.header??4);
  }
  wb.recalculate();
  console.log((await wb.inspect({kind:'match',searchTerm:'#REF!|#DIV/0!|#VALUE!|#NAME\\?|#NUM!',options:{useRegex:true,maxResults:20},maxChars:1200})).ndjson);
  await(await SpreadsheetFile.exportXlsx(wb)).save(`${qa}/system-authored.xlsx`);
} else if(mode==='review') {
  const p=JSON.parse(await fs.readFile(`${qa}/author-plan.json`,'utf8'));
  const views=[...p.views,
    {sheet:main.name,range:'K195:L198',name:'audit-notes'},
    {sheet:main.name,range:'K206:L209',name:'boot-notes'},
    {sheet:main.name,range:'K215:L216',name:'audit-error-notes'},
    {sheet:'사이버보안 시험 연계',range:'I18:L18',name:'timestamp'},
    {sheet:'사이버보안 시험 연계',range:'I22:L22',name:'aes-result'},
    {sheet:'사이버보안 시험 연계',range:'I37:L37',name:'recovery-result'}];
  for(const v of views) {
    const b=await wb.render({sheetName:v.sheet,range:v.range,scale:1,format:'png'});
    await fs.writeFile(`${qa}/after-${v.name}.png`,new Uint8Array(await b.arrayBuffer()));
  }
}
