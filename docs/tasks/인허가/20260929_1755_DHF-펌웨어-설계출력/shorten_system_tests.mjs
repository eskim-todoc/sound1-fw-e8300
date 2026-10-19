import fs from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
import {FileBlob,SpreadsheetFile} from '@oai/artifact-tool';
const task=fileURLToPath(new URL('./',import.meta.url));
const qa=`${task}/qa/${process.argv[3]??'system-concise-rev0'}`;
const mode=process.argv[2]??'before';
const plan=JSON.parse(await fs.readFile(`${qa}/plan.json`,'utf8').catch(error=>{
  if(mode==='before' && error.code==='ENOENT') return '{}';
  throw error;
}));
const wb=await SpreadsheetFile.importXlsx(await FileBlob.load(`${qa}/system-${mode==='review'?'candidate':'before'}.xlsx`));
if(mode==='build'){
  for(const x of plan.changes) wb.worksheets.getItem(x.sheet).getRange(x.cell).values=[[x.after]];
  wb.recalculate();
  console.log((await wb.inspect({kind:'region',sheetId:'보고서_시스템 시험',range:plan.inspectRange??'H34:L34',maxChars:2200})).ndjson);
  console.log((await wb.inspect({kind:'match',searchTerm:'#REF!|#DIV/0!|#VALUE!|#NAME\\?|#NUM!',options:{useRegex:true,maxResults:10},maxChars:1000})).ndjson);
  await(await SpreadsheetFile.exportXlsx(wb)).save(`${qa}/system-authored.xlsx`);
}else{
  const views=plan.views??(mode==='before'?[['rate','G34:J34'],['led','G98:J99']]:[
    ['rate','G34:L34'],['current','G41:J43'],['nofm','G64:J67'],['led','G98:J99'],['file','G219:J220']
  ]);
  for(const [name,range] of views){
    const png=await wb.render({sheetName:'보고서_시스템 시험',range,scale:1,format:'png'});
    await fs.writeFile(`${qa}/${mode}-${name}.png`,new Uint8Array(await png.arrayBuffer()));
  }
  if(mode==='review' && !plan.views){
    const png=await wb.render({sheetName:'시험 보조자료',range:'A5:D6',scale:1,format:'png'});
    await fs.writeFile(`${qa}/review-helper.png`,new Uint8Array(await png.arrayBuffer()));
  }
  console.log(mode,'views rendered');
}
