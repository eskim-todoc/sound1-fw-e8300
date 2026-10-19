import fs from 'node:fs/promises';
import {Workbook,SpreadsheetFile} from '@oai/artifact-tool';
const wb=Workbook.create();const sh=wb.worksheets.add('SYS82_METHOD');
sh.getRange('D282').values=[['\nD\n']];
await(await SpreadsheetFile.exportXlsx(wb)).save(import.meta.dirname+'/method82-artifact.xlsx');
console.log('Artifact-authored SYS82 D282 = D');
