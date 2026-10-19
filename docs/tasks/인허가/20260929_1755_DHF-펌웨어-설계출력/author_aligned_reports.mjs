// 기준서의 시험 행을 그대로 사용하고 결과/판정 두 열만 덧붙인다.
import fs from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
import {FileBlob, SpreadsheetFile} from '@oai/artifact-tool';
const task=fileURLToPath(new URL('./',import.meta.url));
const qa=`${task}/qa/criterion-report-alignment`;
const plans=JSON.parse(await fs.readFile(`${qa}/plan.json`,'utf8'));
const mode=process.argv[2]??'before';
const letter=i=>String.fromCharCode(64+i);
const savePng=async(wb,sheet,range,file)=>{
  const png=await wb.render({sheetName:sheet,range,scale:1,format:'png'});
  await fs.writeFile(`${qa}/${file}.png`,new Uint8Array(await png.arrayBuffer()));
};
for(const p of plans){
  const wb=await SpreadsheetFile.importXlsx(await FileBlob.load(mode==='review'?`${qa}/${p.kind}-candidate.xlsx`:p.criterion));
  const main=wb.worksheets.getItem(mode==='review'?p.reportSheet:p.criterionSheet);
  if(mode==='before'){
    await savePng(wb,main.name,`${letter(p.commonEnd-1)}${p.header}:${letter(p.commonEnd)}${p.records[0].row+1}`,`${p.kind}-criterion-before`);
    const old=await SpreadsheetFile.importXlsx(await FileBlob.load(p.report));
    await savePng(old,p.reportSheet,`${letter(p.commonEnd-1)}${p.reportIds[0][0]-2}:${letter(p.commonEnd+2)}${p.reportIds[0][0]+2}`,`${p.kind}-report-before`);
    console.log(p.kind,'baseline rendered');
    continue;
  }
  const actual=letter(p.commonEnd+1), verdict=letter(p.commonEnd+2);
  const put=(s,a,v)=>{s.getRange(a).values=[[v]];};
  if(mode==='build'){
    put(main,'B2',p.criterionSheets[p.criterionSheet].rows[1][1].replace('기준서','보고서'));
    main.getRange(`${actual}1:${actual}${p.criterionSheets[p.criterionSheet].rows.length}`).format.columnWidthPx=p.actualWidth;
    main.getRange(`${verdict}1:${verdict}${p.criterionSheets[p.criterionSheet].rows.length}`).format.columnWidthPx=125;
    for(const c of [actual,verdict]){
      main.getRange(`${c}${p.header}`).copyFrom(main.getRange(`${letter(p.commonEnd)}${p.header}`),'all');
      main.getRange(`${c}${p.header}:${c}${p.header+1}`).merge();
      main.getRange(`${c}${p.header}:${c}${p.header+1}`).format={wrapText:true,horizontalAlignment:'center',verticalAlignment:'center',font:{name:'맑은 고딕',size:10,bold:true},fill:'#E7E6E6',borders:{preset:'all',style:'thin',color:'#B7B7B7'}};
    }
    put(main,`${actual}${p.header}`,'시험/측정 내용 및 결과');put(main,`${verdict}${p.header}`,'통과 여부');
    for(const r of p.records){
      for(const c of [actual,verdict])main.getRange(`${c}${r.row}`).copyFrom(main.getRange(`${letter(p.commonEnd)}${r.row}`),'all');
      put(main,`${actual}${r.row}`,r.actual);put(main,`${verdict}${r.row}`,r.judgment);
      main.getRange(`${actual}${r.row}:${verdict}${r.row}`).format={font:{name:'맑은 고딕',size:10,color:'#111111'},wrapText:true,verticalAlignment:'top',horizontalAlignment:'left',borders:{preset:'all',style:'thin',color:'#B7B7B7'}};
      main.getRange(`B${r.row}:${verdict}${r.row}`).format.rowHeight=r.height;
      main.getRange(`${verdict}${r.row}`).format.horizontalAlignment='center';
      if(r.judgment!=='통과')main.getRange(`${verdict}${r.row}`).format.font={name:'맑은 고딕',size:10,color:'#C00000',bold:true};
      // Required evidence placeholders stay in the result cell, never in an extra test row.
      if(r.qcc||r.photo)main.getRange(`${actual}${r.row}`).format.font.color='#C00000';
    }
    const oldProvenance=p.reportSheets[p.reportSheet].rows[4][p.kind==='integration'?9:6];
    main.mergeCells(`${actual}5:${verdict}8`);put(main,`${actual}5`,oldProvenance);
    main.getRange(`${actual}5:${verdict}8`).format={font:{name:'맑은 고딕',size:10},wrapText:true,verticalAlignment:'center'};
    main.mergeCells(`${actual}10:${verdict}13`);
    put(main,`${actual}10`,'시험 ID, 방법, 준비 조건, 제한 조건, 절차 및 통과 기준은 기준서와 동일하다. 시험/측정 결과와 판정만 추가한다.\n조건별 상세 결과는 「시험 보조자료」에 보존한다. 예상 그림은 실제 측정 화면이 아니며, 실제 사진은 (사진필요)에 추가한다.');
    main.getRange(`${actual}10:${verdict}13`).format={font:{name:'맑은 고딕',size:10},wrapText:true,verticalAlignment:'center'};
    const detail=wb.worksheets.add('시험 보조자료');detail.showGridLines=false;
    put(detail,'A2','시험 조건별 상세 결과');
    detail.getRange('A2').format.font={name:'맑은 고딕',size:14,bold:true};
    put(detail,'A3','본문의 시험 행은 기준서와 동일하다. 조건별 값과 확인 범위의 상세 기록은 이 시트에 보존한다.');
    const data=[];
    for(const r of p.records){
      for(const c of r.comparisons)data.push([r.id,c.expected,c.actual,r.judgment]);
      for(const note of r.notes)data.push([r.id,'확인 범위 및 첨부 자료',note,r.judgment]);
    }
    detail.getRange(`A5:D${data.length+5}`).format={font:{name:'맑은 고딕',size:10},wrapText:true,verticalAlignment:'top',horizontalAlignment:'left'};
    detail.getRange('A5:D5').values=[['시험 ID','입력 조건 및 예상 결과','시험/측정 내용 및 결과','통과 여부']];
    detail.getRange('A5:D5').format={font:{name:'맑은 고딕',size:10,bold:true},fill:'#E7E6E6',horizontalAlignment:'center',verticalAlignment:'center',rowHeight:30};
    detail.getRange(`A6:D${data.length+5}`).values=data;
    const widths=[225,1100,1250,125];
    widths.forEach((w,i)=>{detail.getRangeByIndexes(0,i,data.length+5,1).format.columnWidthPx=w;});
    data.forEach((row,i)=>{
      const h=Math.max(35,...row.map((v,c)=>String(v).split('\n').reduce((n,line)=>n+Math.max(1,Math.ceil([...line].reduce((sum,ch)=>sum+(ch.charCodeAt(0)>127?13.4:7.1),0)/(widths[c]-18))),0)*14+18));
      if(h>409)throw new Error(`${p.kind} support row ${i+6} is too tall: ${h}`);
      detail.getRange(`A${i+6}:D${i+6}`).format.rowHeight=h;
      if(row[2].includes('(사진필요)')||row[2].includes('(QCC 검증 결과 필요)'))detail.getRange(`C${i+6}`).format.font.color='#C00000';
    });
    detail.freezePanes.freezeRows(5);
    const images=wb.worksheets.add('예상 그림');images.showGridLines=false;
    put(images,'A2','예상 그림과 실제 사진 첨부');images.getRange('A2').format.font={name:'맑은 고딕',size:14,bold:true};
    put(images,'A3','예상 도식 / 실측 화면 아님. 본문의 시험 ID로 찾고, 같은 입력의 실제 사진을 오른쪽에 첨부한다.');
    images.getRange('A1:A160').format.columnWidthPx=300;images.getRange('B1:C160').format.columnWidthPx=620;
    p.imageRows=[];
    for(let i=0;i<p.images.length;i++){
      const im=p.images[i],r=6+i*3;
      put(images,`A${r}`,`그림 ${i+1}. ${im.title??im.caption}\n${im.tests.join('\n')}`);
      put(images,`B${r}`,'예상 도식 / 실측 화면 아님');put(images,`C${r}`,'(사진필요) 같은 입력 조건의 실제 사진/파형을 첨부한다.');
      const caption=String(images.getRange(`A${r}`).values[0][0]);
      const captionLines=caption.split('\n').reduce((n,line)=>n+Math.max(1,Math.ceil([...line].reduce((s,ch)=>s+(ch.charCodeAt(0)>127?13.4:7.1),0)/282)),0);
      images.getRange(`A${r}:C${r}`).format={font:{name:'맑은 고딕',size:10},wrapText:true,verticalAlignment:'top',rowHeight:Math.max(55,captionLines*14+18)};
      images.getRange(`A${r+1}:C${r+1}`).format.rowHeight=296;
      images.getRange(`C${r}`).format.font.color='#C00000';
      const bytes=await fs.readFile(im.path);
      images.images.add({dataUrl:`data:image/png;base64,${bytes.toString('base64')}`,anchor:{from:{row:r,col:1,rowOffsetPx:4,colOffsetPx:4},extent:{widthPx:600,heightPx:371}}});
      p.imageRows.push({row:r,tests:im.tests,path:im.path});
    }
    wb.recalculate();
    console.log((await wb.inspect({kind:'match',searchTerm:'#REF!|#DIV/0!|#VALUE!|#NAME\\?',options:{useRegex:true,maxResults:20},maxChars:1000})).ndjson);
    await(await SpreadsheetFile.exportXlsx(wb)).save(`${qa}/${p.kind}-authored.xlsx`);
    await fs.writeFile(`${qa}/${p.kind}-built.json`,JSON.stringify({records:p.records,imageRows:p.imageRows},null,2));
  }
  for(const idx of [0,Math.floor(p.records.length/2),p.records.length-1]){
    const r=p.records[idx];await savePng(wb,main.name,`${letter(p.commonEnd-1)}${r.row}:${verdict}${r.row}`,`${p.kind}-after-${idx+1}`);
  }
  await savePng(wb,main.name,`${letter(p.commonEnd-1)}${p.header}:${verdict}${p.records[0].row}`,`${p.kind}-after-header`);
  await savePng(wb,'시험 보조자료','A5:D8',`${p.kind}-after-detail`);
  await savePng(wb,'예상 그림','A6:C7',`${p.kind}-after-image`);
  if(mode==='review'){
    const picks=p.kind==='unit'?['CFX-023','CFX-039','HEAR-002','CM3-051','BOOT-020']:p.kind==='integration'?['SW-FW-IntegrationTest-ID-005','SW-FW-IntegrationTest-ID-017']:['SW-FW-SYS-Test-ID-10','SW-FW-SYS-Test-ID-62','SW-FW-SYS-Test-ID-77'];
    for(const id of picks){const r=p.records.find(r=>r.uid===id);await savePng(wb,main.name,`${actual}${r.row}:${verdict}${r.row}`,`${p.kind}-full-${id}`);}
  }
  console.log(p.kind,mode,'done');
}
