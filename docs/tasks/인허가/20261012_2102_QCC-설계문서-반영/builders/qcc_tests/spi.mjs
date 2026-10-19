import fs from 'node:fs/promises';import {Workbook,SpreadsheetFile}from '@oai/artifact-tool';
const p=JSON.parse(await fs.readFile(import.meta.dirname+'/spi-patch.json','utf8'));const wb=Workbook.create();
for(const [kind,name]of [['unit','UNIT_PATCH'],['integration','INTEGRATION_PATCH']]){const sh=wb.worksheets.add(name);for(const[cell,value]of Object.entries(p[kind]))sh.getRange(cell).values=[[value]];}
await(await SpreadsheetFile.exportXlsx(wb)).save(import.meta.dirname+'/spi-artifact.xlsx');console.log('Artifact-authored logical/SPI21byte and integration support scope patch.');
