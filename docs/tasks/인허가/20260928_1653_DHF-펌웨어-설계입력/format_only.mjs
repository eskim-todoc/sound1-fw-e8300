import fs from 'node:fs/promises';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {FileBlob,SpreadsheetFile} from '@oai/artifact-tool';
const t=path.dirname(fileURLToPath(import.meta.url)),q=path.join(t,'format-only-20260929');
const p=path.resolve(t,'../../../참고/인허가/사운드1/DHF/02_설계 입력/SW/펌웨어/SRS-FW-TD24A 소프트웨어 요구사항 사양서.xlsx');
await fs.mkdir(q,{recursive:true});await fs.copyFile(p,path.join(q,'before.xlsx'));
const w=await SpreadsheetFile.importXlsx(await FileBlob.load(p));
const s=w.worksheets.getItem('소프트웨어 입력 요구사항 사양서_변경 비교');
for(const r of [11,13,15]){s.getRange(`A${r}`).values=[[null]];s.getRange(`F${r}:I${r}`).format.fill='#FFFFFF';}
w.recalculate();await(await SpreadsheetFile.exportXlsx(w)).save(path.join(q,'authored.xlsx'));
