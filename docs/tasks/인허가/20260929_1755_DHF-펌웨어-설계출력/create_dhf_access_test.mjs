import fs from 'node:fs/promises';
import path from 'node:path';
import { FileBlob, SpreadsheetFile, Workbook } from '@oai/artifact-tool';

const root = 'G:/공유 드라이브/20_프로젝트별/01_project sound1/08_DHF';
const target = path.join(root, '05_설계 유효성 확인', 'SW', '외부기');
const output = path.join(target, '[테스트] DHF 파일 쓰기 확인.xlsx');
const qa = new URL('./qa/dhf-output-access-20261012/', import.meta.url);
await fs.access(path.join(root, '05_설계 유효성 확인', 'SW'));
await fs.mkdir(target, { recursive: true });
try {
  await fs.access(output);
  throw new Error('같은 이름의 기존 파일이 있어 덮어쓰지 않는다.');
} catch (err) {
  if (err.code !== 'ENOENT') throw err;
}
await fs.mkdir(qa, { recursive: true });
const wb = Workbook.create();
const sheet = wb.worksheets.add('파일 확인');
sheet.showGridLines = false;
sheet.mergeCells('A1:B1');
sheet.getRange('A1').values = [['Sound1 DHF 파일 생성/수정 확인']];
sheet.getRange('A3:B9').values = [
  ['확인 항목', '내용'],
  ['파일 생성', '확인 완료'],
  ['내용 수정', '수정 전'],
  ['저장 횟수', 1],
  ['입력 값 1', 3],
  ['입력 값 2', 4],
  ['합계', null],
];
sheet.getRange('B9').formulas = [['=SUM(B7:B8)']];
sheet.getRange('A1:B9').format.font = { name: '맑은 고딕', size: 10 };
sheet.getRange('A1').format.font = { name: '맑은 고딕', size: 14, bold: true };
sheet.getRange('A1:B1').format.rowHeight = 32;
sheet.getRange('A3:B9').format.rowHeight = 25;
sheet.getRange('A3:B9').format.verticalAlignment = 'center';
sheet.getRange('A3:B3').format.fill = '#F2F2F2';
sheet.getRange('A3:B3').format.font.bold = true;
sheet.getRange('A3:B9').format.borders = { preset: 'all', style: 'thin', color: '#D9D9D9' };
sheet.getRange('A:A').format.columnWidth = 26;
sheet.getRange('B:B').format.columnWidth = 34;
sheet.getRange('B6:B9').setNumberFormat('0');
wb.recalculate();
await (await SpreadsheetFile.exportXlsx(wb)).save(output);

// 새로 만든 파일만 다시 읽고 수정하여 실제 저장 가능 여부를 확인한다.
const reopened = await SpreadsheetFile.importXlsx(await FileBlob.load(output));
const modified = reopened.worksheets.getItem('파일 확인');
if (modified.getRange('B5').values[0][0] !== '수정 전') throw new Error('최초 저장 내용 확인 실패');
modified.getRange('B5').values = [['확인 완료']];
modified.getRange('B6').values = [[2]];
reopened.recalculate();
await (await SpreadsheetFile.exportXlsx(reopened)).save(output);
const final = await SpreadsheetFile.importXlsx(await FileBlob.load(output));
const verified = final.worksheets.getItem('파일 확인');
final.recalculate();
if (verified.getRange('B5').values[0][0] !== '확인 완료' || verified.getRange('B6').values[0][0] !== 2 || verified.getRange('B9').values[0][0] !== 7) {
  throw new Error('최종 파일 내용/계산 검증 실패');
}
for (const row of verified.getRange('A1:B9').values) {
  if (row.some(value => typeof value === 'string' && /^#(REF!|DIV\/0!|VALUE!|NAME\?|N\/A|NUM!|NULL!)/.test(value))) throw new Error('수식 오류');
}
console.log((await final.inspect({ kind: 'region', sheetId: '파일 확인', range: 'A1:B9', maxChars: 2000 })).ndjson);
const preview = await final.render({ sheetName: '파일 확인', autoCrop: 'all', scale: 1.5, format: 'png' });
await fs.writeFile(new URL('preview.png', qa), new Uint8Array(await preview.arrayBuffer()));
console.log(JSON.stringify({ output, created: true, modified: true, reopened: true, total: 7, size: (await fs.stat(output)).size }));
