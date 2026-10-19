import fs from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
import {Workbook,SpreadsheetFile,FileBlob} from '@oai/artifact-tool';
const path=n=>fileURLToPath(new URL(n,import.meta.url));
const p=JSON.parse(await fs.readFile(path('style-plan.json'),'utf8'));
if(process.argv[2]==='author'){
 const w=Workbook.create();
 for(const sn of [...new Set(p.changes.map(v=>v.sheet))]){
  const s=w.worksheets.add(sn);
  for(const c of p.changes.filter(v=>v.sheet===sn))s.getRange(c.cell).values=[[c.new]];
  if(sn==='보고서_시스템 시험'){
   s.getRange('A34:L293').format.font={size:10};
   for(const [r,ht] of Object.entries(p.heights))s.getRange(`A${r}:L${r}`).format.rowHeight=ht;
  }
 }
 let co=w.worksheets.getItem('보고서_시스템 시험_비교'),h=p.headerRow;
 co.getRange(`B${h}:L${h}`).merge();co.getRange(`B${h}`).values=[['2026-10-16 확인 완료 시험 내용 및 표시 정리']];
 co.getRange(`B${h}:L${h+1}`).format.font={name:'맑은 고딕',size:10,bold:true};co.getRange(`B${h}:L${h+1}`).format.fill='#E7E6E6';
 co.getRange(`B${h+1}:D${h+1}`).merge();co.getRange(`B${h+1}`).values=[['변경 위치']];co.getRange(`E${h+1}:L${h+1}`).merge();co.getRange(`E${h+1}`).values=[['변경 전후']];
 for(const c of p.contentChanges){let r=c.compareRow;co.getRange(`B${r}:D${r}`).merge();co.getRange(`B${r}`).values=[[`${c.sheet}!${c.cell}`]];co.getRange(`E${r}:L${r}`).merge();co.getRange(`E${r}`).values=[[`[-] ${c.old.trim()}\n[+] ${c.new.trim()||'(삭제)'}`]];
  let rg=co.getRange(`B${r}:L${r}`);rg.format.font={name:'맑은 고딕',size:10};rg.format.wrapText=true;rg.format.verticalAlignment='center';rg.format.rowHeight=Math.min(409.5,Math.max(50,(c.old.trim()+'\n'+c.new.trim()).split('\n').length*14+10));rg.format.borders={preset:'all',style:'thin',color:'#808080'};
 }
 let r=p.lastRow;co.getRange(`B${r}:D${r}`).merge();co.getRange(`B${r}`).values=[['서식 적용 범위']];co.getRange(`E${r}:L${r}`).merge();co.getRange(`E${r}`).values=[['보고서_시스템 시험 34~293행: 10pt, 위/아래 빈 줄 1개. 그 외 셀과 시트: 위/아래 빈 줄 제거. 핑크 시험의 녹색 밑줄은 검정/밑줄 없음. 사진 높이 8cm 및 순서 유지, 행 높이와 사진 위치 조정.']];co.getRange(`B${r}:L${r}`).format.font={name:'맑은 고딕',size:10};co.getRange(`B${r}:L${r}`).format.wrapText=true;co.getRange(`B${r}:L${r}`).format.rowHeight=60;
 w.recalculate();
 console.log((await w.inspect({kind:'table',range:'보고서_시스템 시험!J158:L159',include:'values,formulas',tableMaxRows:2,tableMaxCols:3})).ndjson);
 await(await SpreadsheetFile.exportXlsx(w)).save(path('style-authored.xlsx'));
 console.log('authored',p.changes.length);
}else{
 const w=await SpreadsheetFile.importXlsx(await FileBlob.load(path('style-after.xlsx')));
 const image=await w.render({sheetName:'보고서_시스템 시험',range:'J34:L38',scale:1});await fs.writeFile(path('style-artifact-preview.png'),new Uint8Array(await image.arrayBuffer()));console.log('rendered');
}
