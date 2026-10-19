import fs from 'node:fs/promises';
import {Workbook,SpreadsheetFile}from '@oai/artifact-tool';
const payload=JSON.parse(await fs.readFile(import.meta.dirname+'/scope-patch.json','utf8'));
const wb=Workbook.create();const sh=wb.worksheets.add('QCC_SCOPE');
for(const [cell,value]of Object.entries(payload.cells))sh.getRange(cell).values=[[value]];
await(await SpreadsheetFile.exportXlsx(wb)).save(import.meta.dirname+'/scope-artifact.xlsx');
console.log('Artifact-authored QCC support scope at '+Object.keys(payload.cells).join(', '));
