import fs from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
import {FileBlob,Workbook,SpreadsheetFile} from '@oai/artifact-tool';
const q=new URL('./qa/sitc-current-design/',import.meta.url);
const mode=process.argv[2];
await fs.mkdir(q,{recursive:true});
if(mode==='before'||mode==='review'){
  const path=mode==='before'?fileURLToPath(new URL('../../../../../참고/인허가/사운드1/DHF/03_설계 출력/SW/펌웨어/SITC-FW-TD24A 소프트웨어 통합시험 기준서.xlsx',q)):fileURLToPath(new URL('candidate.xlsx',q));
  const w=await SpreadsheetFile.importXlsx(await FileBlob.load(path));
  if(mode==='review')w.recalculate();
  console.log((await w.inspect({kind:'table',range:"'기준서_통합 시험'!C25:M26",include:'values,formulas',tableMaxRows:2,tableMaxCols:11})).ndjson);
  for(const [name,sheetName,range] of [[mode+'-main','기준서_통합 시험','B23:M27'],[mode+'-diff','기준서_통합 시험_변경 비교','A23:M28']]){
    const b=await w.render({sheetName,range,scale:1.2,format:'png'});
    await fs.writeFile(new URL(name+'.png',q),new Uint8Array(await b.arrayBuffer()));
  }
  console.log((await w.inspect({kind:'match',searchTerm:'#REF!|#DIV/0!|#VALUE!|#NAME\\?',options:{useRegex:true,maxResults:20}})).ndjson);
}else{
  const p=JSON.parse(await fs.readFile(new URL('plan.json',q),'utf8'));
  const w=Workbook.create();
  for(const [name,data] of [['본문',p.rows],['비교',p.diffrows],['준비',p.header]]){
    const s=w.worksheets.add(name),c=data[0].length;
    const r=s.getRange(`A1:${String.fromCharCode(64+c)}${data.length}`);r.values=data;
    r.format.font.name='맑은 고딕';r.format.font.size=11;r.format.wrapText=true;
  }
  w.recalculate();
  console.log((await w.inspect({kind:'table',range:'본문!I1:M2',include:'values',tableMaxRows:2,tableMaxCols:5})).ndjson);
  console.log((await w.inspect({kind:'match',searchTerm:'#REF!|#DIV/0!|#VALUE!|#NAME\\?',options:{useRegex:true,maxResults:20}})).ndjson);
  await (await SpreadsheetFile.exportXlsx(w)).save(fileURLToPath(new URL('authored.xlsx',q)));
  console.log('17 integration criteria authored with Artifact Tool');
}
