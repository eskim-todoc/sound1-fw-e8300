import fs from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
import {FileBlob,SpreadsheetFile} from '@oai/artifact-tool';
const root=fileURLToPath(new URL('../../../../',import.meta.url));
const qa=fileURLToPath(new URL('./qa/suvr/',import.meta.url));
const ref='G:/공유 드라이브/20_프로젝트별/01_project sound1/04_docs/99_참고용DHF/04_설계 검증/SW/펌웨어/SUVR-FW-TD20B 소프트웨어 유닛 검증 보고서.xlsx';
const mode=process.argv[2];
const source=mode==='direct'?`${qa}/before-direct-comparison.xlsx`:mode==='direct-review'?`${qa}/direct-candidate.xlsx`:mode==='review'?`${qa}/candidate.xlsx`:ref;
const wb=await SpreadsheetFile.importXlsx(await FileBlob.load(source));
console.log((await wb.inspect({kind:'sheet',include:'id,name',maxChars:2000})).ndjson);
if(mode==='direct'||mode==='direct-review'){
  const plan=JSON.parse(await fs.readFile(`${qa}/direct-plan.json`,'utf8'));
  const original=JSON.parse(await fs.readFile(`${qa}/plan.json`,'utf8'));
  const s=wb.worksheets.getItem('보고서_유닛 시험');
  const font='맑은 고딕',red='#C00000';
  const put=(cell,value)=>{s.getRange(cell).values=[[value]];};
  if(mode==='direct'){
    // The copied template still contains two old signature pictures.
    // Cells were blank, but Excel prints floating pictures: keep signature fields empty.
    wb.worksheets.getItem('표지').deleteAllDrawings();
    for(const a of plan.oldMerges)s.unmergeCells(a);
    const end=plan.summaryRow+5;
    s.getRange(`B34:M${end}`).clear({applyTo:'all'});
    s.getRange(`B34:M${end}`).formulas=Array.from({length:end-33},()=>Array(12).fill(''));
    s.getRange(`B34:M${plan.bodyEnd}`).format={font:{name:font,size:10,color:'#111111'},wrapText:true,verticalAlignment:'top',horizontalAlignment:'left',borders:{preset:'all',style:'thin',color:'#B7B7B7'}};
    s.getRange(`B34:M${plan.bodyEnd}`).values=plan.rows;
    plan.widths.forEach((w,c)=>{s.getRangeByIndexes(0,c+1,end,1).format.columnWidthPx=w;});
    for(const m of plan.meta){
      s.getRange(`B${m.row}:M${m.row}`).format.rowHeight=m.height;
      if(m.kind==='rtt')s.getRange(`K${m.row}:L${m.row}`).format.font.name='Consolas';
      if(m.kind==='note'||m.kind==='image')s.getRange(`L${m.row}`).format.font.color=red;
      if(m.kind==='image')s.getRange(`H${m.row}:J${m.row}`).format.verticalAlignment='center';
      if(m.kind==='rtt'&&plan.rows[m.row-34][9]!==plan.rows[m.row-34][10])s.getRange(`L${m.row}:M${m.row}`).format.font.color=red;
    }
    for(const a of plan.merges)s.mergeCells(a);
    s.getRange(`B34:C${plan.bodyEnd}`).format.horizontalAlignment='center';
    s.getRange(`F34:G${plan.bodyEnd}`).format.horizontalAlignment='center';
    s.getRange(`M34:M${plan.bodyEnd}`).format.horizontalAlignment='center';
    for(const u of plan.units){
      s.getRange(`D${u.start}:M${u.start}`).format.borders={top:{style:'medium',color:'#687784'}};
      if(u.judgment!=='통과')s.getRange(`M${u.start}`).format={font:{color:red,bold:true},fill:'#FFF2F2'};
    }
    for(const im of plan.images){
      const bytes=await fs.readFile(`${qa}/expected-images/${im.file}`);
      s.images.add({dataUrl:`data:image/png;base64,${bytes.toString('base64')}`,anchor:{from:{row:im.row-1,col:10,rowOffsetPx:6,colOffsetPx:6},extent:{widthPx:600,heightPx:371}}});
    }
    put('K32','통과 기준\n(예상 결과)');put('L32','측정/검토 내용\n(시험 결과)');
    put('G14','같은 입력 조건의 예상 결과와 시험 결과를 한 행에 나란히 적었다. 유닛의 판정은 첫 비교 행의 결과란에 표시한다.\nRTT 비교 문자열과 예상 그림은 해당 유닛 바로 아래에 배치했다. 기존 보조 시트는 상세 자료로 유지한다.\n예상 도식은 실측 화면이 아니며, (사진필요)와 (QCC 검증 결과 필요)는 자료를 추가할 자리다.');
    s.mergeCells(`D${plan.qccRow}:M${plan.qccRow}`);
    put(`D${plan.qccRow}`,'(QCC 검증 결과 필요) QCC 내부 유닛 정의와 기준이 확정되면 이 위치 다음에 해당 시험 결과를 추가한다.');
    s.getRange(`D${plan.qccRow}:M${plan.qccRow}`).format={font:{name:font,size:10,color:red},wrapText:true,rowHeight:36};
    const r=plan.summaryRow;
    put(`B${r}`,'6. 미해결 오류 및 확인할 자료');s.getRange(`B${r}:F${r}`).format.font={name:font,size:12,bold:true};
    s.mergeCells(`D${r+1}:K${r+1}`);put(`D${r+1}`,'유닛별 판정 집계. 세부 비교 행과 RTT 행은 중복 집계하지 않는다.');
    put(`L${r+1}`,'통과');s.getRange(`M${r+1}`).formulas=[[`=COUNTIFS(E34:E${plan.bodyEnd},"<>",M34:M${plan.bodyEnd},"통과")`]];
    put(`L${r+2}`,'결과 대조 필요');s.getRange(`M${r+2}`).formulas=[[`=COUNTIFS(E34:E${plan.bodyEnd},"<>",M34:M${plan.bodyEnd},"결과 대조 필요")`]];
    s.mergeCells(`D${r+4}:M${r+4}`);put(`D${r+4}`,'결과 대조 필요: '+plan.units.filter(u=>u.judgment!=='통과').map(u=>u.id).join(', '));
    s.getRange(`D${r+4}:M${r+4}`).format={font:{name:font,size:10,color:red},wrapText:true,rowHeight:36};
    s.getRange(`D${r+1}:M${r+2}`).format={font:{name:font,size:10},wrapText:true,rowHeight:28};
    s.showGridLines=false;
    wb.recalculate();
    console.log((await wb.inspect({kind:'region',sheetId:'보고서_유닛 시험',range:`L${r+1}:M${r+2}`,maxChars:1000})).ndjson);
    console.log((await wb.inspect({kind:'match',searchTerm:'#REF!|#DIV/0!|#VALUE!|#NAME\\?',options:{useRegex:true,maxResults:20},maxChars:2000})).ndjson);
    await(await SpreadsheetFile.exportXlsx(wb)).save(`${qa}/direct-candidate.xlsx`);
  }
  const units=new Map(plan.units.map(u=>[u.id,u]));
  const views=[];
  for(const id of ['CFX-001','CFX-018','CFX-020','CM3-014','CM3-042','BOOT-014']){
    const u=units.get(id);views.push([`direct-${id}`,`H${u.start}:M${Math.min(u.comparisonEnd,u.start+3)}`]);
  }
  for(const id of ['CFX-039','CM3-036','CM3-038','CM3-041','CM3-049']){
    const u=units.get(id);views.push([`direct-${id}`,`K${u.start}:M${u.comparisonEnd}`]);
  }
  for(const im of plan.images)views.push([`direct-image-${im.id}-${im.row}`,`J${im.row}:M${im.row}`]);
  views.push(['direct-summary',`D${plan.summaryRow}:M${plan.summaryRow+4}`]);
  for(const [name,range] of views){
    const png=await wb.render({sheetName:s.name,range,scale:1,format:'png'});
    await fs.writeFile(`${qa}/${name}.png`,new Uint8Array(await png.arrayBuffer()));
  }
  console.log(JSON.stringify({units:plan.units.length,images:plan.images.length,bodyEnd:plan.bodyEnd,counts:plan.counts,output:`${qa}/direct-candidate.xlsx`}));
}else if(process.argv[2]==='review'){
  const views=[['final-cover','표지','B2:O24'],['final-front','보고서_유닛 시험','B5:M29'],['final-main','보고서_유닛 시험','D32:M35'],['final-numeric','보고서_유닛 시험','G49:M51'],['final-scope','보고서_유닛 시험','G44:M44'],['final-exception','보고서_유닛 시험','G104:M105'],['final-bottom','보고서_유닛 시험','D145:M160'],['final-input','입력 및 예상값','A190:C196'],['final-rtt','RTT 결과 대조','A4:F8'],['final-observed','관찰값 확인','A244:C250']];
  for(const [name,sheetName,range]of views){
    const png=await wb.render({sheetName,range,scale:1,format:'png'});await fs.writeFile(`${qa}/${name}.png`,new Uint8Array(await png.arrayBuffer()));
  }
  console.log((await wb.inspect({kind:'region',sheetId:'보고서_유닛 시험',range:'K35:M35',maxChars:2000})).ndjson);
}else if(process.argv[2]==='before'){
  for(const [name,sheetName,range] of [['reference-cover','표지','B2:O24'],['reference-head','보고서_유닛 시험','B5:F29'],['reference-main','보고서_유닛 시험','D32:M36']]){
    const png=await wb.render({sheetName,range,scale:1,format:'png'});
    await fs.writeFile(`${qa}/${name}.png`,new Uint8Array(await png.arrayBuffer()));
  }
  console.log(wb.help('range.format.font',{include:'index,notes',maxChars:2500}).ndjson);
}else{
  const plan=JSON.parse(await fs.readFile(`${qa}/plan.json`,'utf8'));
  const template=JSON.parse(await fs.readFile(`${qa}/reference.json`,'utf8'));
  const cover=wb.worksheets.getItem('표지');
  const s=wb.worksheets.getItem('보고서_유닛 시험');
  const put=(sheet,cell,value)=>{sheet.getRange(cell).values=[[value]];};
  const red='#C00000', font='맑은 고딕';
  put(cover,'L2','SUVR-FW-TD24A');put(cover,'L3','Rev.0');
  put(cover,'B5','소프트웨어 유닛 검증 보고서\n(TD24A-A-EXT)');
  put(cover,'D12','수석연구원');put(cover,'H12','김은수');
  for(const a of ['M12','D14','H14','J14','M14','D16','H16','J16','M16','D24'])put(cover,a,null);
  put(cover,'B24',0);put(cover,'F24','최초 작성');
  cover.getRange('B25:O32').clear({applyTo:'contents'});
  // Template header, columns and fills remain; only current test rows replace old units.
  for(const a of template['보고서_유닛 시험'].merges){
    if(Number(a.match(/\d+/)[0])>=34)s.unmergeCells(a);
  }
  const bodyStyle=s.getRange('D34:M34');
  for(let r=35;r<=149;r++)s.getRange(`D${r}:M${r}`).copyFrom(bodyStyle,'all');
  s.getRange('B34:M223').clear({applyTo:'contents'});
  // Explicitly clear inherited shared formulas before writing fixed current IDs/criteria.
  s.getRange('B34:M223').formulas=Array.from({length:190},()=>Array(12).fill(''));
  put(s,'D7','TD24A-A-EXT');put(s,'D8','(시험 빌드 버전 기입)');s.getRange('D8').format.font.color=red;
  put(s,'C11','인공달팽이관 외부장치 Sound1 (TD24A-A-EXT)');
  const equipments=[
    ['EDK Ezairo / IDE','E8300 펌웨어 빌드, 다운로드 및 코드 검토'],
    ['SEGGER RTT','처리 완료 후 상태, 입력/출력 배열, 반환값을 확인'],
    ['오실로스코프','필요 시 UART RX/TX, Calibration 신호, PMIC 출력 확인'],
    ['시험 입력/응답','공유 메모리/배열의 고정 입력과 가상 장치 응답'],
    ['시험 저장소/메모리','운영 파일과 분리한 저장/삭제/OTA/기동 시험'],
    ['이어폰 / OD DAC','순음, 무음과 연속 재생을 청취'],
    ['QCC 펌웨어 자료','(QCC 검증 결과 필요) 현재는 E8300 연계 범위만 판정'],
    ['참조 기준서','SUVC-FW-TD24A 소프트웨어 유닛 검증 기준서, Rev.0'],
    ['결과 기록','시험자 확인 결과를 기입. RTT 수집 로그 원문과 구분']];
  equipments.forEach(([a,b],i)=>{put(s,`C${i+16}`,a);put(s,`D${i+16}`,b);s.getRange(`C${i+16}:F${i+16}`).format.wrapText=true;s.getRange(`C${i+16}:F${i+16}`).format.rowHeight=34;});
  s.getRange('C22:F22').format.font.color=red;
  put(s,'D27',null);put(s,'D28',null);put(s,'D29','김은수');
  s.mergeCells('G5:M8');put(s,'G5',plan.provenance);s.getRange('G5:M8').format={font:{name:font,size:10},wrapText:true,verticalAlignment:'center',horizontalAlignment:'left'};
  s.mergeCells('G10:M12');put(s,'G10','[I] Inspection : 제어 또는 시각적 확인\n[A] Analysis : 분석적인 증거에 기반한 확인\n[D] Demonstration : 정량적인 측정 없이 동작 특성 확인\n[T] Test : 정량적인 측정을 통한 정량적인 특성 확인');
  s.getRange('G10:M12').format={font:{name:font,size:10},wrapText:true,verticalAlignment:'center',horizontalAlignment:'left'};
  s.mergeCells('G14:M17');put(s,'G14','보고서의 시험 조건과 통과 기준은 현재 기준서 115항과 동일하다.\n입력별 상세값과 기존 확인 범위는 「입력 및 예상값」, RTT 전체 비교 문자열은 「RTT 결과 대조」에서 시험 ID로 찾는다.\n사진은 (사진필요), QCC 내부 증거는 (QCC 검증 결과 필요)로 표시한다.');
  s.getRange('G14:M17').format={font:{name:font,size:10},wrapText:true,verticalAlignment:'center',horizontalAlignment:'left'};
  const widths=[0,62,145,205,280,46,160,350,330,340,460,630,120];
  for(let c=1;c<=12;c++)s.getRangeByIndexes(0,c,223,1).format.columnWidthPx=widths[c];
  s.getRange('C15').format.wrapText=true;s.getRange('B15:F15').format.rowHeight=34;
  s.getRange('B34:M149').format={font:{name:font,size:10,color:'#111111'},wrapText:true,verticalAlignment:'top',horizontalAlignment:'left',borders:{preset:'all',style:'thin',color:'#B7B7B7'}};
  const rows=plan.tests.map(t=>{
    const r=t.result;
    const detail=[r.result,r.exception?`${r.exception_scope||'결과 대조'}: ${r.exception}`:'',r.photo,r.qcc].filter(Boolean).join('\n');
    return [...t.original.slice(1,11),detail,r.judgment];
  });
  s.getRange('B34:M148').values=rows;
  const lineCount=(value,width,mono=false)=>String(value??'').split('\n').reduce((n,line)=>n+Math.max(1,Math.ceil([...line].reduce((sum,ch)=>sum+(mono?7.4:/[^\x00-\x7f]/.test(ch)?13.4:7.1),0)/(width-16))),0);
  rows.forEach((row,i)=>{
    const r=i+34;
    const h=Math.max(72,...row.map((v,c)=>lineCount(v,widths[c+1])*14+16));
    if(h>409)throw new Error(`Report row too tall: ${r} ${h}`);
    s.getRange(`B${r}:M${r}`).format.rowHeight=h;
    s.getRange(`F${r}:G${r}`).format.horizontalAlignment='center';
    s.getRange(`M${r}`).format.horizontalAlignment='center';
    if(plan.tests[i].result.photo||plan.tests[i].result.qcc||plan.tests[i].result.exception)s.getRange(`L${r}`).format.font.color=red;
  });
  s.getRange('M34:M148').conditionalFormats.add('containsText',{text:'대조',format:{font:{color:red,bold:true},fill:'#FFF2F2'}});
  for(const col of [0,1]){
    let a=0;
    for(let i=1;i<=rows.length;i++)if(i===rows.length||rows[i][col]!==rows[a][col]||(col===1&&rows[i][0]!==rows[a][0])){
      if(i-a>1){const cell=String.fromCharCode(66+col);s.mergeCells(`${cell}${34+a}:${cell}${33+i}`);}
      a=i;
    }
  }
  s.getRange('B34:C148').format.verticalAlignment='center';s.getRange('B34:C148').format.horizontalAlignment='center';
  s.getRange('B149:M223').clear({applyTo:'all'});
  s.mergeCells('D149:M149');put(s,'D149','(QCC 검증 결과 필요) QCC 내부 유닛 정의와 기준이 확정되면 이 위치 다음에 해당 시험 결과를 추가한다.');s.getRange('D149:M149').format={font:{name:font,size:10,color:red},wrapText:true,rowHeight:36};
  put(s,'B152','6. 미해결 오류 및 확인할 자료');s.getRange('B152:F152').format.font={name:font,size:12,bold:true};
  s.mergeCells('D153:K153');put(s,'D153','시험자 확인에 따른 통과 / 이전 기록과 결과 대조가 필요한 유닛');
  put(s,'L153','통과');s.getRange('M153').formulas=[['=COUNTIFS(M34:M148,"통과")']];
  put(s,'L154','결과 대조 필요');s.getRange('M154').formulas=[['=COUNTIFS(M34:M148,"결과 대조 필요")']];
  let rr=156;for(const t of plan.tests.filter(t=>t.result.exception)){
    const desc=`[${t.result.exception_scope||'결과 대조'}] ${t.result.exception}`;
    put(s,`D${rr}`,t.id);s.mergeCells(`E${rr}:M${rr}`);put(s,`E${rr}`,desc);s.getRange(`D${rr}:M${rr}`).format={font:{name:font,size:10,color:red},wrapText:true,rowHeight:Math.max(42,lineCount(desc,widths.slice(4).reduce((a,b)=>a+b,0))*14+16)};rr++;
  }
  s.showGridLines=false;s.freezePanes.unfreeze();

  const addSheet=(name,headers,data,colWidths,types=[])=>{
    const sh=wb.worksheets.add(name);sh.showGridLines=false;sh.freezePanes.freezeRows(4);
    const last=String.fromCharCode(64+headers.length),end=data.length+4;
    put(sh,'A2',name);sh.getRange(`A2:${last}2`).merge();sh.getRange('A2').format.font={name:font,size:14,bold:true};
    sh.getRange(`A4:${last}4`).values=[headers];
    sh.getRange(`A4:${last}${end}`).format={font:{name:font,size:10},wrapText:true,verticalAlignment:'top',horizontalAlignment:'left'};
    sh.getRange(`A4:${last}4`).format={fill:'#E7E6E6',font:{name:font,size:10,bold:true},horizontalAlignment:'center',verticalAlignment:'center',rowHeight:28};
    sh.getRange(`A5:${last}${end}`).values=data;
    colWidths.forEach((width,c)=>{sh.getRangeByIndexes(0,c,end,1).format.columnWidthPx=width;});
    data.forEach((row,i)=>{
      const h=Math.max(24,...row.map((v,c)=>lineCount(v,colWidths[c],types.includes(c))*14+12));
      if(h>409)throw new Error(`Appendix row too tall ${name} ${i+5} ${h}`);
      sh.getRange(`A${i+5}:${last}${i+5}`).format.rowHeight=h;
    });
    for(const c of types)sh.getRangeByIndexes(4,c,data.length,1).format.font.name='Consolas';
    sh.tabColor='#9EADBA';return sh;
  };
  const input=addSheet('입력 및 예상값',['유닛 ID','시험 조건 / 구분','입력 / 예상값 / 기존 확인 범위'],plan.vectors,[130,350,1120]);
  put(input,'A3','사전 시험 자료. 이 시트의 기존 PC 관찰과 준비 상태는 신규 실측 결과가 아니다. 보고서 판정은 본문과 대조한다.');
  const log=addSheet('RTT 결과 대조',['유닛 ID','조건','입력 조건','예상 출력 전문','시험자 확인에 따른 결과 기입','기록 구분'],plan.logs,[120,170,550,510,510,340],[3,4]);
  put(log,'A3','결과란은 시험자 확인에 따라 기입했다. 실제 수집 로그는 별도 첨부한다. 오류 조건/관찰 전용/기존 불일치는 통과로 바꾸지 않았다.');
  log.getRange(`E5:F${plan.logs.length+4}`).conditionalFormats.add('containsText',{text:'필요',format:{font:{color:red}}});
  const variable=addSheet('관찰값 확인',['유닛 ID','조건','관찰값의 비교 방법'],plan.observations,[130,180,1300]);
  put(variable,'A3','실행마다 달라지는 값은 수집값 또는 정해진 관계식으로 확인한다. 실제 숫자나 수집 로그를 임의로 채우지 않는다.');
  wb.recalculate();
  console.log((await wb.inspect({kind:'match',searchTerm:'#REF!|#DIV/0!|#VALUE!|#NAME\\?',options:{useRegex:true,maxResults:20},maxChars:2000})).ndjson);
  await(await SpreadsheetFile.exportXlsx(wb)).save(`${qa}/candidate.xlsx`);
  const views=[['cover','표지','B2:O24'],['front','보고서_유닛 시험','B5:M29'],['main','보고서_유닛 시험','D32:M35'],['main-numeric','보고서_유닛 시험','G49:M51'],['main-exception','보고서_유닛 시험','G104:M105'],['summary','보고서_유닛 시험','D152:M160'],['input','입력 및 예상값','A4:C10'],['rtt','RTT 결과 대조','A4:F8'],['observed','관찰값 확인','A4:C6']];
  for(const [name,sheetName,range]of views){
    const png=await wb.render({sheetName,range,scale:1,format:'png'});await fs.writeFile(`${qa}/${name}.png`,new Uint8Array(await png.arrayBuffer()));
  }
  console.log(JSON.stringify({units:rows.length,sheets:5,output:`${qa}/candidate.xlsx`}));
}
