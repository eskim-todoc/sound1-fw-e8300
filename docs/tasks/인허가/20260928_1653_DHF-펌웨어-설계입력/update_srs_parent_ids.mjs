// 상위 ID의 접두어만 바꾸고, 비교 시트의 삭제 원문은 보존한다.
import fs from 'node:fs/promises';
import {constants} from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {FileBlob, SpreadsheetFile} from '@oai/artifact-tool';

const task=path.dirname(fileURLToPath(import.meta.url));
const qa=path.join(task,'qa/srs-parent-id-20261012');
const target=path.resolve(task,'../../../참고/인허가/사운드1/DHF/02_설계 입력/SW/펌웨어/SRS-FW-TD24A 소프트웨어 요구사항 사양서_rev0(김은수 작성).xlsx');
const main='소프트웨어 입력 요구사항 사양서';
const compare=main+'_변경 비교';
const mode=process.argv[2];
await fs.mkdir(qa,{recursive:true});
if(mode==='prepare') await fs.copyFile(target,path.join(qa,'before.xlsx'),constants.COPYFILE_EXCL);
const workbook=await SpreadsheetFile.importXlsx(await FileBlob.load(path.join(qa,mode==='review'?'candidate.xlsx':'before.xlsx')));
if(mode==='prepare') {
  const changes=[];
  for(const name of [main,compare]) {
    const sheet=workbook.worksheets.getItem(name);
    const rows=sheet.getRange('A1:I250').values;
    rows.forEach((row,index)=>{
      if(name===compare && String(row[0]??'').trim()==='[-]') return;
      row.forEach((value,col)=>{
        if(typeof value!=='string' || !/SYS-RID-FW-\d+/.test(value)) return;
        if(col!==7) throw Error(`Unexpected parent-ID column: ${name} / ${col}`);
        changes.push({sheet:name,cell:`H${index+1}`,before:value,after:value.replace(/SYS-RID-FW-(\d+)/g,'SYS-RID-SP-FW-$1')});
      });
    });
  }
  if(changes.length!==98) throw Error(`Unexpected edit count ${changes.length}`);
  await fs.writeFile(path.join(qa,'plan.json'),JSON.stringify({target,changes},null,2));
  console.log(JSON.stringify({changes:changes.length,sheets:[main,compare]}));
} else if(mode==='build') {
  const plan=JSON.parse(await fs.readFile(path.join(qa,'plan.json'),'utf8'));
  for(const edit of plan.changes) workbook.worksheets.getItem(edit.sheet).getRange(edit.cell).values=[[edit.after]];
  workbook.recalculate();
  console.log((await workbook.inspect({kind:'region',sheetId:main,range:'G6:H8',maxChars:1800})).ndjson);
  console.log((await workbook.inspect({kind:'match',searchTerm:'#REF!|#DIV/0!|#VALUE!|#NAME\\?|#NUM!',options:{useRegex:true,maxResults:10},maxChars:1000})).ndjson);
  await(await SpreadsheetFile.exportXlsx(workbook)).save(path.join(qa,'authored.xlsx'));
}
if(mode==='prepare' || mode==='review') {
  for(const [label,sheetName,range] of [['main',main,'G4:I11'],['last',main,'G87:I92'],['compare',compare,'G11:I16']]) {
    const image=await workbook.render({sheetName,range,scale:1.5,format:'png'});
    await fs.writeFile(path.join(qa,`${mode}-${label}.png`),new Uint8Array(await image.arrayBuffer()));
  }
  console.log(mode,'previews complete');
}
