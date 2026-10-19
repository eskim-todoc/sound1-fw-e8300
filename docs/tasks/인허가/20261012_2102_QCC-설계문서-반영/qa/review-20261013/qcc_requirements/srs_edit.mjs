import fs from 'node:fs/promises';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {FileBlob,SpreadsheetFile} from '@oai/artifact-tool';
const root=path.dirname(fileURLToPath(import.meta.url));
const plan=JSON.parse(await fs.readFile(path.join(root,'srs-cell-plan.json'),'utf8'));
const wb=await SpreadsheetFile.importXlsx(await FileBlob.load(path.join(root,'srs-structural.xlsx')));
const sheet=wb.worksheets.getItem('소프트웨어 입력 요구사항 사양서');
const before=await wb.render({sheetName:sheet.name,range:'C49:I53',scale:1.5,format:'png'});
await fs.writeFile(path.join(root,'srs-before.png'),new Uint8Array(await before.arrayBuffer()));
for(const e of plan.edits){sheet.getRange(e.cell).values=[[e.value]];sheet.getRange(e.cell).format.wrapText=true;}
wb.recalculate();
const check=plan.edits.map(e=>({cell:e.cell,value:sheet.getRange(e.cell).values[0][0],matches:sheet.getRange(e.cell).values[0][0]===e.value}));
if(check.some(e=>!e.matches))throw new Error('Artifact Tool value check failed');
await fs.writeFile(path.join(root,'artifact-check.json'),JSON.stringify(check,null,2));
await(await SpreadsheetFile.exportXlsx(wb)).save(path.join(root,'srs-artifact.xlsx'));
console.log(JSON.stringify({cells:check.length,allMatch:true}));
