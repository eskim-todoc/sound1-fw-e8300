// 기존 시험 문서의 셀만 수정한다. 도형/병합/서식 보존은 패키지 결합 단계에서 확인한다.
import fs from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
import {FileBlob, SpreadsheetFile} from '@oai/artifact-tool';
const task=fileURLToPath(new URL('./',import.meta.url));
const qa=`${task}/qa/evidence-collection`;
const baseline=JSON.parse(await fs.readFile(`${qa}/baseline.json`,'utf8'));
const mode=process.argv[2]??'before';
const keys=process.argv.slice(3);
const preview={
  'unit-criterion':['기준서_유닛 검증','H49:K49'],
  'unit-report':['보고서_유닛 시험','K49:M49'],
  'integration-criterion':['기준서_통합 시험','I31:M31'],
  'integration-report':['보고서_통합시험','M31:O31'],
  'system-input':['기준서_시스템 시험','E34:J34'],
  'system-criterion':['기준서_시스템 시험','G40:J40'],
  'system-report':['보고서_시스템 시험','J40:L40'],
};
const plan=mode==='before'?[]:JSON.parse(await fs.readFile(`${qa}/patch-plan.json`,'utf8'));
for(const b of baseline.books.filter(b=>!keys.length||keys.includes(b.key))){
  if(mode!=='before'&&!plan.some(p=>p.key===b.key))continue;
  const input=mode==='review'?`${qa}/${b.key}-candidate.xlsx`:b.backup;
  const wb=await SpreadsheetFile.importXlsx(await FileBlob.load(input));
  if(mode==='build'){
    for(const p of plan.filter(p=>p.key===b.key)){
      const sheet=wb.worksheets.getItem(p.sheet??b.sheet);
      sheet.getRange(p.cell).values=[[p.value]];
      if(p.height)sheet.getRange(p.cell).format.rowHeight=p.height;
      if(p.red)sheet.getRange(p.cell).format.font.color='#C00000';
    }
    wb.recalculate();
    console.log(b.key,(await wb.inspect({kind:'match',searchTerm:'#REF!|#DIV/0!|#VALUE!|#NAME\\?',options:{useRegex:true,maxResults:20},maxChars:1000})).ndjson);
    await(await SpreadsheetFile.exportXlsx(wb)).save(`${qa}/${b.key}-authored.xlsx`);
  }
  const [sheet,range]=preview[b.key];
  const png=await wb.render({sheetName:sheet,range,scale:1,format:'png'});
  await fs.writeFile(`${qa}/${b.key}-${mode}.png`,new Uint8Array(await png.arrayBuffer()));
  console.log(b.key,mode,'done');
  if(mode==='review'){
    const extra={
      'unit-criterion':[[b.sheet,'H72:K73'],[b.sheet,'H124:K124'],[b.sheet,'H142:K142']],
      'unit-report':[[b.sheet,'K62:M62'],[b.sheet,'L124:M124'],['RTT 결과 대조','C270:G271'],['입력 및 예상값','B6503:C6503']],
      'system-report':[[b.sheet,'J45:L45'],[b.sheet,'J104:L104']],
      'integration-criterion':[[b.sheet,'J29:M29'],[b.sheet,'J39:M39']],
    }[b.key]??[];
    for(let i=0;i<extra.length;i++){
      const [sn,r]=extra[i];const data=await wb.render({sheetName:sn,range:r,scale:1,format:'png'});
      await fs.writeFile(`${qa}/${b.key}-extra-${i+1}.png`,new Uint8Array(await data.arrayBuffer()));
    }
  }
}
