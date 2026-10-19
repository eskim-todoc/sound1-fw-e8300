import fs from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
import {Workbook,SpreadsheetFile,FileBlob} from '@oai/artifact-tool';
const p=JSON.parse(await fs.readFile(new URL('confirmation-plan.json',import.meta.url),'utf8'));
const path=n=>fileURLToPath(new URL(n,import.meta.url));
if(process.argv[2]==='author'){
 const w=Workbook.create();
 for(const sn of [...new Set(p.changes.map(v=>v.sheet))]){
  const s=w.worksheets.add(sn);for(const c of p.changes.filter(v=>v.sheet===sn))s.getRange(c.cell).values=[[c.new]];
 }
 const co=w.worksheets.add('보고서_시스템 시험_비교');let h=p.headerRow;
 co.getRange(`B${h}:L${h}`).merge();co.getRange(`B${h}`).values=[['2026-10-16 사용자 확인 반영 — 시험 28·32']];co.getRange(`B${h}:L${h}`).format.rowHeight=28;
 co.getRange(`B${h+1}:D${h+1}`).values=[['구분','변경 위치','확인 근거']];co.getRange(`E${h+1}:L${h+1}`).merge();co.getRange(`E${h+1}`).values=[['변경 전후']];
 co.getRange(`B${h}:L${h+1}`).format.font={name:'맑은 고딕',size:11,bold:true};co.getRange(`B${h}:L${h+1}`).format.fill='#E7E6E6';
 for(const c of p.changes){let r=c.compareRow;co.getRange(`B${r}:D${r}`).values=[['시험28/32',`${c.sheet}!${c.cell}`,'사용자 답변(2026-10-16)']];co.getRange(`E${r}:L${r}`).merge();co.getRange(`E${r}`).values=[[`\n[-] ${c.old.trim()}\n[+] ${c.new.trim()}\n`]];
  const q=co.getRange(`B${r}:L${r}`);q.format.font={name:'맑은 고딕',size:10};q.format.wrapText=true;q.format.verticalAlignment='center';q.format.rowHeight=Math.min(409.5,Math.max(70,(c.old.trim()+'\n'+c.new.trim()).split('\n').length*15+30));q.format.borders={preset:'all',style:'thin',color:'#808080'};
 }
 w.recalculate();await(await SpreadsheetFile.exportXlsx(w)).save(path('confirmation-authored.xlsx'));console.log('authored',p.changes.length);
} else {
 const w=await SpreadsheetFile.importXlsx(await FileBlob.load(path('confirmation-after.xlsx')));
 for(const [name,range] of [['28','J158:L159'],['32','J169:L171']]){const png=await w.render({sheetName:'보고서_시스템 시험',range,scale:1});await fs.writeFile(path(`confirmation-${name}.png`),new Uint8Array(await png.arrayBuffer()));}
}
