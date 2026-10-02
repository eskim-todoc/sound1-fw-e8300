import fs from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
import {Workbook, SpreadsheetFile} from '@oai/artifact-tool';
const dir=new URL('./qa/sstc-id63/',import.meta.url);
const w=Workbook.create(); const s=w.worksheets.add('ID63');
const values=[
 '- 외부장치와 리모콘을 연결하고 배터리 잔량을 조회할 수 있는 상태로 준비한다.',
 '- QCC에서 전달하는 배터리 잔량 정보를 확인하고 리모콘에서 잔량을 조회한다.\n- 배터리 잔량 정보를 변경한 후 다시 조회하여 표시 변화를 확인한다.',
 '- 리모콘에 외부장치의 배터리 잔량이 표시되어야 한다.\n- 표시된 잔량은 외부장치에서 전달한 잔량 정보에 대응해야 하며, 잔량 변경 후 다시 조회하면 변경된 상태가 표시되어야 한다.'
];
s.getRange('A1:C1').values=[values];
w.recalculate();
console.log((await w.inspect({kind:'table',range:'ID63!A1:C1',include:'values',tableMaxRows:1,tableMaxCols:3})).ndjson);
await (await SpreadsheetFile.exportXlsx(w)).save(fileURLToPath(new URL('authored.xlsx',dir)));
await fs.writeFile(new URL('values.json',dir),JSON.stringify(values));
