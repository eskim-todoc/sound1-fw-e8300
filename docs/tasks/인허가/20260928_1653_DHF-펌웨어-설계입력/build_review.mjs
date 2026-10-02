import fs from 'node:fs/promises';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {FileBlob,SpreadsheetFile,Workbook} from '@oai/artifact-tool';
const dir=path.join(path.dirname(fileURLToPath(import.meta.url)),'review-20260929');
const jobs=JSON.parse(await fs.readFile(path.join(dir,'plan.json'),'utf8'));
for(const j of jobs){
 const original=await SpreadsheetFile.importXlsx(await FileBlob.load(j.backup));
 const body=original.worksheets.getItemAt(1);
 const before=await original.render({sheetName:body.name,range:j.kind==='SRS'?'F6:H9':'C25:F26',scale:1});
 await fs.writeFile(path.join(dir,j.kind+'-before.png'),new Uint8Array(await before.arrayBuffer()));
 // Author requested fills on imported data; native merge below preserves other workbook objects.
 for(const c of j.colors)original.worksheets.getItem(c.sheet).getRange(c.cell).format.fill='#'+c.color;
 original.recalculate();
 await(await SpreadsheetFile.exportXlsx(original)).save(path.join(dir,j.kind+'-colors.xlsx'));
 const w=Workbook.create(); const s=w.worksheets.add('변경 비교');
 s.showGridLines=false;s.freezePanes.freezeRows(7);
 const n=j.records.length+7;
 s.getRange(`A1:G${n}`).format.font={name:'맑은 고딕',size:10};
 const widths=[12,30,24,52,52,28,34];
 widths.forEach((v,i)=>s.getRange(`${String.fromCharCode(65+i)}1:${String.fromCharCode(65+i)}${n}`).format.columnWidth=v);
 s.getRange('A2:G2').merge();s.getRange('A2').values=[[j.kind+' 변경 비교']];s.getRange('A2').format.font={name:'맑은 고딕',size:16,bold:true};
 s.getRange('A3:G3').merge();s.getRange('A3').values=[['비교 원본: '+path.basename(j.source)]];
 s.getRange('A4:G4').merge();s.getRange('A4').values=[['[+] 추가: 초록 / [~] 수정: 노랑 / [-] 삭제: 빨강. 본문에는 추가/수정 셀만 색상을 표시합니다.']];
 s.getRange('A5:G5').merge();s.getRange('A5').values=[['2026-09-29 비교본. 위치를 누르면 현재 본문으로 이동합니다. 이후 편집 내용은 자동 반영되지 않습니다.']];
 s.getRange('A6:G6').merge();s.getRange('A6').values=[['SSTC의 기존 93행 이후 내용은 12행 아래로 이동하여 대조했습니다. 셀 단위 변경 건수이며 요구사항 개수와 다릅니다.']];
 s.getRange('A3:G6').format.rowHeight=24;
 s.getRange('A7:G7').values=[['구분','현재 위치','요구사항/항목','[-] 변경 전','[+] 변경 후','원본 위치','변경 이유']];
 s.getRange('A7:G7').format={fill:'#34495E',font:{name:'맑은 고딕',size:10,bold:true,color:'#FFFFFF'},rowHeight:28};
 const text=v=>v===null?'(빈칸)':typeof v==='string'&&v.startsWith('=')?"'"+v:String(v);
 s.getRange(`A8:G${n}`).values=j.records.map(r=>[r.status,r.sheet+'!'+r.cell,r.id,text(r.old),text(r.new),r.source,r.reason]);
 s.getRange(`A8:G${n}`).format.wrapText=true;s.getRange(`A8:G${n}`).format.verticalAlignment='top';
 j.records.forEach((r,i)=>{
  const row=i+8, fill=r.status.startsWith('[+]')?'#E2F0D9':r.status.startsWith('[-]')?'#FCE4D6':'#FFF2CC';
  s.getRange(`A${row}`).format.fill=fill;
  if(r.status.startsWith('[~]'))s.getRange(`D${row}:E${row}`).format.fill=fill;
  else s.getRange(`${r.status.startsWith('[+]')?'E':'D'}${row}`).format.fill=fill;
  const vals=[r.status,r.sheet+'!'+r.cell,r.id,text(r.old),text(r.new),r.source,r.reason];
  let lines=1;vals.forEach((v,k)=>{lines=Math.max(lines,String(v).split('\n').reduce((a,l)=>a+Math.max(1,Math.ceil([...l].reduce((x,c)=>x+(c.charCodeAt(0)>127?2:1),0)/(widths[k]-2))),0));});
  s.getRange(`A${row}:G${row}`).format.rowHeight=Math.min(400,Math.max(36,lines*15+10));
 });
 w.recalculate();
 console.log((await w.inspect({kind:'table',range:'변경 비교!A7:C10',tableMaxRows:4,tableMaxCols:3,maxChars:1200})).ndjson);
 const preview=await w.render({sheetName:'변경 비교',range:'A7:G12',scale:1});
 await fs.writeFile(path.join(dir,j.kind+'-review.png'),new Uint8Array(await preview.arrayBuffer()));
 await(await SpreadsheetFile.exportXlsx(w)).save(path.join(dir,j.kind+'-review.xlsx'));
}
