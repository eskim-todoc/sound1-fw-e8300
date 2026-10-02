import fs from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
import {FileBlob,Workbook,SpreadsheetFile} from '@oai/artifact-tool';
const q=new URL('./qa/suvc-practical-criteria/',import.meta.url);
const mode=process.argv[2];
if(mode==='before'||mode==='review'){
  const path=fileURLToPath(new URL(mode==='before'?'before.xlsx':'candidate.xlsx',q));
  const w=await SpreadsheetFile.importXlsx(await FileBlob.load(path));
  console.log((await w.inspect({kind:'table',range:"'기준서_유닛 검증'!B28:K32",include:'values',tableMaxRows:5,tableMaxCols:10})).ndjson);
  if(mode==='review')w.recalculate();
  const views=mode==='before'?[
    ['before-main','기준서_유닛 검증','B28:K32']
  ]:[
    ['final-main','기준서_유닛 검증','B28:K33'],
    ['final-diff','기준서_유닛 검증_변경 비교','A28:K33'],
    ['final-equipment','기준서_유닛 검증','B13:K24'],
    ['final-vector','기준서_유닛 검증','D46:K49'],
    ['final-list','유닛 목록','A1:G5']
  ];
  for(const [file,sheetName,range] of views){
    const b=await w.render({sheetName,range,scale:1.4,format:'png'});
    await fs.writeFile(new URL(file+'.png',q),new Uint8Array(await b.arrayBuffer()));
  }
  console.log((await w.inspect({kind:'match',searchTerm:'#REF!|#DIV/0!|#VALUE!|#NAME\\?',options:{useRegex:true,maxResults:20},summary:'formula error scan'})).ndjson);
}else{
  const p=JSON.parse(await fs.readFile(new URL('plan.json',q),'utf8'));
  const w=Workbook.create();
  for(const [name,data] of [['본문',p.physical],['비교',p.diffphysical],['준비',p.header]]){
    const s=w.worksheets.add(name);const cols=data[0].length;
    const r=s.getRange(`A1:${String.fromCharCode(64+cols)}${data.length}`);r.values=data;
    r.format.font.name='맑은 고딕';r.format.font.size=11;r.format.wrapText=true;
  }
  w.recalculate();
  console.log((await w.inspect({kind:'table',range:'본문!B1:K2',include:'values',tableMaxRows:2,tableMaxCols:10})).ndjson);
  console.log((await w.inspect({kind:'match',searchTerm:'#REF!|#DIV/0!|#VALUE!|#NAME\\?',options:{useRegex:true,maxResults:20}})).ndjson);
  await (await SpreadsheetFile.exportXlsx(w)).save(fileURLToPath(new URL('authored.xlsx',q)));
  console.log('115 unit criteria authored with Artifact Tool');
}
