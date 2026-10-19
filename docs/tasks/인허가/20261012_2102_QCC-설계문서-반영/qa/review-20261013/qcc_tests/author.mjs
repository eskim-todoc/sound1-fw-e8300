import fs from 'node:fs/promises';
import path from 'node:path';
import { FileBlob, SpreadsheetFile, Workbook } from '@oai/artifact-tool';
const here=import.meta.dirname;
const p=JSON.parse(await fs.readFile(path.join(here,'author-payload.json'),'utf8'));
for(const [key,b] of Object.entries(p.books)){
 if(process.argv[2] && process.argv[2]!==key)continue;
 const baseline=await SpreadsheetFile.importXlsx(await FileBlob.load(b.path));
 console.log(JSON.stringify({key,import:(await baseline.inspect({kind:'sheet',include:'id,name',maxChars:1200})).ndjson}));
 const range=key.startsWith('SU')?'B1737:M1737':key.startsWith('SI')?'B136:O136':'B282:L283';
 const before=await baseline.render({sheetName:b.sheet,range,scale:1,format:'png'});
 await fs.writeFile(path.join(here,`${key}-before.png`),new Uint8Array(await before.arrayBuffer()));
 const stage=Workbook.create();
 for(const [sn,comp] of Object.entries(b.comparison)){
  const sh=stage.worksheets.add(sn);
  for(const [addr,c] of Object.entries(comp))sh.getRange(addr).values=[[c.display]];
  sh.getRange('A1:O1775').format.font={name:key.startsWith('SI')?'맑은 고딕':'Calibri',size:11};
  sh.getRange('A1:O1775').format.wrapText=true;
 }
 stage.recalculate();
 await (await SpreadsheetFile.exportXlsx(stage)).save(path.join(here,`${key}-authored.xlsx`));
 console.log(JSON.stringify({key,cells:Object.values(b.comparison).reduce((n,c)=>n+Object.keys(c).length,0),exported:true}));
}
