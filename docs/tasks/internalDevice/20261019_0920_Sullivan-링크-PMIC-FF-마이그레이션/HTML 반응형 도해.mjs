// HTML reading edition: preserve the source graph relationships as real, reflowing text.
const escape = s => String(s).replaceAll('&','&amp;').replaceAll('<','&lt;').replaceAll('>','&gt;').replaceAll('"','&quot;');
const node = (id,title,text,links=[],kind='work',short=title) => ({id,title,text,links,kind,short});
const models = {
  2: {main:['H','A','S','Q','P','B','C','R','D'],nodes:[
    node('H','백텔 읽기 준비','F는 채널당 프레임 수, S는 안정 NOP 개수입니다.',[['다음','A']]),
    node('A','ForwardCheck · F슬롯','직전 자극을 원래 펄스폭으로 끝내는 경계를 보존합니다.',[['다음','S']]),
    node('S','전원 안정 NOP','기본표·override·남은 슬롯에 맞추어 S개를 배치합니다.',[['조건 확인','Q']]),
    node('Q','F가 4 이상인가?','Duration 최소값 적용 여부를 결정합니다.',[['예','P'],['아니요 · 변경 생략','B']],'decision','F ≥ 4 판단'),
    node('P','Duration 최소값 적용','ForwardCheck 뒤에서 적용합니다.',[['이후','B']]),
    node('B','Power 백텔 읽기','프레임 조건에 맞는 조회 패킷을 배치합니다.',[['다음','C']]),
    node('C','수신 NOP · 3개','백텔을 수신할 구간을 확보합니다.',[['다음','R']]),
    node('R','변경한 Duration 원복','다음 자극이 원래 폭으로 나가도록 복원합니다.',[['다음','D']],'success'),
    node('D','프레임 경계 padding','다음 정상 자극으로 연결합니다.',[],'success')
  ]},
  3: {main:['INIT','WARM','READY','RISE','HOLD','LOW','REBASE'],note:'소스 누락·세션 재시작은 모든 상태의 공통 취소 조건입니다. 이전 요청을 정리하고 초기화/warmup부터 시작합니다. 아래 FF3는 후속 구현 참고입니다.',nodes:[
    node('INIT','초기 / 연속성 재설정','소스 사건과 세션의 시작점을 새로 정합니다.',[['초기화 후','WARM']],'work','초기화'),
    node('WARM','Warmup','현재 입력을 baseline으로 삼아 준비합니다.',[['warmup / 설정 준비','READY']]),
    node('READY','Ready · 새 상승 대기','유효한 새 상승을 기다립니다.',[['유효한 새 상승','RISE'],['소스 누락 / 세션 변경','INIT']],'work','Ready'),
    node('RISE','최고 구간 / revision 갱신','이번 상승 요청을 게시합니다.',[['게시 후','HOLD']],'work','상승 요청'),
    node('HOLD','지속 입력 유지','같은 구간의 보상을 반복해서 요청하지 않습니다.',[['같은 사건의 더 높은 구간','RISE'],['입력 하강','LOW']],'work','입력 유지'),
    node('LOW','낮아짐 확인','hold·hysteresis·low-confirm 조건을 확인합니다.',[['다시 상승 / 조건 미충족','HOLD'],['하강·재무장 조건 충족','REBASE']],'decision','하강 확인'),
    node('REBASE','재무장','이전 요청을 종결하고 필요한 High를 확인합니다. 새 baseline/event를 만들되 잔량 R은 보존합니다.',[['새 상승 대기','READY']],'success','재무장')
  ]},
  4: {main:['A','B','C','D','E','W','K','OK'],nodes:[
    node('A','FF3 snapshot 수신','CFX 관측을 한 묶음으로 가져옵니다.',[['다음','B']]),
    node('B','유효하고 실행 가능한가?','설정·세션·시각·표본과 실행 gate를 검사합니다.',[['예','C'],['아니요','X']],'decision','실행 조건 검사'),
    node('C','성공값 cache와 장부 확인','현재 P, 누적 요구 S, 처리 C, 잔량 R, budget G를 확인합니다.',[['다음','D']]),
    node('D','추가량 Δ 계산','S−C, min(S,G)−R, Pmax−P의 세 여유로 계산합니다.',[['결과 판별','E']]),
    node('E','유효한 양수 Δ인가?','이미 처리됐거나 여유가 없는 0적용과 무효 요청을 구분합니다.',[['양수','W'],['0','Z'],['무효','X']],'decision','추가량 판별'),
    node('W','PMIC 쓰기 / 성공 확인','하드웨어에 적용한 결과를 확인합니다.',[['결과','K']]),
    node('K','실제 쓰기가 성공했는가?','요청값만으로 성공값이나 장부를 갱신하지 않습니다.',[['예','OK'],['아니요','ERR']],'decision','쓰기 결과'),
    node('OK','성공량만 가산 / C=S','실제 성공량만 R에 더합니다. 부분 적용도 이번 요청은 종결합니다.',[],'success','성공 종결'),
    node('X','적용하지 않음','원인과 요청 종결 상태를 기록합니다.',[],'error','무효 요청'),
    node('Z','0적용으로 종결 / C=S','같은 요청을 나중에 자동으로 재충전하지 않습니다.',[],'success','0적용 종결'),
    node('ERR','성공 cache / 가산 금지','장부를 무효화하고 정의된 오류 복귀를 수행합니다.',[],'error','쓰기 실패')
  ]},
  5: {main:['S','A','D','N','TX'],nodes:[
    node('S','Sullivan 원본','59 20 access index value / 59 FF 버전 조회', [['도메인·index 의미를 이식','A']],'work','원본 명령'),
    node('A','Sound1 신규 제안','8F 20 링크·PMIC / 8F FF 독립 버전·지원 조회. FF 번호는 후속 예약입니다.',[['분기 등록','D']],'work','신규 명령'),
    node('D','Sound1 8F 디스패처','기존 옵션과 신규 도메인을 구분합니다.',[['기존 옵션1~4','O'],['새 옵션0x20 / 0xFF','N']],'decision','명령 분기'),
    node('N','신규 서비스 / 응답 생성','범위와 권한을 검사하고 검증된 서비스 요청을 만듭니다.',[['응답 반환','TX']],'work','신규 handler'),
    node('TX','호출자가 한 번만 송신','응답 송신과 명령 정리를 한 곳에서 수행합니다.',[],'success','단일 송신'),
    node('T','현재 Sound1 계약 보존','59 01/02 묵음, 8F 01~04 기존 디버그, 8C Gain을 보존합니다.',[['기존8F 분기 유지','D']],'neutral','기존 계약'),
    node('O','기존 handler','기존 옵션1~4의 의미·응답·부수효과를 유지합니다.',[['응답 반환','TX']],'neutral','기존 handler')
  ]},
  7: {main:['P0','P1','P2','P3','P4','P5','RUN'],note:'상태를 바꿀 때 기존 NOP와 이전 요청 정리를 유지합니다. FF 장부 복원은 후속 FF 구현 때 실제 물리 초기화 완료에 연결합니다.',nodes:[
    node('P0','PowerIC_Reset','PMIC 초기화와 ramp를 수행합니다.',[['PMIC 준비','P1']],'work','PMIC 초기화'),
    node('P1','PowerIC_OK','FPGA를 초기화합니다.',[['FPGA 준비','P2']],'work','FPGA 초기화'),
    node('P2','FPGA_Ok','내부기 전원과 응답을 확인합니다.',[['전원 확인','P3']],'work','내부기 전원 확인'),
    node('P3','ISD_Power_Ok','ID·맵·passkey와 경로를 확인합니다.',[['경로 확인','P4']],'work','경로 확인'),
    node('P4','ISD_pathOpen_Ok','자극용 10V를 준비합니다.',[['10V 확인','P5']],'work','10V 준비'),
    node('P5','stimul_10V_Ok','자극 설정과 Live 준비를 마칩니다.',[['맵 / 자극 준비','RUN']],'work','Live 준비'),
    node('RUN','운용','1차는 주기 백텔·PMIC를 실행합니다. FF는 후속 확장입니다.',[['백텔 실패 / 재시도 소진','P2'],['FPGA clear 오류','P1'],['PMIC 오류 / 사용 해제','P0']],'success','운용')
  ]},
  8: {main:['A','S','M','F','I','NORMAL','C'],nodes:[
    node('A','CM3 전경 PMIC 서비스','모든 전원 요청의 판단 경계를 둡니다.',[['다음','S']]),
    node('S','정지 / 오류 / 초기화인가?','초기화가 우선하는 상황인지 확인합니다.',[['예','INIT'],['아니요','M']],'decision','초기화 판단'),
    node('M','매핑 / OTA / 모드 제약인가?','기존 측정·연결·자극 처리와 충돌하지 않게 합니다.',[['예','MAP'],['아니요','F']],'decision','모드 판단'),
    node('F','강제 고정인가?','허용 범위의 고정 요청을 확인합니다.',[['예','FIX'],['아니요','I']],'decision','고정 판단'),
    node('I','무한 / hold 시험인가?','출력 동결 시험과 정상 링크 제어를 구분합니다.',[['예','HOLD'],['아니요','NORMAL']],'decision','동결 판단'),
    node('NORMAL','Normal 제어','1차: 최신 백텔 피드백. 후속 FF 도입 시 FF3 → 최신 백텔 → FF1 순서입니다.',[['전원 요청','C']],'success','Normal'),
    node('C','공통 적용 경계','범위·I2C·성공 여부와 되읽기를 확인합니다.',[],'success','공통 적용'),
    node('INIT','기존 초기화 / 상태 전환','reset/ramp를 우선합니다. 후속 FF 도입 시 pending도 취소합니다.',[],'neutral','초기화 우선'),
    node('MAP','기존 모드 경로와 직렬화','매핑 PMIC 하한은172가 기본입니다. OTA/측정 제약과 FIFO 소유를 보존하고 FF는 허용하지 않습니다.',[],'neutral','매핑 / OTA 경로'),
    node('FIX','허용 고정값 적용','성공값만 게시하며 FF3는 허용하지 않습니다.',[['고정 요청','C']],'neutral','고정값 적용'),
    node('HOLD','출력 동결','정상 링크 판정과 구분합니다.',[],'neutral','동결 시험')
  ]},
  9: {main:['P0','P1','P2','P3','P4','P5'],note:'개정 계획11.1/11.5절 기준입니다. 1차는 단계5에서 완료하며, 아래 FF는 별도 후속 범위입니다.',nodes:[
    node('P0','0 · 기준 고정','기존164 기반 동작·현재 빌드·시험 hook을 식별합니다.',[['다음','P1']]),
    node('P1','1 · 오류 / 공통 적용','I2C 실패·성공값·single TX를 정리합니다.',[['다음','P2']]),
    node('P2','2 · PMIC / 정책','설정최저112, Normal140, Mapping172, 상한214, coin1을 병합합니다.',[['다음','P3']]),
    node('P3','3 · 백텔 / 주기','PCM·안정 NOP·100ms 단위 설정과 Mapping IDLE/Standby를 연결합니다.',[['다음','P4']]),
    node('P4','4 · 프로토콜 / 저장 / 확장','8F20 링크·PMIC, LINKPARM, FF 미지원과 공유 tail을 구현합니다.',[['다음','P5']]),
    node('P5','5 · 1차 통합 완료','Normal·Mapping·QCC·OTA·sleep 및 빌드/실기 기준을 확인합니다.',[],'success','1차 완료'),
    node('FF1','후속 · FF1','자극값 급증 감지·쿨다운·CM3 가산을 별도 범위에서 구현합니다.',[],'neutral'),
    node('OBS','후속 · FF3 관측','입력 보정·독립 시계·session/snapshot을 전원 쓰기 없이 확인합니다.',[['관측 검증 후','APPLY']],'neutral','FF3 관측'),
    node('APPLY','후속 · FF3 실제 적용','장부·상한·최신 백텔 중재를 검증하고 적용합니다.',[],'neutral','FF3 적용')
  ]}
};
const messages = [
 ['앱','CM3','FF OFF / BEGIN'],['CM3','앱','staging 편집 시작 ACK'],
 ['앱','CM3','scalar / 구간표 개별 편집'],['CM3','앱','개별 저장 ACK'],
 ['앱','CM3','VALIDATE'],['CM3','앱','전체 관계 검사 결과'],
 ['앱','CM3','SAVE_APPLY'],['CM3','파일','직렬화 / 쓰기 / sync / close / 되읽기'],
 ['파일','CM3','길이 / CRC / 전체 값 확인'],['CM3','CFX','설정 전체와 새 token 게시'],
 ['CM3','앱','SAVE_APPLY 응답'],['CFX','CFX','같은 token의 설정 수신'],
 ['CFX','CM3','config ACK'],['앱','CM3','파일 / ACK / 장부 / 준비 조회'],
 ['CM3','앱','각 상태와 미완료 원인'],['앱','CM3','준비 확인 후 mode3 선택']
];
const architectureNodes = [
 ['APP','앱 / 시험 도구','명령과 응답'],['Q','QCC','무선 / SPI 중계'],
 ['H','HEAR','입력 peak / 신호 연산'],['C','CFX','자극 / 백텔 PCM. FF 관측·요청은 후속'],
 ['M','CM3','연결 상태 / PMIC 중재 / 프로토콜·파일'],
 ['F','FPGA','PCM 전달 / 백텔 FIFO'],['P','TX PMIC','링크 송신 전원'],
 ['R','무선 링크 / 송신 경로','기능 경로이며 별도 칩을 뜻하지 않음'],['I','내부 이식장치','자극 실행 / 백텔 반환']
];
const architectureEdges = [
 ['APP','Q','⇄','앱 명령 / 응답'],['H','C','→','블록 관측값'],
 ['C','M','→','공유 요청 / 백텔 ready'],['M','C','→','설정 / 적용 상태, FF token은 후속'],
 ['Q','M','⇄','SPI 명령 / 응답'],['C','F','→','PCM'],['M','F','⇄','I2C 판독 / 설정'],
 ['M','P','→','I2C 레벨 설정'],['P','R','→','송신 전원'],['F','R','⇄','전달 / 수신'],['R','I','⇄','전달 / 백텔']
];
function graph(n,model) {
 const names = new Map(model.nodes.map(x=>[x.id,x]));
 for(const x of model.nodes)for(const [,to]of x.links)if(!names.has(to))throw new Error('Unknown diagram target '+to);
 const id = key => `rd-${n}-${key}`;
 const ordered = model.main.map(key=>names.get(key));
 const extra = model.nodes.filter(x=>!model.main.includes(x.id));
 const card = (x,i) => `<article class="rd-node rd-${x.kind}" id="${id(x.id)}" data-node="${escape(x.id)}"><div class="rd-node-top"><span class="rd-index">${String(i+1).padStart(2,'0')}</span><span class="rd-kind">${x.kind==='decision'?'조건':x.kind==='error'?'오류 / 미적용':x.kind==='success'?'완료 / 진행':x.kind==='neutral'?'별도 분기':'처리 / 상태'}</span></div><h4>${escape(x.title)}</h4><p>${escape(x.text)}</p>${x.links.length?`<ul class="rd-transitions">${x.links.map(([when,to])=>`<li><a href="#${id(to)}"><span>${escape(when)}</span><b><span aria-hidden="true">→ </span>${escape(names.get(to).short)}</b></a></li>`).join('')}</ul>`:'<span class="rd-terminal">이 분기 종료</span>'}</article>`;
 return `${model.note?`<p class="rd-note">${escape(model.note)}</p>`:''}<p class="rd-reading">기본 진행은 연결선을 따릅니다. 분기·반복·복귀는 각 상자 아래의 <b>조건 → 목적지</b>로 표시하며, 목적지를 누르면 해당 상태로 이동합니다.</p><div class="rd-graph" data-graph="${n}"><svg class="rd-wires" aria-hidden="true" focusable="false"></svg><div class="rd-main">${ordered.map(card).join('')}</div></div>${extra.length?`<section class="rd-outcomes" aria-label="조건에 따른 별도 분기"><h4 class="rd-outcome-title">조건별 분기 / 후속 범위</h4><div class="rd-extra">${extra.map((x,i)=>card(x,ordered.length+i)).join('')}</div></section>`:''}`;
}
function architecture() {
 const lookup = new Map(architectureNodes.map(x=>[x[0],x]));
 const card = key => {const [id,title,text]=lookup.get(key);return `<article class="rd-node" id="rd-1-${id}" data-node="${id}"><h4>${escape(title)}</h4><p>${escape(text)}</p></article>`;};
 return `<div class="rd-architecture"><div class="rd-endpoint">${card('APP')}</div><div class="rd-inline-arrow" aria-hidden="true">⇅</div><section class="rd-boundary"><h4 class="rd-boundary-title">Sound1 외부장치</h4>${card('Q')}<section class="rd-boundary rd-core-boundary"><h4 class="rd-boundary-title">E8300 · 코어와 처리 유닛</h4><div class="rd-cores">${['H','C','M'].map(card).join('')}</div></section><div class="rd-hardware">${['F','P','R'].map(card).join('')}</div></section><div class="rd-inline-arrow" aria-hidden="true">⇅</div><div class="rd-endpoint">${card('I')}</div></div><section class="rd-connections"><h4>누가 누구에게 무엇을 전달하는가</h4><ul>${architectureEdges.map(([from,to,arrow,label])=>`<li><div class="rd-connection-path"><a href="#rd-1-${from}">${escape(lookup.get(from)[1])}</a><span aria-hidden="true">${arrow}</span><a href="#rd-1-${to}">${escape(lookup.get(to)[1])}</a></div><p>${escape(label)}</p></li>`).join('')}</ul></section>`;
}
function sequence() {
 return `<p class="rd-note">후속 FF 설정의 시퀀스입니다. 1차 LINKPARM 구현에서 FF staging / AUDIOFF를 활성화하는 흐름이 아닙니다.</p><div class="rd-participants">${['앱','CM3 설정 서비스','파일 저장','CFX'].map(x=>`<span>${x}</span>`).join('')}</div><ol class="rd-messages">${messages.map(([from,to,text],i)=>`<li><span class="rd-message-number">${String(i+1).padStart(2,'0')}</span><div class="rd-message-path"><b>${from}</b><span aria-label="전달">→</span><b>${to}</b></div><p>${escape(text)}</p>${i===10?'<aside class="rd-message-note">이 응답만으로 CFX 적용 완료를 뜻하지 않습니다.</aside>':i===15?'<aside class="rd-message-note">새 session · warmup · 실행 gate를 별도로 확인합니다.</aside>':''}</li>`).join('')}</ol>`;
}
export const diagramManifest = Object.fromEntries([1,2,3,4,5,6,7,8,9].map(n=>[n,n===1?{type:'architecture',nodes:architectureNodes.map(x=>x[0]),edges:architectureEdges.map(([from,to,arrow,label])=>({from,to,arrow,label}))}:n===6?{type:'sequence',participants:['앱','CM3','파일','CFX'],messages}: {type:n===3||n===7?'state':'flow',nodes:models[n].nodes.map(x=>x.id),primary:models[n].main,edges:models[n].nodes.flatMap(x=>x.links.map(([condition,to])=>({from:x.id,to,condition}))) }]));
export function renderDiagram(n) {
 if(n===1)return architecture();
 if(n===6)return sequence();
 if(!models[n])throw new Error('Unknown diagram '+n);
 return graph(n,models[n]);
}
