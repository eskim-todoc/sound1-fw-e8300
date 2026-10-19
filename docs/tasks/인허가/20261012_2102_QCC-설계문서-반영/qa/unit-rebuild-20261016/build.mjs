import fs from 'node:fs/promises';
import {FileBlob, SpreadsheetFile} from '@oai/artifact-tool';
const mode=process.argv[2];
const wb=await SpreadsheetFile.importXlsx(await FileBlob.load(mode==='preview'?'base.xlsx':'prepared.xlsx'));
const main=wb.worksheets.getItem('보고서_유닛 시험');
if(mode==='preview'){
 console.log((await wb.inspect({kind:'sheet',include:'id,name',maxChars:1200})).ndjson);
 const p=await wb.render({sheetName:main.name,range:'J1724:M1726',scale:1.5});
 await fs.writeFile('base-artifact.png',new Uint8Array(await p.arrayBuffer()));
} else {
 const plan=JSON.parse(await fs.readFile('plan.json','utf8'));
 const matrix=Array.from({length:38},(_,i)=>Array.from({length:12},(_,j)=>plan.values[String.fromCharCode(66+j)+(1727+i)]??null));
 main.getRange('B1727:M1764').values=matrix;
 for(const [a,v] of Object.entries(plan.updates))main.getRange(a).values=[[v]];
 main.getRange('B1727:M1764').format.font={name:'Calibri',size:11,color:'#000000'};
 main.getRange('B1727:M1764').format.wrapText=true;
 for(const [r,h] of Object.entries(plan.rowHeights))main.getRange(`B${r}:M${r}`).format.rowHeight=h;
 for(const range of plan.merges)main.mergeCells(range);
 const cmp=wb.worksheets.add(plan.comparisonSheet);cmp.showGridLines=false;
 cmp.getRange('B2').values=[['유닛 시험 보고서 변경 비교']];cmp.getRange('B2').format.font={name:'맑은 고딕',size:14,bold:true};
 cmp.getRange('B4:G4').values=[['시험 ID','조건','본문 셀','항목','변경 전','변경 후']];
 cmp.getRange('B4:G4').format={fill:'#F5F5F5',font:{name:'맑은 고딕',size:10,bold:true},horizontalAlignment:'center',verticalAlignment:'center',wrapText:true,rowHeight:28};
 for(const [col,w] of Object.entries({A:2,B:32,C:8,D:12,E:25,F:36,G:130}))cmp.getRange(`${col}:${col}`).format.columnWidth=w;
 const rows=plan.changes.map(x=>[x.id,x.condition,x.address,x.field,'[-] '+(x.before.trim()||'(없음)'),'[+] '+x.after.trim()]);
 cmp.getRange(`B5:G${4+rows.length}`).values=rows;
 const body=cmp.getRange(`B5:G${4+rows.length}`);body.format.font={name:'맑은 고딕',size:10,color:'#000000'};body.format.wrapText=true;body.format.verticalAlignment='center';
 cmp.getRange(`F5:F${4+rows.length}`).format.font.color='#C00000';cmp.getRange(`G5:G${4+rows.length}`).format.font.color='#008000';
 cmp.getRange(`B5:D${4+rows.length}`).format.horizontalAlignment='center';
 cmp.getRange(`E5:G${4+rows.length}`).format.horizontalAlignment='left';
 for(let i=0;i<rows.length;i++){
  const lineCount=(s,w)=>String(s).split('\n').reduce((acc,line)=>acc+Math.max(1,Math.ceil([...line].reduce((v,c)=>v+(c.charCodeAt(0)>255?2:1),0)/w)),0);
  const h=Math.max(54,Math.max(lineCount(rows[i][0],27),lineCount(rows[i][4],34),lineCount(rows[i][5],124))*15+18);
  cmp.getRange(`B${i+5}:G${i+5}`).format.rowHeight=Math.min(409.5,h);
 }
 cmp.freezePanes.freezeRows(4);
 wb.recalculate();
 console.log((await wb.inspect({kind:'table',range:`${main.name}!D1727:G1728`,include:'values',tableMaxRows:2,tableMaxCols:4,maxChars:1600})).ndjson);
 console.log((await wb.inspect({kind:'match',searchTerm:'#REF!|#DIV/0!|#VALUE!|#NAME\\?',options:{useRegex:true,maxResults:20},maxChars:1000})).ndjson);
 const out=await SpreadsheetFile.exportXlsx(wb);await out.save('authored.xlsx');
 const p=await wb.render({sheetName:cmp.name,range:'B4:G9',scale:1.25});await fs.writeFile('comparison-artifact.png',new Uint8Array(await p.arrayBuffer()));
 console.log(JSON.stringify({authored:true,comparisonRows:rows.length}));
}
