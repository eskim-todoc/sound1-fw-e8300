import fs from 'node:fs/promises';
import path from 'node:path';
import {Workbook,SpreadsheetFile,FileBlob} from '@oai/artifact-tool';
const base=path.join(process.cwd(),'tmp/sound1-32ch');
const m=JSON.parse(await fs.readFile(path.join(base,'model.json'),'utf8'));
const wb=Workbook.create();
const main=wb.worksheets.add('32전극 결과'), calc=wb.worksheets.add('전류 연산'), data=wb.worksheets.add('모델 데이터'), refs=wb.worksheets.add('맵 및 근거');
const navy='#173550',blue='#245B78',pale='#E8F2F5',yellow='#FFF0BB';
const col=n=>String.fromCharCode(64+n);
function band(s,r,t){s.mergeCells(r);s.getRange(r).values=[[t]];s.getRange(r).format={fill:navy,font:{name:'맑은 고딕',size:14,color:'#FFFFFF',bold:true},rowHeight:32};}
function head(s,r,values){s.getRange(r).values=[values];s.getRange(r).format={fill:blue,font:{name:'맑은 고딕',size:10,color:'#FFFFFF',bold:true},rowHeight:34,wrapText:true};}
function widths(s,ws){ws.forEach((w,i)=>s.getRange(`${col(i+1)}:${col(i+1)}`).format.columnWidth=w);}
function note(s,r,t){s.mergeCells(r);s.getRange(r).values=[[t]];s.getRange(r).format={wrapText:true,rowHeight:34,font:{name:'맑은 고딕',size:10,color:'#49545D'}};}
for(const s of [main,calc,data,refs]){s.getRange('A1:R60').format.font.name='맑은 고딕';s.getRange('A1:R60').format.font.size=10;}
band(main,'A1:L1','Sound1 | ID-6 · ID-7 32전극 예상 전류');
note(main,'A2:L2','노란 셀 3개 선택 → 32개 전극의 PCM Amplitude와 전류가 함께 갱신됩니다. 실측값·합격 허용차가 아닌 코드 기반 계산 예상값입니다.');
main.getRange('A4:B6').values=[['주파수 [Hz]',3815],['음압 [dBSPL]','69~71'],['오디오 볼륨',1]];
main.getRange('B4:B6').format={fill:yellow,font:{color:'#0000FF',bold:true,size:12},rowHeight:27};
main.dataValidations.add({range:'B4',rule:{type:'list',formula1:'"3815,100,114,128,2869,3038,3217"'}});
main.dataValidations.add({range:'B5',rule:{type:'list',formula1:'"74~76,69~71,119~120,34~36,40~42,98~110"'}});
main.dataValidations.add({range:'B6',rule:{type:'list',formula1:'"1,10"'}});
note(main,'D4:L4','고정 맵: 프로그램 1 · 32-band CIS · 자극 볼륨 4 · 펄스폭 50 us · 음(-) 선행');
note(main,'D5:L5','16번 전극은 기본 맵 T=C=1,500 uA입니다. 코드 환산 전류는 1,492 uA로 고정됩니다.');
note(main,'D6:L6','98~110은 추가 범위입니다. 119~120은 110 dBSPL 사양 밖의 비교용 외삽이며 합격 판정에 사용하지 않습니다.');
main.getRange('A8:C8').values=[['ADC 하한','ADC 중간 음압','ADC 상한']];
for(let i=0;i<3;i++)main.getRange(`${col(i+1)}9`).formulas=[[`=INDEX('맵 및 근거'!$E$5:$G$10,MATCH(VALUE(LEFT(B5,FIND("~",B5)-1)),'맵 및 근거'!$B$5:$B$10,0),${i+1})`]];
main.getRange('A8:C9').format.fill=pale;main.getRange('A9:C9').setNumberFormat('#,##0');
note(main,'D8:L9','하/중/상: 계산 최소 / 중간 음압의 시간 중앙값 / 계산 최대. 잡음·음향 주파수 응답·DAC 실측 오차 제외.');
main.getRange('A11:B11').values=[['조건 번호',null]];
main.getRange('B11').formulas=[[`=(MATCH(B4,'맵 및 근거'!$J$5:$J$11,0)-1)*12+(MATCH(VALUE(LEFT(B5,FIND("~",B5)-1)),'맵 및 근거'!$B$5:$B$10,0)-1)*2+IF(B6=1,1,2)`]];
head(main,'A13:L13',['전극','FFT 대역 [Hz]','PCB 주소','T [uA]','C [uA]','PCM 하','PCM 중','PCM 상','전류 하 [uA]','전류 중 [uA]','전류 상 [uA]','확인 대상']);
for(let e=1;e<=32;e++){
 const r=e+13,c=e+4,b=m.bandbins[e-1];
 main.getRange(`A${r}:E${r}`).values=[[e,`${b[0]*31.25}~${b.at(-1)*31.25}`,m.pcb[e-1],e===16?1500:500,1500]];
 for(let j=0;j<6;j++)main.getRange(`${col(6+j)}${r}`).formulas=[[`='전류 연산'!${col(10+j)}${c}`]];
 main.getRange(`L${r}`).values=[[e===16?'T=C 고정':e===26?'ID-6':([1,2,3,23,24,25].includes(e)?'ID-7 주요':'전체 비교')]];
 main.getRange(`A${r}:L${r}`).format.rowHeight=23;
 if(e%2===0)main.getRange(`A${r}:L${r}`).format.fill=pale;
 if(e===16)main.getRange(`A${r}:L${r}`).format.fill='#FCE4D6';
}
main.getRange('I14:K45').setNumberFormat('#,##0');main.getRange('J14:J45').format.font.bold=true;
note(main,'A47:L47','조건: 믹서 출력에 Sound1 ADC 표 진폭의 정현파를 인가한 모델. 2초 안정화 후 1초 / 1ms 간격 FFT, 음압 1dB 격자+중간값에서 관측한 범위입니다.');
note(main,'A48:L48','HEAR FFT와 DSP log/power는 이상 수학 근사입니다. 비트 단위 일치·연속 음압/모든 위상 최악값을 보장하지 않습니다. 34~36 dBSPL은 ADC 5~6으로 양자화 영향이 큽니다.');
note(main,'A49:L49','시험은 음압계로 먼저 맞추고 ADC를 함께 기록하십시오. maxAudioInput은 16샘플 최대값의 순간 스냅샷이므로 매 무선 응답이 이 진폭 범위에 머무는 조건은 아닙니다.');
widths(main,[13,23,12,11,11,11,11,11,15,15,15,15]);main.freezePanes.freezeRows(13);

band(refs,'A1:J1','맵 · 환산 상수 · 모델 적용 범위');
head(refs,'A4:G4',['음압 범위','하한','상한','중간','ADC 하한','ADC 중간','ADC 상한']);
m.ranges.forEach((x,i)=>{const r=i+5;refs.getRange(`A${r}:D${r}`).values=[[x.label,x.lo,x.hi,x.mid]];['B','D','C'].forEach((c,j)=>refs.getRange(`${col(5+j)}${r}`).formulas=[[`=INT(262143*10^((${c}${r}-128.293)/20))`]]);});
refs.getRange('J4:J11').values=[['주파수 [Hz]'],...m.freqs.map(f=>[f])];
head(refs,'A13:C13',['상수','값','의미 / 근거']);
const constants=[['offset reg',109,'stimulationParaCal.c: 500*7185 >>15'],['offset 실현값',497,'18677*109 >>12'],['current slope',19333,'Q12 DAC C 환산: amplitude당 4.7200 uA'],['xMax',21892,'ci_map.c 프로그램 1'],['xMin base',8,'ci_map.c 프로그램 1'],['p',1258291,'Q1.23: x^0.15'],['x^p scale',5163793,'Q8.16'],['C PCM',211,'CM3 기본 맵 레벨'],['volume1 gain',32768,'Q9.15: xMin 보정'],['volume10 gain',981153,'Q9.15: xMin 보정']];
refs.getRange('A14:C23').values=constants;
const notes=[
'ADC: [참고] Sound1 입력 음압 대비 ADC 출력 테이블.xlsx, 완제품 감쇠 -8.293 dB. ADC=INT(262143*10^((dBSPL-128.293)/20)).',
'맵: src/2__cm3/Gen1_5/FS/ci_map.c 프로그램 1. T500/C1500, 단 전극16 T=C1500. 프로그램을 바꾸면 이 결과표를 적용하지 않습니다.',
'CM3: stimulationParaCal.c의 offset/StimDAC/T/C 정수 변환. 표시 전류=497+INT(19333*PCM/4096), 물리 DAC 측정치가 아닌 코드 환산값.',
'CFX: signalProcessing/agc.c → FrequencyAnalysis.c → nonlinearMapping.c → stimulationStrategy.c. Hann/32-band: src/2__cm3/Gen1_5/FS/ci_fft.c.',
'16kHz, 16샘플 AGC, 512점 FFT, 정확한 Hann 계수/32-band passbin 적용. FFT magnitude는 abs(DFT)/128 근사; bin0~3 및253~255 제외.',
'각 주파수·음압·볼륨에서 위상0 정현파를 floor 양자화, 2초 안정화, 이후 1초 1000개 창. 음압 정수 격자+중간값; 전 구간 보증 오차가 아닙니다.',
'log/power 라이브러리 LUT 및 HEAR 내부 정수 반올림은 이상 함수로 근사. 본 표는 시험 준비용 예상이며 실기 측정/허용차 확정이 필요합니다.',
'음압계 교정, 스피커/마이크/기구의 주파수 응답, 잡음, 방향 오차는 미포함. ADC를 믹서 출력 진폭으로 가정하여 음향 전달함수를 중복 적용하지 않습니다.',
'119~120 dBSPL은 최대 사양110 초과 비교용 외삽. 34~36에서는 ADC 양자화가 커서 40~42의 추가 조건도 함께 비교하십시오.'
];notes.forEach((x,i)=>note(refs,`A${26+i*2}:J${27+i*2}`,x));widths(refs,[23,17,23,16,16,16,16,10,10,18]);

band(data,'A1:H1','호스트 계산 중간값 | 84조건 × 32전극');
note(data,'A2:H2','AGC·FFT의 대역 대표값입니다. 최종 전류는 이 숫자를 입력으로 전류 연산 시트의 Excel 수식에서 계산합니다.');
head(data,'A4:H4',['조건','주파수','음압 범위','볼륨','전극','대표값 하','대표값 중','대표값 상']);
data.getRange(`A5:H${m.rows.length+4}`).values=m.rows;data.freezePanes.freezeRows(4);widths(data,[12,16,19,12,12,19,19,19]);

band(calc,'A1:R1','정수 비선형 매핑 → PCM → 전류 | 선택 조건의 재계산');
note(calc,'A2:R2','x^p = INT(INT(10^(INT(INT(10*LOG10(x*32/2^47)*65536)*1258291/2^23)/65536/10)*2^23)*5163793/2^23). x<1이면 0.');
head(calc,'A4:R4',['전극','원본 행','대표값 하','대표값 중','대표값 상','x^p 하','x^p 중','x^p 상','T PCM','PCM 하','PCM 중','PCM 상','uA 하','uA 중','uA 상','계수 A','계수 B','xMin']);
const xp=x=>`IF(${x}<1,0,INT(INT(10^(INT(INT(10*LOG10(MAX(1,${x})*32/2^47)*65536)*1258291/2^23)/65536/10)*2^23)*5163793/2^23))`;
for(let e=1;e<=32;e++){const r=e+4;
 calc.getRange(`A${r}`).values=[[e]];
 calc.getRange(`B${r}`).formulas=[[`=('32전극 결과'!$B$11-1)*32+A${r}`]];
 for(let j=0;j<3;j++){
  calc.getRange(`${col(3+j)}${r}`).formulas=[[`=INDEX('모델 데이터'!$F$5:$H$2692,B${r},${j+1})`]];
  calc.getRange(`${col(6+j)}${r}`).formulas=[[`=${xp(col(3+j)+r)}`]];
  calc.getRange(`${col(10+j)}${r}`).formulas=[[`=MAX(I${r},MIN('맵 및 근거'!$B$21,INT((INT(P${r}*${col(6+j)}${r}/65536)+Q${r})/256)))`]];
  calc.getRange(`${col(13+j)}${r}`).formulas=[[`='맵 및 근거'!$B$15+INT('맵 및 근거'!$B$16*${col(10+j)}${r}/4096)`]];
 }
 calc.getRange(`I${r}`).formulas=[[`=IF(A${r}=16,'맵 및 근거'!$B$21,0)`]];
 calc.getRange(`R${r}`).formulas=[[`=INT('맵 및 근거'!$B$18*IF('32전극 결과'!$B$6=1,'맵 및 근거'!$B$22,'맵 및 근거'!$B$23)/32768)`]];
 calc.getRange(`P${r}`).formulas=[[`=INT(('맵 및 근거'!$B$21-I${r})*2^24/(${xp("'맵 및 근거'!$B$17")}-${xp('R'+r)}))`]];
 calc.getRange(`Q${r}`).formulas=[[`='맵 및 근거'!$B$21*256-INT(P${r}*${xp("'맵 및 근거'!$B$17")}/65536)`]];
}widths(calc,Array(18).fill(13));calc.freezePanes.freezeRows(4);

const qa=[];
for(const c of m.cases){main.getRange('B4:B6').values=[[c.f],[`${c.lo}~${c.hi}`],[c.vol]];wb.recalculate();const got=main.getRange('I14:K45').values;if(JSON.stringify(got)!==JSON.stringify(c.currents))throw Error(`current mismatch case ${c.case}: ${JSON.stringify(got)} vs ${JSON.stringify(c.currents)}`);qa.push(c.case);}
main.getRange('B4:B6').values=[[3815],['69~71'],[1]];wb.recalculate();
await fs.writeFile(path.join(base,'84case-check.json'),JSON.stringify({passed:qa.length,total:84}));
for(const [s,range] of [['32전극 결과','A1:L49'],['전류 연산','A1:R36'],['모델 데이터','A1:H36'],['맵 및 근거','A1:J43']]){
 const preview=await wb.render({sheetName:s,range,scale:1,format:'png'});await fs.writeFile(path.join(base,s+'.png'),new Uint8Array(await preview.arrayBuffer()));
}
const errors=await wb.inspect({kind:'match',searchTerm:'#REF!|#DIV/0!|#VALUE!|#NAME\\?|#N/A|#NUM!',options:{useRegex:true,maxResults:30},maxChars:3000});
await fs.writeFile(path.join(base,'errors.txt'),errors.ndjson);
const output=path.join(process.cwd(),'docs/참고/인허가/사운드1/Sound1 ID-6 ID-7 32채널 예상 전류표.xlsx');
await (await SpreadsheetFile.exportXlsx(wb)).save(output);
const reopened=await SpreadsheetFile.importXlsx(await FileBlob.load(output));reopened.recalculate();
const got=reopened.worksheets.getItem('32전극 결과').getRange('I14:K45').values;
const expected=m.cases.find(c=>c.f===3815&&c.lo===69&&c.vol===1).currents;
if(JSON.stringify(got)!==JSON.stringify(expected))throw Error('reopen mismatch');
console.log(JSON.stringify({output,checked:qa.length,errors:errors.ndjson,reopened:true}));
