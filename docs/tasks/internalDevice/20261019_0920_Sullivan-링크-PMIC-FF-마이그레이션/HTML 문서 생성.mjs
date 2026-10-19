import fs from 'node:fs/promises';
import path from 'node:path';
import crypto from 'node:crypto';
import {fileURLToPath, pathToFileURL} from 'node:url';
import {createRequire} from 'node:module';
import {renderDiagram, diagramManifest} from './HTML 반응형 도해.mjs';

// Regenerate this reading edition from the evidence-based Markdown source.
const root = path.dirname(fileURLToPath(import.meta.url));
const require = createRequire('C:/Users/EUNSU/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/package.json');
const {marked} = require('marked');
const source = await fs.readFile(path.join(root, '분석과 계획.md'), 'utf8');
const implementation = (await fs.readFile(path.join(root, '빌드와 실기 확인.md'), 'utf8')).replace(/^# /m, '## ');
const baseline = JSON.parse(await fs.readFile(path.join(root, '분석 기준.json'), 'utf8'));
const sourceHash = crypto.createHash('sha256').update(source).digest('hex');
const refs = source.slice(source.indexOf('[system]:'));
const body = source.slice(0, source.indexOf('[system]:'));
const starts = [...body.matchAll(/<a id="([^"]+)"><\/a>\s*\n## (\d+)\. ([^\n]+)/g)];
const chapters = {};
starts.forEach((m, i) => { chapters[+m[2]] = body.slice(m.index, starts[i + 1]?.index ?? body.length); });
if (starts.length !== 14) throw new Error('Expected fourteen source chapters.');
const state = chapters[10];
const stateA = state.slice(0, state.indexOf('### 10.2'));
const stateB = state.slice(state.indexOf('### 10.2'), state.indexOf('### 10.3'));
const stateC = state.slice(state.indexOf('### 10.3'));
const esc = s => String(s).replaceAll('&', '&amp;').replaceAll('<', '&lt;').replaceAll('>', '&gt;').replaceAll('"', '&quot;');

const diagrams = [
  ['01-architecture', 'CFX·CM3·FPGA·PMIC의 역할', '전체 확장 위치를 나타낸 구조도. 1차는 링크·PMIC이며 FF 모듈은 후속 구현 대상입니다.'],
  ['02-backtel-layout', '백텔 PCM 배치 순서', 'ForwardCheck 이후에 Duration을 변경하고, 끝에서 원복합니다. 코드상 순서이며 실측 파형이 아닙니다.'],
  ['03-ff-input-states', 'FF3 입력 상태 다이어그램', 'warmup → 상승 감지 → 최고 구간 유지 → 낮아짐 확인 → 재무장. 재무장이 PMIC 하강을 뜻하지는 않습니다.'],
  ['04-ff-ledger-flow', 'FF3 요청 검사와 보상 장부', '요청의 신선도와 세 가지 여유를 검사한 뒤, 실제 쓰기 결과에 따라 장부를 갱신합니다.'],
  ['05-protocol-routing', '기존 명령과 신규 도메인', '1차에 구현한 Sound1 0x8F/0x20과 후속 FF 도메인 예약을 구분합니다.'],
  ['06-config-sequence', '설정 편집·저장·CFX ACK', 'SAVE_APPLY 성공과 CFX 수신 완료, 실제 전압 적용을 각각 확인하는 제안 흐름입니다.'],
  ['07-link-states', '내부기 초기화와 운용 상태', '기존 초기화 순서를 보존하고, 신규 FF는 준비가 끝난 뒤에 연결합니다.'],
  ['08-power-arbitration', 'PMIC 요청 중재 흐름', 'PMIC 공통 적용 경계의 제안입니다. 1차의 Normal은 백텔 제어만 실행하며 그림의 FF 분기는 후속 확장입니다.'],
  ['09-migration-stages', '단계별 이식 흐름', '개정된 1차 단계0–5와 별도 FF 후속 범위입니다. 현재 계획11.1/11.5절에 맞춰 표시했습니다.'],
];
function figure(n) {
  const [, title, caption] = diagrams[n - 1];
  return `<figure class="diagram rd-figure" id="figure-${n}" data-renderer="html"><div class="figure-top"><span class="eyebrow">도해 ${String(n).padStart(2, '0')}</span><span class="rd-reflow-label">창 너비에 맞춰 재배치</span></div><h3>${title}</h3><div class="rd-diagram">${renderDiagram(n)}</div><figcaption>${caption}</figcaption></figure>`;
}
function steps(items, cls = '') {
  return `<ol class="flow ${cls}">${items.map(([title, text]) => `<li><strong>${title}</strong><span>${text}</span></li>`).join('')}</ol>`;
}
function notice(title, text, type = '') {
  return `<aside class="notice ${type}"><strong>${title}</strong><p>${text}</p></aside>`;
}
function compare(heads, rows, caption = '') {
  return `<div class="table-scroll" tabindex="0" role="region" aria-label="${esc(caption || heads.join(' · '))}"><table>${caption ? `<caption>${caption}</caption>` : ''}<thead><tr>${heads.map(h => `<th scope="col">${h}</th>`).join('')}</tr></thead><tbody>${rows.map(row => `<tr>${row.map((cell, i) => i === 0 ? `<th scope="row">${cell}</th>` : `<td>${cell}</td>`).join('')}</tr>`).join('')}</tbody></table></div>`;
}
function detail(markdown, label) {
  let md = markdown.replace(/^!\[[^\]]*\]\([^\n]+\)\s*\n\n그림 \d+\.[^\n]*\n/gm, '');
  md = md.replace(/^(#{2,3}) /gm, '$1# ');
  let html = marked.parse(md + '\n\n' + refs);
  html = html.replace(/href="([A-Za-z]:\/[^\"]*)"/g, (_, p) => `href="${pathToFileURL(p).href}"`);
  html = html.replace(/<table>/g, '<div class="table-scroll" tabindex="0" role="region" aria-label="상세 분석 표"><table>').replace(/<\/table>/g, '</table></div>');
  html = html.replace(/<th>/g, '<th scope="col">');
  return `<details class="analysis-detail"><summary><span>${label}</span><small>현재 코드·조건·예외·근거를 모두 보기</small></summary><div class="source-content">${html}</div></details>`;
}

const overview = `
<div class="lead"><span class="tag proposal">1차 구현 · 실기 대기</span><h2>링크·PMIC를 이식하고,<br>SDK·호스트 검사를 마쳤습니다.</h2><p>Sound1에도 백텔 확인과 PMIC 조정의 골격이 있습니다. 현재 흐름을 보존하며 링크·백텔·PMIC를 먼저 옮겼고, FF는 확장할 경계만 남겼습니다.</p></div>
<div class="summary-grid"><article><span class="card-number">01</span><h3>연결 흐름 보존</h3><p>내부기 초기화, 맵 적용, QCC 알림과 Live 진입 순서를 유지합니다.</p></article><article><span class="card-number">02</span><h3>PMIC 적용은 CM3</h3><p>CFX는 백텔 PCM을 만들고 CM3는 돌아온 상태와 정책으로 전원을 조정합니다.</p></article><article><span class="card-number">03</span><h3>FF는 후속 확장</h3><p>감지·장부·전용 타이머는 구현하지 않습니다. PMIC 적용 경계와 번호·공유 확장만 준비합니다.</p></article></div>
${compare(['1차 설정', '기본값 / 범위', '의미'], [['운영 설정 최저','<b>2.8V · 코드112</b>','Normal/Mapping 하한과 고정값의 설정 최저. reset/ramp 예외'], ['Normal 하한','<b>3.5V · 코드140</b>','전원 여유 시 하강을 멈추는 기본값'], ['Mapping 하한','<b>4.3V · 코드172</b>','전용/Live 조정 경로에 함께 적용'], ['백텔 주기','<b>기본300ms · 100ms 단위</b>','1–255 설정. Mapping 검사 재시작에도 반영'], ['재시도','<b>Normal/Live 모드0 기본1coin</b>','정상 백텔로 재충전. Mapping 전용은 원본처럼 실패 시 재초기화']], '1차 구현값 · 유효 저장값 우선 / 실기 미실행')}
<section class="section"><div class="section-heading"><span class="eyebrow">기능 관계</span><h3>백텔은 돌아온 상태를 보고, FF는 커지는 신호를 봅니다.</h3></div>
<div class="system-map"><div class="chip cfx"><span class="eyebrow">E8300 · CFX / HEAR</span><h4>신호 처리와 요청 생성</h4><div class="chip-row"><span>입력 peak<br><b>FF3 · 후속</b></span><span>자극값 급증<br><b>FF1 · 후속</b></span><span>백텔 조회<br><b>PCM 생성</b></span></div><p>공유 메모리로 요청·표본·설정을 교환</p></div><div class="connector" aria-hidden="true">↓</div><div class="chip cm3"><span class="eyebrow">E8300 · CM3</span><h4>연결 상태와 PMIC 제어</h4><p>요청 유효성 → 우선순위 → I2C 쓰기 → 성공값/장부 갱신</p><div class="chip-row"><span>FPGA FIFO<br><b>백텔 수신</b></span><span>PMIC<br><b>출력 레벨</b></span></div></div><p class="map-note">FPGA/무선 링크를 통해 내부기에 명령을 보내고 백텔을 받습니다. PMIC는 무선 송신 경로의 전원을 조절합니다.</p></div>
</section>
${compare(['기능', '무엇을 보는가', '언제 조정하는가', '이식의 핵심'], [['링크·백텔', '내부기가 돌려준 전원 상태 / 수신 여부', '백텔 판독 후', '1차 · PCM 순서·주기·프레임 가드·유효 표본'], ['PMIC', '상태·설정·유효한 실제 성공값', 'CM3의 공통 적용 경계', '초기화와 Normal 범위 분리, 실패 전파'], ['FF1 · 후속', '계산된 자극 amplitude 합의 증가', '급증 flag를 CM3가 소비할 때', '감지·쿨다운·보류·상한 처리'], ['FF3 · 후속', 'AGC 전 혼합 입력 16표본 peak', '유효한 새 상승 요청일 때', '입력 보정·시계·snapshot·ACK·보상 장부']], '기능별 차이')}
${notice('이번 결과의 범위', '2026-10-19 현재 작업본의 분석과 <b>미구현 제안</b>입니다. 펌웨어 수정·타깃 빌드·장치 송수신·전압/링크 실측을 수행한 결과는 아닙니다.', 'important')}
${figure(1)}
${detail(chapters[1] + '\n' + chapters[2], '상세 분석 · 1–2장: 이식 범위와 코어별 역할')}`;

const link = `
<div class="lead"><span class="tag current">현재 코드 + 이식 제안</span><h2>조회 PCM부터 결과 소비까지,<br>백텔 한 회차를 함께 옮깁니다.</h2><p>백텔은 내부기가 돌려주는 정보입니다. CFX가 조회를 준비하고 CM3가 FPGA FIFO에서 결과를 읽습니다. 주기는 100ms 단위로 설정하고 기본300ms를 사용합니다.</p></div>
${steps([['CFX · 조회 생성', 'Live 자극 뒤에 백텔 PCM 배치'], ['FPGA / 내부기', '조회 전달과 백텔 반환'], ['CM3 · 유효 판독', 'FIFO·I2C 성공·표본 구분'], ['CM3 · 조정 / 정리', 'PMIC 조정, FIFO·ready 정리']])}
<section class="section"><div class="section-heading"><span class="eyebrow">Sullivan 시퀀스</span><h3>Duration 변경은 ForwardCheck 뒤에, 복원은 다음 자극 전에</h3></div>
${steps([['ForwardCheck', '직전 자극의 경계 보존'], ['안정 NOP', '기본표 + override + 슬롯 여유'], ['Duration 최소 / 읽기', 'F≥4 조건을 보존'], ['수신 NOP 3개', '백텔 수신 구간'], ['Duration 복원', '프레임 padding 후 다음 자극']], 'compact')}
</section>
${compare(['비교', 'Sound1 현재', 'Sullivan', '권장 처리'], [['프레임 가드', 'F>12이면 검사 생략', 'F≥10이면 검사 생략', '<b>F1–9 새 경로, F10–12 기존 fallback 검토</b>'], ['주기', '상수300 / 실제301회 Live 호출', '100ms 단위 runtime 설정 / 같은 증가식', '설정값·호출수·실측시간 구분'], ['이전 결과 미소비', '다음 검사가 반드시 기다리지는 않음', '짧은 주기와 CM3 지연을 함께 검토', '검사 순번/ACK 또는 유한 대기·timeout'], ['검사 생략·오류', '정상 링크의 근거가 될 수 없음', '실패를 Low/High로 오인하면 안 됨', '오래된 결과와 통신 실패를 따로 표시']], '조용히 동작 범위를 줄이지 않기')}
${notice('24슬롯·1.25ms는 코드 배치 계산', 'PCM은 1ms당 24슬롯입니다. 회로 ON 구간 30슬롯은 코드상 1.25ms로 계산되며, 실제 파형이나 안정화 시간의 측정값이 아닙니다.')}
${notice('Mapping의 두 검사 경로도 함께 병합', '원본도 Mapping IDLE/Standby의 일부 주기는 고정300ms이고, 전용 백텔 검사에는 coin이 없습니다. 이번 안은 주기 설정을 Mapping 진입점에도 연결하되 측정 중 검사 중지는 유지하고, 전용 실패의 재초기화 정책은 원본대로 둡니다.')}
<div class="figure-grid">${figure(2)}${figure(7)}</div>
${detail(chapters[3] + '\n' + stateA, '상세 분석 · 3장·10.1절: PCM·프레임 가드·초기화 상태')}`;

const pmic = `
<div class="lead"><span class="tag proposal">1차 목표 · 구현 전</span><h2>레벨의 범위와 적용 순서를 먼저 정하고,<br>성공한 쓰기만 실제값으로 기록합니다.</h2><p>설정 가능한 최저값은112(2.8V), Normal 하한의 기본값은140(3.5V), Mapping 하한의 기본값은172(4.3V)로 정합니다. 상한214는 유지합니다.</p></div>
<div class="level-range"><div><span>운영 설정의 최저</span><strong>112</strong><small>코드식 2.8V</small></div><div class="range-main"><span>Normal 기본 하한 → 기본 상한</span><div class="range-line"><b>140</b><span></span><b>214</b></div><div class="range-labels"><small>3.5V</small><small>5.35V</small></div></div></div><p class="caption">전압은 소스 주석식 V = 0.025 × level로 계산한 예시입니다. 보드 허용값이나 실측 전압을 대신하지 않습니다.</p>
${notice('초기화와 Normal을 같은 clamp로 묶지 않기', 'reset75=1.875V는 운영 설정 최저112보다 낮습니다. 모든 쓰기를112–214로 자르지 않고 reset/ramp의 기존 초기화 경로를 구분합니다. Normal3.5V는 기동 고정 출력이 아니라 기본 감소 정지선입니다.', 'important')}
${compare(['백텔 결과', 'Sound1 현재', 'Sullivan 모드0'], [['LowUnstable', '유지', '설정 step 상승'], ['LowStable', '+1', '설정 step 상승 · 기본4'], ['HighUnstable', '유지', '−1'], ['HighStable', '−1', '−1'], ['미수신', '오류 / 재초기화', 'coin을 소모해 상승·재시도, 소진 시 재초기화'], ['TooMuch', '오류 / 재초기화', '재시도 시 상승하지 않음']], '같은 이름이어도 제어 정책이 다릅니다')}
<div class="two-up"><article class="info-card"><h3>제품 초기 권장</h3><p>전원 상태 모드0 · 고정 해제 · 무한 해제 · FF 미구현. Normal140 / Mapping172 / 상한214, 운영 설정 최저112를 사용합니다.</p></article><article class="info-card"><h3>Mapping도 별도 하한으로 조정</h3><p>기본172=4.3V. 전용/Live의 두 경로를 함께 맞추고 Low +4, High −1을 적용합니다. Normal 복귀 때3.5V로 즉시 강제 하강시키지 않습니다.</p><h3>수신 여부 모드1</h3><p>Low/High를 사용하지 않는 시험용 탐색 정책입니다. 고정/무한과의 조합은 연결 상실 판단을 지연시킬 수 있습니다.</p></article></div>
<section class="section"><h3>모드1 가속의 코드상 예시</h3><p>donor의 하한140, 상한214, step4에서 가속 ON일 때의 레벨 순서입니다. Sound1 제품의 권장 전압 순서가 아닙니다.</p><div class="number-track" aria-label="가속 상승 레벨">${[140,144,148,156,164,176,188,204,214].map((n, i) => `<span><small>${i ? i + '회 상승' : '시작'}</small><b>${n}</b></span>`).join('')}</div><p class="caption">8회 상승으로 상한 도달 → 상한에서 다음 미수신까지 총 9회 판정. 가속 OFF이면 총 20회입니다.</p></section>
${figure(8)}
${detail(chapters[4] + '\n' + stateB, '상세 분석 · 4장·10.2절: 모드·coin·고정·safe boot·중재')}`;

const ff1 = `
${notice('1차에는 FF1을 구현하지 않습니다', '아래는 후속에 참고할 원본 동작입니다. 현재 단계에서는 자극값 감지·flag·pending·PMIC 추가 상승을 넣지 않습니다.')}
<div class="lead"><span class="tag source">후속 확장 참고 · 1차 미구현</span><h2>FF1은 자극값이 급격히 커질 때<br>작은 전원 상승을 요청합니다.</h2><p>FF는 feedforward입니다. FF1은 AGC·주파수 처리 이후 계산된 자극 amplitude의 합을 봅니다. 프로토콜의 버전 조회 바이트 0xFF와는 다른 의미입니다.</p></div>
${steps([['자극값 합계', 'CIS 활성 밴드 / NofM 선택16개'], ['직전 값과 비교', '직전 평균에 따른 임계 초과'], ['flag / pending', 'boolean 요청 · 큐가 아님'], ['CM3 조건 검사', 'Normal·연결·모드0·고정/무한 해제'], ['성공 시 +step', '기본1 · I2C 실패는 성공 아님']])}
<div class="two-up"><section class="info-card"><h3>임계값</h3>${compare(['직전 채널당 평균', '증가율 임계'], [['0–63','5%'],['64–127','10%'],['128–191','15%'],['192–255','20%']])}</section><section class="example-card"><span class="eyebrow">계산 예 · 실측 아님</span><h3>1600 → 1760</h3><p>CIS 32채널, 직전 평균 50.<br>증가율 10%가 임계 5%를 초과합니다.</p><div class="equation">PMIC 180 <span>→</span> 181</div><p>다른 조건이 맞고 쓰기가 성공했을 때 기본 step 1을 적용합니다.</p><small>코드식 4.500V → 4.525V</small></section></div>
${notice('설정 변경을 소리의 상승으로 오인하지 않기', '기본 cooldown은 50ms입니다. 맵·볼륨 변경 직후에는 비교를 한 번 건너뜁니다. I2C가 바쁘면 pending을 보류하고, 실패한 되읽기/쓰기를 성공으로 기록하지 않습니다.')}
${compare(['비교', 'FF1', 'FF3'], [['관측 위치', '계산된 자극값', 'AGC 전 혼합 입력 peak'], ['요청 표현', '상승 flag / 고정 step', 'event·revision·구간 누적량'], ['필수 관리', '임계·쿨다운·pending', '입력 보정·독립 시계·snapshot·장부'], ['도입 순서', '앞선 단계에서 적용 흐름 검증', '관측 검증 후 실제 적용 허용']], 'FF3와의 차이')}
${detail(chapters[5], '상세 분석 · 5장: FF1 계산과 CM3 적용 조건')}`;

const ff3 = `
${notice('1차에는 FF3를 구현하지 않습니다', '도해와 계산기는 후속 기능을 이해하기 위한 설명입니다. 입력 감지·TIMER0·snapshot·보상 장부·AUDIOFF 저장을 현재 단계에 구현하지 않습니다.')}
<div class="lead"><span class="tag source">후속 확장 참고 · Sound1 미구현</span><h2>FF3는 입력 상승을 먼저 감지하고,<br>아직 적용하지 않은 만큼만 보정합니다.</h2><p>HEAR의 최근 16표본 혼합 입력 절대값 최대치를 AGC 전에 읽습니다. raw값은 dB나 RMS가 아니므로 Sound1 입력 소스별 보정이 선행돼야 합니다.</p></div>
${steps([['warmup', '처음 소리를 baseline으로 관측'], ['상승 감지', '새 최고 구간 / revision'], ['유지', '같은 구간 반복 가산 금지'], ['하강 확인', 'hold·hysteresis·low-confirm'], ['재무장', '새 사건 준비 · PMIC 하강 아님']])}
${notice('소리 사건과 전원 보상 잔량은 따로 기억합니다', '새 사건이나 FF OFF로 <b>이미 올린 잔량 R</b>을 지우지 않습니다. 무효 장부는 OFF 토글만으로 복구되지 않으며 실제 PMIC 초기화 경계를 확인해야 합니다.', 'important')}
<section class="calculator" id="ledger-example" aria-labelledby="ledger-title"><div class="section-heading"><span class="eyebrow">대화형 계산 예 · 장치 연결 없음</span><h3 id="ledger-title">세 가지 여유 중 가장 작은 값이 추가량 Δ입니다.</h3><p>모든 실행 조건과 장부가 유효하다고 가정합니다. TTL·세션·ACK·I2C 검사 전체를 시뮬레이션하는 기능은 아닙니다.</p></div>
<div class="preset-row" aria-label="계산 예 선택"><button type="button" data-preset="first">첫 상승</button><button type="button" data-preset="next">같은 사건 추가</button><button type="button" data-preset="cap">상한 부분 적용</button><button type="button" data-preset="repeat">동일 요청 반복</button><button type="button" data-preset="reclaim">High 회수 뒤</button></div>
<div class="calc-inputs">${[['P','현재 PMIC',180],['Pmax','유효 상한',214],['S','이번 누적 요구량',12],['C','이미 처리한 요구량',0],['R','성공 보상 잔량',0],['G','보상 budget',20]].map(([id,label,value])=>`<label for="calc-${id}"><span>${label} <b>${id}</b></span><input id="calc-${id}" type="number" min="0" max="255" step="1" value="${value}" inputmode="numeric"></label>`).join('')}</div>
<div id="calc-error" class="calc-error" role="status" hidden></div>
<div class="budget-grid" id="calc-budget"><div><span>미처리 요청</span><strong id="out-request">12</strong><code>S − C</code></div><div><span>보상 잔량 여유</span><strong id="out-budget">12</strong><code>min(S,G) − R</code></div><div><span>현재 레벨 여유</span><strong id="out-voltage">34</strong><code>Pmax − P</code></div></div>
<div class="calc-result" id="calc-result" aria-live="polite" aria-atomic="true"><div><span>추가량 Δ</span><strong id="out-delta">12</strong><small>PMIC step</small></div><p id="out-explanation"></p></div>
<div class="two-up result-branches"><div><h4>실제 쓰기가 성공하면</h4><p id="out-success"></p></div><div><h4>통신 실패 / 쓰기 결과 불명</h4><p>성공값이나 R을 갱신하지 않습니다. 실패 원인을 처리하고 장부 유효성과 초기화 복귀를 확인합니다.</p></div></div>
</section>
<section class="section"><h3>모드3 선택과 실제 보정 가능 상태는 다릅니다.</h3><div class="gate-grid">${[['운용','Normal · 연결 · 10V 준비 · Live'],['자극','음량>0 · CIS/NofM · F1–9'],['설정','현재 token의 CFX ACK · 같은 session'],['표본','일관된 snapshot · 요청/heartbeat TTL'],['제어','강제 고정/무한 해제 · 유효 장부'],['하드웨어','독립 시계 · I2C 가용 · 성공값 cache']].map(([title,text])=>`<article><span>${title}</span><p>${text}</p></article>`).join('')}</div></section>
<div class="figure-grid">${figure(3)}${figure(4)}</div>
${detail(chapters[6], '상세 분석 · 6장: 입력 보정·상태기계·장부·High 중재')}`;

const protocol = `
<div class="lead"><span class="tag proposal">Sound1 프로파일1.0 · FF 예약</span><h2>헤더 하나를 치환하는 이식이 아니라,<br>기존 명령 안에 새 도메인을 추가합니다.</h2><p>Sound1의 0x8F 범용 디버그와 기존 옵션1–3을 유지합니다. 원격 develop에서 제거된4번 fake sleep은 복원하지 않습니다. Sullivan의 0x59 전체 의미를 덮어씌우면 기존 앱의 명령이 다른 동작을 시킬 수 있습니다.</p></div>
<div class="protocol-banner"><div><small>Sullivan</small><code>59 <b>20</b> …</code></div><span aria-hidden="true">→</span><div><small>Sound1 권장</small><code>8F <b>20</b> …</code></div></div>
${compare(['계약', '보존 또는 제안'], [['기존 명령 보존', '<code>59 01/02</code> 묵음 · <code>8F 01..04</code> 기존 디버그 · <code>8C</code> Gain'], ['주의할 기존 옵션', '<code>8F 02</code>는 OTA 상태 강제/백텔 우회. 무한 coin으로 재해석하지 않음'], ['신규 기능 도메인', '<code>8F 20 access index [value]</code>'], ['버전 조회', '<code>8F FF 00 00</code> · 독립 Sound1 프로파일 제안'], ['응답 책임', 'handler는 응답 버퍼/결과 반환 → 호출자 한 곳에서 TX/clear'], ['에러 / 요청 순서', '<code>F0 8F …</code>에는 index가 없으므로 요청 하나씩 종결 후 다음 요청']], '호환 경계')}
<section class="section"><div class="section-heading"><span class="eyebrow">바이트 구조</span><h3>논리 요청 4/5바이트와 SPI 21바이트를 구분합니다.</h3></div><div class="packet"><div><span class="byte command">8F<small>명령</small></span><span class="byte domain">20<small>도메인</small></span><span class="byte">00/01<small>읽기/쓰기</small></span><span class="byte">index<small>인덱스</small></span><span class="byte optional">value<small>쓰기만</small></span></div><p>읽기 4B · 쓰기 5B · 응답은 <code>8F 20 access index L payload…</code>로 5+L≤20B</p></div></section>
${notice('RX 끝 바이트가 길이라는 가정은 아직 금지', 'TX의 SPI [20]은 논리 길이입니다. RX에도 같은 계약이 있는지는 확인되지 않았습니다. QCC의 실제 길이 전달 또는 20바이트 패딩 계약부터 확정합니다.', 'important')}
<section class="section"><h3>인덱스 묶음으로 찾기</h3><p>1차는0–16을 구현합니다.17은 OFF/미지원 표시만 제공하며 FF 모드1/3은 거부합니다.18 이후 FF 영역과 schema/편집은 예약·미지원으로 두고 실제 기능을 광고하지 않습니다.</p><div class="index-map">${[['0–16','링크·PMIC'],['17–26','FF mode·FF1 · 후속'],['37–39','편집·schema·상태'],['40–60','FF3 scalar'],['64–111','구간·증가량·hold'],['112–118','진단 페이지'],['120–135','누적 요구표 S']].map(([index,name])=>`<div><code>${index}</code><span>${name}</span></div>`).join('')}</div><p class="caption">인덱스는 10진수입니다. 120–135는 active/staging 누적 요구표이며 runtime 잔량 R이 아닙니다. 폐기·예약 영역은 상세표에서 확인합니다.</p></section>
${figure(5)}
${detail(chapters[8], '상세 분석 · 8장: 전체 인덱스·단위·권한·가상 패킷 예시')}`;

const shared = `
${notice('1차는 링크 설정과 필요한 공유 필드만', 'LINKPARM의 링크·PMIC 설정과 주기/NOP 공유 tail을 먼저 병합합니다. 아래 FF token·staging·CFX ACK·TIMER0는 후속 참고이며 AUDIOFF 파일은 지금 생성하지 않습니다.')}
<div class="lead"><span class="tag proposal">공유 계약 + 영속성</span><h2>링크 설정부터 병합하고,<br>FF용 공유·저장은 뒤에 붙입니다.</h2><p>설정은 CM3에서 CFX로, 관측 표본은 CFX에서 CM3로 갑니다. token·session·sequence와 독립 시계가 오래된 요청과 반쪽 표본의 적용을 막습니다.</p></div>
<div class="two-up"><section class="info-card"><span class="eyebrow">CM3 → CFX · 설정</span><h3>token 무효화 → 전체 쓰기 → 게시 → ACK</h3><p>기존 공유 구조체 끝에 필드를 추가하고 양쪽 offset·크기·정렬을 실제 map/ELF로 확인합니다.</p></section><section class="info-card"><span class="eyebrow">CFX → CM3 · 관측</span><h3>odd 작성 중 → even 완료 → 복사 후 재확인</h3><p>24워드 snapshot을 일관되게 읽습니다. 파일 generation, config token, 입력 session은 서로 다른 값입니다.</p></section></div>
<span class="tag source">아래 편집/ACK 흐름은 후속 FF 참고</span>
${steps([['BEGIN', 'active를 staging에 복사'], ['개별 편집', '실행 설정은 그대로 유지'], ['VALIDATE', '표와 시간·누적 관계 검사'], ['SAVE_APPLY', '파일/되읽기 후 active·공유 게시'], ['ACK / gate', 'CFX 수신과 실행 준비 별도 확인']])}
${compare(['확인한 단계', '알 수 있는 것', '아직 알 수 없는 것'], [['개별 쓰기 ACK','staging 값 반영','전체 유효성·저장'], ['SAVE_APPLY 성공','파일 검증·CM3 active·공유 token 게시','CFX가 현재 설정을 받았는지'], ['CFX ACK','같은 token의 설정을 수신','모든 실행 gate와 장부 유효성'], ['PMIC 쓰기 성공','해당 레벨 코드 적용 성공','실제 전압 안정화·링크 개선']], '완료를 한 가지 ACK로 합치지 않기')}
${notice('단일 FAT 파일 덮어쓰기는 전원 차단 복구를 보장하지 않습니다', '<code>AUDIOFF.TXT</code>는 96바이트 바이너리이며 단일 파일을 덮어씁니다. CRC 오류 시 FF OFF로 돌아가는 최소안과, 이전 정상 설정을 복구하는 A/B·저널 설계는 별개입니다. 같은 FAT의 파일 두 개만으로 볼륨 손상까지 보호되지는 않습니다.', 'important')}
<div class="two-up"><article class="info-card"><h3>TIMER0는 후속 FF3에서</h3><p>1차에서는 추가 초기화하지 않습니다. FF3 구현 시 Normal clock 이후 별도로 초기화·검증하며 기존 TIMER3는 유지합니다.</p></article><article class="info-card"><h3>두 시간 단위 구분</h3><p><code>source_tick</code>은 1ms PCM 사건 순번, <code>source_wall</code>은 독립 하향 카운터입니다. 서로 빼거나 CM3 루프 횟수로 대체하지 않습니다.</p></article></div>
${figure(6)}
${detail(chapters[7] + '\n' + chapters[9] + '\n' + stateC, '상세 분석 · 7·9장·10.3절: 공유·시간·파일·수명 경계')}`;

const plan = `
${detail(implementation, '현재 구현 · 빌드와 실기 확인 / 프로토콜 예')}
<div class="lead"><span class="tag proposal">1차 코드·빌드 완료 / 실기 대기</span><h2>기능을 한꺼번에 켜지 않고,<br>실패 원인이 보이는 단위로 진행합니다.</h2><p>첫 작업은 오류 반환, PMIC 성공값 관리, 응답 송신 책임과 실제 길이 계약입니다. 1차는 링크·PMIC와 그 설정/검증으로 완료하며 FF를 구현하지 않습니다.</p></div>
<ol class="roadmap">${[['0','기준 고정','현재 자료·원격 CM3 구조·시험 hook을 함께 보존'],['1','오류·공통 적용','I2C 실패 전파·성공값·single TX'],['2','PMIC·정책','설정최저112·Normal140·Mapping172·상한214·coin1'],['3','백텔·주기','PCM·안정 NOP·100ms 단위·Mapping IDLE/Standby'],['4','프로토콜·저장·확장','8F20 링크/PMIC·LINKPARM·FF 미지원·공유 tail'],['5','1차 통합 완료','코드 통합·SDK/호스트 검사 완료, 실제 장치 회귀는 미실행']].map(([n,title,text])=>`<li><span>${n}</span><div><h3>${title}</h3><p>${text}</p></div></li>`).join('')}</ol>
${notice('FF는 이 단계들의 완료 조건이 아닙니다', '후속에 FF1 또는 FF3를 연결할 PMIC 공통 적용 경계·예약 번호·공유 ABI 확장 규칙만 준비합니다. 감지·장부·TIMER0·AUDIOFF를 지금 구현할 필요는 없습니다.')}
<section class="section"><h3>코드를 옮기기 전에 함께 바로잡을 결함</h3>${compare(['문제', '이식 시 보완'], [['고정 해제 0을 validator가 거부','<code>8F 20 01 06 00</code>의 정상 해제 시험'], ['I2C 실패를 정상 표본처럼 분류','성공/표본/실패를 별도 반환'], ['실패한 force 쓰기로 cache 갱신','성공 확인 뒤에만 갱신'], ['donor handler의 자체 TX','target 호출자 한 곳에서 송신'], ['LINKPARM 기본 writer의 반환 누락','seek·write 길이·sync·close 모두 검사'], ['설정 손상 시 복구 의미 불명','FF OFF fallback 또는 별도 영속성 설계']], '확인한 소스 결함과 대응')}
</section>
<div class="verification-levels"><article><span>01 · 호스트</span><h3>순수 판단·프로토콜</h3><p>경계값, 중복 요청, 장부 수식, 오류 응답, 파일 실패와 응답 유실</p></article><article><span>02 · 실제 타깃</span><h3>배치·시간·전달</h3><p>CFX/CM3 ABI, TIMER0, PCM 24슬롯, QCC/SPI 길이, 모드 회귀</p></article><article><span>03 · 물리 측정</span><h3>전압·파형·링크</h3><p>보드 허용 범위, 안정화, 입력 보정, 소리 상승/지속/하강과 FF 효과</p></article></div>
${notice('아직 결정·측정할 항목', '운영 설정 최저2.8V, Normal 기본3.5V, Mapping 기본4.3V는 사용자 지정으로 확정했습니다. 실제 보드 동작, F10–12 fallback과 저장 복구는 검증하며, raw 보정표는 후속 FF 때 확인합니다.')}
${figure(9)}
${detail(chapters[11] + '\n' + chapters[12] + '\n' + chapters[13], '상세 분석 · 11–13장: 파일별 책임·회귀 시험·남은 결정')}`;

const evidence = `
<div class="lead"><span class="tag current">근거와 읽기 안내</span><h2>현재 코드에서 확인한 사실과<br>이식 후 검증할 항목을 구분했습니다.</h2><p>이 HTML은 원본 Markdown의 14개 챕터를 기능별로 재배치한 읽기 문서입니다. 각 탭의 ‘상세 분석’에서 조건·예외·파일 근거를 모두 확인할 수 있습니다.</p></div>
${compare(['기준', '내용'], [['대상','<code>sound1-fw-e8300-backup</code> · 현재 미커밋 변경 포함'], ['Sound1 HEAD','<code>57d71179ebe7d3ea537918ab7385734327e9673c</code>'], ['원본','<code>sullivan-1-5-fw-hw-test-link-pwr-bt-period</code> · 관련 미커밋 변경 포함'], ['Sullivan HEAD','<code>e73fa77b66dd5b5af51fce29524ff791df4070a8</code>'], ['분석 기준일','2026-10-19 · 파일 근거 52개'], ['제품 검증 수준','소스 분석. 펌웨어 구현·타깃 빌드·실기 시험 미실행']], '분석 입력')}
<section class="section"><h3>도표와 표시의 의미</h3><div class="legend"><span class="tag current">현재 코드</span><span>Sound1에서 확인한 동작</span><span class="tag source">Sullivan 동작</span><span>옮길 원본의 동작과 시험 설정</span><span class="tag proposal">제안 / 미구현</span><span>권장 설계와 앞으로의 검증</span></div><p>수치 예시와 상태도는 코드/설계 설명입니다. 실제 장치 로그나 실측 화면으로 해석하지 않습니다. 모든 계산 도구는 이 문서 안에서만 동작하며 장치와 연결하지 않습니다.</p></section>
<section class="section"><h3>탐색과 인쇄</h3><ul class="read-list"><li>탭에서 위/아래 방향키(좁은 화면은 좌/우), Home/End로 이동할 수 있습니다.</li><li>문서 검색은 숨겨진 상세 분석까지 찾고 해당 탭을 엽니다.</li><li>‘전체 보기’는 모든 탭과 상세를 펼칩니다. 브라우저 찾기와 연속 읽기에 사용합니다.</li><li>‘전체 인쇄’는 모든 기능과 상세를 인쇄합니다. 계산 예시는 현재 선택한 값으로 출력됩니다.</li><li>도해는 글자 크기를 유지하며 창 너비에 맞춰 재배치됩니다. 조건 → 목적지 링크로 분기·반복·복귀 상태를 따라갈 수 있습니다.</li></ul></section>
<section class="section design-notes"><h3>UI/UX 조사에서 적용한 원칙</h3><p>네이비 탐색 영역과 밝은 본문을 나누고, 파랑은 선택·이동, 청록은 관계 설명, 황갈색은 주의할 조건에 사용했습니다. 의미는 색과 함께 문구로 표시합니다.</p><ul><li><a href="https://www.w3.org/WAI/ARIA/apg/patterns/tabs/" target="_blank" rel="noopener noreferrer">W3C APG · Tabs</a>: 선택 상태와 탭-패널 관계를 표시하고, 방향키·Home/End 탐색을 적용했습니다.</li><li><a href="https://www.w3.org/WAI/WCAG22/Understanding/reflow.html" target="_blank" rel="noopener noreferrer">WCAG 2.2 · Reflow</a>: 좁은 화면에서 본문을 한 열로 재배치하고 넓은 표만 내부에서 스크롤합니다.</li><li><a href="https://www.w3.org/WAI/WCAG22/Understanding/contrast-minimum.html" target="_blank" rel="noopener noreferrer">WCAG 2.2 · Contrast</a>: 본문과 표지·탭의 주요 색 조합을 검사하고, 선택·주의를 색만으로 전달하지 않습니다.</li><li><a href="https://www.w3.org/WAI/WCAG22/Understanding/target-size-minimum.html" target="_blank" rel="noopener noreferrer">WCAG 2.2 · Target Size</a>: 주요 버튼과 탭에 충분한 높이와 간격을 확보했습니다.</li></ul><p class="caption">일부 자동/수동 검사를 수행한 문서이며 전체 접근성 인증이나 실제 사용자 시험을 뜻하지 않습니다.</p><p class="caption">본문·탭의 기본 글꼴은 설치된 KoPubWorld바탕체입니다. 다른 PC에서는 해당 서체를 설치하면 같은 글꼴로 읽을 수 있습니다. 도해의 상자·라벨도 본문과 같은 글꼴입니다. 코드만 구문을 구분하기 위한 별도 글꼴을 사용합니다.</p></section>
<section class="section"><h3>원본과 검증 기록</h3><div class="reference-links"><a href="분석과%20계획.md">원본 Markdown</a><a href="작업.md">작업 기록</a><a href="분석%20기준.json">파일 해시 기준</a><a href="HTML%20검증.json">HTML 검증 결과</a></div><p class="caption">내용 원본 SHA-256: <code>${sourceHash}</code></p></section>
${detail(chapters[14], '상세 분석 · 14장: 대상·원본의 근거 파일')}`;

const panels = [
 ['overview','개요','전체 관계와 권장안',overview], ['link','링크·백텔','PCM · 주기 · 상태',link], ['pmic','PMIC·전원','레벨 · 정책 · 중재',pmic], ['ff1','FF1','후속 확장 · 1차 미구현',ff1], ['ff3','FF3','후속 확장 · 계산 예시',ff3], ['protocol','프로토콜','헤더 · 인덱스 · 응답',protocol], ['shared','공유·저장','시간 · ACK · 영속성',shared], ['plan','이식·검증','단계 · 파일 · 시험',plan], ['evidence','근거·읽기 안내','기준 · 상세 · 디자인',evidence]
];

const css = (await fs.readFile(path.join(root, 'HTML 스타일.css'), 'utf8')) + '\n' + (await fs.readFile(path.join(root, 'HTML 반응형 도해.css'), 'utf8'));
const js = (await fs.readFile(path.join(root, 'HTML 동작.js'), 'utf8')).replace('/* RESPONSIVE_DIAGRAM_RUNTIME */', await fs.readFile(path.join(root, 'HTML 도해 동작.js'), 'utf8'));
const html = `<!doctype html>
<html lang="ko"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><meta name="color-scheme" content="light"><meta name="description" content="Sullivan의 링크 백텔 체크·PMIC·FF를 현재 Sound1 backup으로 이식하는 분석과 계획. 기능별 탭, 상태도, 흐름도, 계산 예시."><title>Sound1 · 링크·PMIC·FF 마이그레이션 분석</title><style>${css}</style></head>
<body><a class="skip-link" href="#main">본문으로 건너뛰기</a>
<aside class="sidebar"><a href="#overview" class="brand" aria-label="Sound1 마이그레이션 개요"><span class="brand-mark" aria-hidden="true">S1</span><span>Sound1<small>FIRMWARE ENGINEERING</small></span></a><div class="side-title"><span class="eyebrow">Sullivan → Sound1</span><strong>마이그레이션<br>분석과 계획</strong></div><div class="side-divider"></div><span class="nav-label" id="nav-label">기능별 탐색</span><div class="tabs" role="tablist" aria-labelledby="nav-label" aria-orientation="vertical">${panels.map(([id,label,sub],i)=>`<button type="button" role="tab" id="tab-${id}" aria-controls="panel-${id}" aria-selected="${i===0}" tabindex="${i===0?'0':'-1'}" data-tab="${id}"><span class="tab-number" aria-hidden="true">${String(i+1).padStart(2,'0')}</span><span class="tab-text"><b>${label}</b><small>${sub}</small></span><span class="tab-arrow" aria-hidden="true">›</span></button>`).join('')}</div><div class="side-footer"><span class="side-dot"></span>1차 코드 완료 · 실기 대기<small>기준일 2026.10.19<br>대상 sound1-fw-e8300-backup</small></div></aside>
<div class="workspace"><header class="topbar"><div class="breadcrumbs"><span>기술 검토 문서</span><b id="current-tab">개요</b></div><div class="toolbar"><button type="button" id="search-open" aria-haspopup="dialog">검색 <kbd>/</kbd></button><button type="button" id="all-toggle" aria-pressed="false">전체 보기</button><button type="button" id="print-all">전체 인쇄</button></div></header><main id="main" tabindex="-1"><div class="document-heading"><div><span class="eyebrow">MIGRATION STUDY / 2026.10.19</span><h1>링크·PMIC·FF 이식 분석</h1></div><span class="document-state">1차 링크·PMIC / FF 후속</span></div>${notice('현재 구현 상태', '브랜치 <code>claude_feature_sound1-link-pmic</code>에서 FF를 제외한1차 코드를 구현했습니다. SDK 빌드·호스트·공유 ABI 검사는 통과했고 실제 장치 시험은 미실행입니다. <a href="빌드와%20실기%20확인.md">현재 빌드·실기 확인 방법</a>을 먼저 읽어 주세요. 아래 원본14장은 구현 전 분석과 후속 FF 설계도 포함합니다.')}${panels.map(([id,label,sub,content],i)=>`<section class="tab-panel" role="tabpanel" tabindex="0" id="panel-${id}" aria-labelledby="tab-${id}" data-label="${label}" ${i?'hidden':''}><div class="print-panel-title">${label} · ${sub}</div>${content}<div class="panel-end"><span>${label}</span>${i<panels.length-1?`<button type="button" class="next-panel" data-go="${panels[i+1][0]}">다음 · ${panels[i+1][1]} <span aria-hidden="true">→</span></button>`:'<button type="button" data-go="overview">개요로 돌아가기 ↑</button>'}</div></section>`).join('')}<footer class="page-footer"><span>Sound1 · engineering reading edition</span><span>KoPubWorld바탕체 · 원본 분석 14개 챕터</span></footer></main></div>
<dialog id="search-dialog" aria-labelledby="search-title"><div class="dialog-head"><h2 id="search-title">문서 전체 검색</h2><button type="button" class="dialog-close" data-close="search-dialog" aria-label="검색 닫기">닫기 <kbd>Esc</kbd></button></div><label class="search-label" for="search-input">기능·변수·명령·조건 찾기</label><input type="search" id="search-input" placeholder="예: Duration, FF OFF, index, TIMER0" autocomplete="off"><p id="search-count" role="status">검색어를 입력하면 상세 분석까지 찾습니다.</p><div id="search-results"></div></dialog>

<div id="announce" class="sr-only" aria-live="polite"></div><noscript><style>.tab-panel[hidden]{display:block}.analysis-detail>:not(summary){display:block}.sidebar,.toolbar,.figure-open{display:none}.workspace{margin-left:0}.topbar{position:static}.print-panel-title{display:block}</style><p class="notice">JavaScript가 꺼져 있어 모든 탭을 연속 표시합니다. 상세 분석은 각 제목을 눌러 펼칠 수 있습니다.</p></noscript><script type="application/json" id="diagram-manifest">${JSON.stringify(diagramManifest).replaceAll("<", "\\u003c")}</script><script>${js}</script></body></html>`;
await fs.writeFile(path.join(root, '마이그레이션 분석.html'), html, 'utf8');
console.log(JSON.stringify({output:'마이그레이션 분석.html',bytes:Buffer.byteLength(html),panels:panels.length,diagrams:diagrams.length,sourceSha256:sourceHash,evidenceFiles:baseline.files.length}));
