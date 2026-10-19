import fs from 'node:fs/promises';
import path from 'node:path';
import { FileBlob, SpreadsheetFile, Workbook } from '@oai/artifact-tool';
const root = path.resolve(import.meta.dirname, '../..');
const payload = JSON.parse(await fs.readFile(path.join(import.meta.dirname,'payload.json'),'utf8'));
await fs.mkdir(path.join(root,'qa/qcc_tests'),{recursive:true});
const baseline=await SpreadsheetFile.importXlsx(await FileBlob.load(payload.books[0].target));
console.log((await baseline.inspect({kind:'sheet',include:'id,name',maxChars:2000})).ndjson);
const before=await baseline.render({sheetName:payload.books[0].sheet,range:'B1686:M1690',scale:1,format:'png'});
await fs.writeFile(path.join(root,'qa/qcc_tests/before-unit.png'),new Uint8Array(await before.arrayBuffer()));
for (const book of payload.books) {
 if(process.argv[2] && book.key!==process.argv[2])continue;
 const wb=Workbook.create();
 const sh=wb.worksheets.add('QCC_AUTHOR');
 for(const [cell,value] of Object.entries(book.cells)) sh.getRange(cell).values=[[value]];
 const cv=wb.worksheets.add('COVER_AUTHOR');
 for(const [cell,value] of Object.entries(book.cover)) cv.getRange(cell).values=[[value]];
 if(Object.keys(book.aux).length){const ax=wb.worksheets.add('AUX_AUTHOR');for(const [cell,value]of Object.entries(book.aux))ax.getRange(cell).values=[[value]];}
 if(Object.keys(book.history).length){const hx=wb.worksheets.add('HISTORY_AUTHOR');for(const [cell,value]of Object.entries(book.history))hx.getRange(cell).values=[[value]];}
 sh.getRange(`B1:${book.lastCol}${book.finalEnd}`).format.font={name:book.font,size:11};
 sh.getRange(`B1:${book.lastCol}${book.finalEnd}`).format.wrapText=true;
 sh.getRange(`B1:${book.lastCol}${book.finalEnd}`).format.verticalAlignment='center';
 for(const [col,width] of Object.entries(book.widths)) sh.getRange(`${col}:${col}`).format.columnWidth=width;
 for(const row of book.newRows) sh.getRange(`B${row}:${book.lastCol}${row}`).format.rowHeight=book.heights[row]||300;
 for(const merge of book.newMerges) sh.mergeCells(merge);
 for(const col of book.centerCols) sh.getRange(`${col}1:${col}${book.finalEnd}`).format.horizontalAlignment='center';
 sh.getRange(`B${book.previewStart}:${book.lastCol}${book.previewEnd}`).format.borders={preset:'all',style:'thin',color:'#000000'};
 await (await SpreadsheetFile.exportXlsx(wb)).save(path.join(import.meta.dirname,book.key+'-artifact.xlsx'));
 const preview=await wb.render({sheetName:'QCC_AUTHOR',range:`B${book.previewStart}:${book.lastCol}${book.previewEnd}`,scale:1,format:'png'});
 await fs.writeFile(path.join(root,`qa/qcc_tests/${book.key}-authored.png`),new Uint8Array(await preview.arrayBuffer()));
 console.log(JSON.stringify({book:book.key,authoredCells:Object.keys(book.cells).length,newRows:book.newRows.length}));
}
