import fs from 'node:fs/promises';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {FileBlob,SpreadsheetFile} from '@oai/artifact-tool';
const qa=path.join(path.dirname(fileURLToPath(import.meta.url)),'qa/dhf-materials-system-20261012');
const planName=process.argv.find(a=>a.startsWith('--plan='))?.slice(7)||'plan.json';
const plan=JSON.parse(await fs.readFile(path.join(qa,planName),'utf8'));
const only=process.argv.find(a=>a.startsWith('--code='))?.slice(7);
for(const p of plan.books){
  if(only && p.code!==only)continue;
  const wb=await SpreadsheetFile.importXlsx(await FileBlob.load(p.before));
  const range=p.code.startsWith('SS')?'J217:K218':p.code==='SRS'?'F49:H50':'C19:D19';
  try {
    const preview=await wb.render({sheetName:p.main,range,scale:1.3,format:'png'});
    await fs.writeFile(path.join(qa,p.code+'-before.png'),new Uint8Array(await preview.arrayBuffer()));
  }catch(e){console.log(p.code,'preview:',String(e).slice(0,180));}
  for(const e of p.edits)wb.worksheets.getItem(e.sheet).getRange(e.cell).values=[[e.after]];
  for(const e of p.styles){
    const fmt=wb.worksheets.getItem(e.sheet).getRange(e.cell).format;
    fmt.horizontalAlignment=e.horizontal;fmt.verticalAlignment=e.vertical;fmt.wrapText=true;
  }
  const borders=new Map();
  for(const b of p.borders){const key=b.sheet+'|'+b.cell;const v=borders.get(key)||{};v[b.edge]={style:'thin',color:'#000000'};borders.set(key,v);}
  for(const [key,value]of borders){const [sheet,cell]=key.split('|');wb.worksheets.getItem(sheet).getRange(cell).format.borders=value;}
  for(const e of p.edits)if(wb.worksheets.getItem(e.sheet).getRange(e.cell).values[0][0]!==e.after)throw Error(p.code+'/'+e.cell);
  const check=await wb.inspect({kind:'match',searchTerm:'#REF!|#DIV/0!|#VALUE!|#NAME\\?',options:{useRegex:true,maxResults:10},maxChars:1800});
  await fs.writeFile(path.join(qa,p.code+'-artifact-check.txt'),check.ndjson);
  await(await SpreadsheetFile.exportXlsx(wb)).save(p.artifact);
  console.log(p.code,'authored',p.edits.length,'text cells',p.styles.length,'alignments');
}
