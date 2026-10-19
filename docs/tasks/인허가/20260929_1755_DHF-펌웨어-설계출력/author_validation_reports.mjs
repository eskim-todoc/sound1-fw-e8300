import fs from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
import {FileBlob,SpreadsheetFile} from '@oai/artifact-tool';

const task=fileURLToPath(new URL('./',import.meta.url));
const qa=`${task}/qa/validation-reports`;
const mode=process.argv[2]??'before';
const kind=process.argv[3]??'integration';
const sources=JSON.parse(await fs.readFile(`${qa}/sources.json`,'utf8'));
const reference=JSON.parse(await fs.readFile(`${qa}/${kind}-reference.json`,'utf8'));
const reportName=kind==='integration'?'보고서_통합시험':'보고서_시스템 시험';
const candidate=`${qa}/${kind}-candidate.xlsx`;
const wb=await SpreadsheetFile.importXlsx(await FileBlob.load(mode==='review'?candidate:sources[`${kind}-reference`].path));
const sheet=wb.worksheets.getItem(reportName);
const font='맑은 고딕',red='#C00000';
const put=(sh,cell,value)=>{sh.getRange(cell).values=[[value]];};
console.log((await wb.inspect({kind:'sheet',include:'id,name',maxChars:1500})).ndjson);
if(mode==='help'){
  console.log(await wb.help('print area and page setup'));
  process.exit(0);
}

if(mode==='before'){
  const views=kind==='integration'?[['front','B5:I24'],['table','I27:O30']]:[['front','B5:G31'],['table','G34:L38']];
  views.unshift(['cover','B2:O24']);
  for(const [label,range] of views){
    const png=await wb.render({sheetName:label==='cover'?'표지':reportName,range,scale:1,format:'png'});
    await fs.writeFile(`${qa}/${kind}-before-${label}.png`,new Uint8Array(await png.arrayBuffer()));
  }
}else{
  const plan=JSON.parse(await fs.readFile(`${qa}/${kind}-plan.json`,'utf8'));
  if(mode==='build'){
    const cover=wb.worksheets.getItem('표지');
    cover.deleteAllDrawings();
    put(cover,'L2',`${kind==='integration'?'SITR':'SSTR'}-FW-TD24A`);
    put(cover,'L3','Rev.0');
    put(cover,'B5',`소프트웨어 ${kind==='integration'?'통합시험':'시스템 시험'} 보고서\n(TD24A-A-EXT)`);
    put(cover,'D12','수석연구원');put(cover,'H12','김은수');
    for(const cell of ['J12','M12','D14','H14','J14','M14','D16','H16','J16','M16','D24'])put(cover,cell,null);
    put(cover,'B24',0);put(cover,'F24','최초 작성');
    cover.getRange('B25:O34').clear({applyTo:'contents'});
    sheet.deleteAllDrawings();
    for(const m of reference[reportName].merges)if(Number(m.match(/\d+/)[0])>=plan.firstRow)sheet.unmergeCells(m);
    const oldLast=reference[reportName].rows.length, end=Math.max(oldLast,plan.lastRow);
    sheet.getRange(`B${plan.firstRow}:${plan.lastCol}${end}`).clear({applyTo:'all'});
    sheet.getRange(`B${plan.firstRow}:${plan.lastCol}${end}`).formulas=Array.from({length:end-plan.firstRow+1},()=>Array(plan.columnCount).fill(''));
    const body=sheet.getRange(`B${plan.firstRow}:${plan.lastCol}${plan.bodyEnd}`);
    body.values=plan.rows;
    body.format={font:{name:font,size:10,color:'#111111'},wrapText:true,verticalAlignment:'top',horizontalAlignment:'left',borders:{preset:'all',style:'thin',color:'#B7B7B7'}};
    plan.widths.forEach((width,i)=>{sheet.getRangeByIndexes(0,i+1,end,1).format.columnWidthPx=width;});
    for(const [cell,value] of Object.entries(plan.front))put(sheet,cell,value);
    for(const m of plan.frontMerges)sheet.mergeCells(m);
    for(const spec of plan.frontFormats)sheet.getRange(spec.range).format=spec.format;
    for(const item of plan.meta){
      sheet.getRange(`B${item.row}:${plan.lastCol}${item.row}`).format.rowHeight=item.height;
      if(item.kind==='image'||item.pending)sheet.getRange(`${plan.actualCol}${item.row}`).format.font.color=red;
      if(item.kind==='log')sheet.getRange(`${plan.expectedCol}${item.row}:${plan.actualCol}${item.row}`).format.font.name='Consolas';
    }
    for(const m of plan.merges)sheet.mergeCells(m);
    for(const test of plan.tests){
      sheet.getRange(`${plan.idCol}${test.start}:${plan.lastCol}${test.start}`).format.borders={top:{style:'medium',color:'#687784'}};
      sheet.getRange(`${plan.lastCol}${test.start}`).format.horizontalAlignment='center';
    }
    sheet.getRange(`${plan.lastCol}${plan.firstRow}:${plan.lastCol}${plan.bodyEnd}`).conditionalFormats.add('containsText',{text:'필요',format:{font:{color:red,bold:true},fill:'#FFF2F2'}});
    for(const test of plan.tests)sheet.getRange(`${plan.lastCol}${test.start}`).dataValidation={rule:{type:'list',values:['통과','실패','결과 기입 필요','결과 대조 필요','QCC 결과 필요']}};
    for(const image of plan.images){
      const bytes=await fs.readFile(image.path);
      sheet.images.add({dataUrl:`data:image/png;base64,${bytes.toString('base64')}`,anchor:{from:{row:image.row-1,col:plan.expectedIndex,rowOffsetPx:6,colOffsetPx:6},extent:{widthPx:600,heightPx:371}}});
    }
    for(const [cell,value] of Object.entries(plan.footer))put(sheet,cell,value);
    for(const m of plan.footerMerges)sheet.mergeCells(m);
    for(const spec of plan.footerFormats)sheet.getRange(spec.range).format=spec.format;
    for(const [cell,formula] of Object.entries(plan.formulas))sheet.getRange(cell).formulas=[[formula]];
    sheet.freezePanes.unfreeze();sheet.showGridLines=false;
    wb.recalculate();
    console.log((await wb.inspect({kind:'region',sheetId:reportName,range:plan.summaryRange,maxChars:1200})).ndjson);
    console.log((await wb.inspect({kind:'match',searchTerm:'#REF!|#DIV/0!|#VALUE!|#NAME\\?|#NUM!',options:{useRegex:true,maxResults:30},maxChars:2000})).ndjson);
    await(await SpreadsheetFile.exportXlsx(wb)).save(candidate);
  }
  for(const view of plan.views){
    const png=await wb.render({sheetName:view.sheet??reportName,range:view.range,scale:1,format:'png'});
    await fs.writeFile(`${qa}/${kind}-${view.name}.png`,new Uint8Array(await png.arrayBuffer()));
  }
  console.log(JSON.stringify({kind,tests:plan.tests.length,images:plan.images.length,rows:plan.bodyEnd,output:candidate}));
}
