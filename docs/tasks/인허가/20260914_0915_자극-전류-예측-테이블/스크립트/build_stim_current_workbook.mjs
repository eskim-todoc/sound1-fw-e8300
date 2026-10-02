import fs from "node:fs/promises";
import path from "node:path";
import { FileBlob, SpreadsheetFile, Workbook } from "@oai/artifact-tool";

const ROOT = process.cwd();
const THREAD_ID = "00571323-df7f-4ffc-9b92-8f22ee5897b2";
const OUT_DIR = path.join(ROOT, "outputs", THREAD_ID);
const QA_DIR = path.join(ROOT, "tmp", "stim-current-workbook", "previews");
const OUT_FILE = path.join(OUT_DIR, "Sound1 자극 파라미터 및 예상 출력 전류 계산표.xlsx");

const C = {
  navy: "#16324F",
  blue: "#1F4E78",
  cyan: "#D9EAF7",
  pale: "#EEF5FA",
  input: "#FFF2CC",
  green: "#E2F0D9",
  orange: "#FCE4D6",
  red: "#F4CCCC",
  gray: "#E7E6E6",
  darkGray: "#666666",
  white: "#FFFFFF",
  text: "#1F1F1F",
};
const FONT = "맑은 고딕";

function colName(n) {
  let s = "";
  while (n > 0) {
    n -= 1;
    s = String.fromCharCode(65 + (n % 26)) + s;
    n = Math.floor(n / 26);
  }
  return s;
}

function title(sheet, range, text) {
  sheet.mergeCells(range);
  const r = sheet.getRange(range);
  r.values = [[text]];
  r.format = {
    fill: C.navy,
    font: { name: FONT, color: C.white, bold: true, size: 17 },
    horizontalAlignment: "left",
    verticalAlignment: "center",
  };
  r.format.rowHeight = 34;
}

function section(sheet, range, text) {
  sheet.mergeCells(range);
  const r = sheet.getRange(range);
  r.values = [[text]];
  r.format = {
    fill: C.blue,
    font: { name: FONT, color: C.white, bold: true, size: 11 },
    verticalAlignment: "center",
  };
  r.format.rowHeight = 24;
}

function header(sheet, range) {
  const r = sheet.getRange(range);
  r.format = {
    fill: C.blue,
    font: { name: FONT, color: C.white, bold: true, size: 9 },
    horizontalAlignment: "center",
    verticalAlignment: "center",
    wrapText: true,
    borders: {
      top: { style: "thin", color: C.white },
      bottom: { style: "thin", color: C.white },
      left: { style: "thin", color: C.white },
      right: { style: "thin", color: C.white },
    },
  };
  r.format.rowHeight = 31;
}

function body(sheet, range, align = "left") {
  const r = sheet.getRange(range);
  r.format = {
    font: { name: FONT, color: C.text, size: 9 },
    horizontalAlignment: align,
    verticalAlignment: "center",
    wrapText: true,
    borders: {
      top: { style: "thin", color: "#D9D9D9" },
      bottom: { style: "thin", color: "#D9D9D9" },
      left: { style: "thin", color: "#D9D9D9" },
      right: { style: "thin", color: "#D9D9D9" },
    },
  };
}

function setWidths(sheet, widths) {
  widths.forEach((w, i) => {
    sheet.getRange(`${colName(i + 1)}:${colName(i + 1)}`).format.columnWidth = w;
  });
}

function sat24(v) {
  return Math.max(-8388608, Math.min(8388607, v));
}

function parseNumbers(block) {
  return [...block.matchAll(/-?\d+/g)].map((m) => Number(m[0]));
}

async function loadFftAssets() {
  const sourcePath = path.join(ROOT, "src", "2__cm3", "Gen1_5", "FS", "ci_fft.c");
  const text = await fs.readFile(sourcePath, "utf8");
  const h = text.match(/hanning_window_coeff\[Half_FFT_Size\]\s*=\s*\{([\s\S]*?)\};/);
  const p = text.match(/int ci_fft_init_pass_bin_ch_32[\s\S]*?char pass_bin\[Half_FFT_Size\]\s*=\s*\{([\s\S]*?)\};/);
  if (!h || !p) throw new Error("ci_fft.c에서 Hann 계수 또는 32대역 pass-bin을 찾지 못했습니다.");
  const half = parseNumbers(h[1]);
  const passBins = parseNumbers(p[1]);
  if (half.length !== 256 || passBins.length !== 256) {
    throw new Error(`FFT 자산 길이 오류: Hann=${half.length}, pass-bin=${passBins.length}`);
  }
  return { hann: [...half, ...half.slice().reverse()], passBins };
}

function hannCorrection(hann, deltaBin, baseBin = 122) {
  let re = 0;
  let im = 0;
  const nMax = 512;
  for (let n = 0; n < nMax; n += 1) {
    const w = hann[n] / 8388608;
    const x = Math.sin((2 * Math.PI * (baseBin + deltaBin) * n) / nMax);
    re += w * x * Math.cos((2 * Math.PI * baseBin * n) / nMax);
    im -= w * x * Math.sin((2 * Math.PI * baseBin * n) / nMax);
  }
  return Math.sqrt(re * re + im * im) / 128;
}

const AGC = {
  attenYBiasQ8_16: [-1661530, -2008762, -2355995, -2703227, -3050459, -3397692, -3744924, -4092157, -4439389, -4786621],
  ampYBiasQ8_16: [0, 108138, 216277, 324416, 432555, 540694, 648833, 756971, 865110, 973249],
  noiseYBiasQ12_12: [481268, 488027, 494785, 501544, 508303, 515061, 521820, 528579, 535337, 542096],
  attenSlopeQ8_16: [47999, 43193, 38387, 33581, 28774, 23968, 19162, 14356, 9550, 4744],
  logGainQ9_15: [32768, 46955, 68657, 100391, 146792, 214639, 313846, 458905, 671011, 981153],
};
const STIM_VOL_Q2_22 = [2936012, 3355443, 3774873, 4194304];
const STIM_DACS = [
  { code: 0, name: "A", slope: 1.57, slopeQ5_12: 6430, sat: 400, recip: 20871 },
  { code: 1, name: "B", slope: 3.15, slopeQ5_12: 12902, sat: 803, recip: 10402 },
  { code: 2, name: "C", slope: 4.72, slopeQ5_12: 19333, sat: 1203, recip: 6942 },
  { code: 3, name: "D", slope: 6.30, slopeQ5_12: 25804, sat: 1606, recip: 5201 },
];
const OFFSET_DACS = [
  { code: 0, name: "A", slope: 2.27, slopeQ5_12: 9297, sat: 295, recip: 14435 },
  { code: 1, name: "B", slope: 4.56, slopeQ5_12: 18677, sat: 592, recip: 7185 },
];

function mapDefaults(program, electrode) {
  const k = electrode - 1;
  const isMap2 = program === 2;
  const strategy = program === 3 ? 2 : 1;
  const firstPhase = program === 3 ? 1 : 0;
  const pulse = program === 2 ? 13 : program === 3 ? 14 : program === 4 ? 255 : 50;
  const bands = program === 2 ? 24 : 32;
  let t;
  let c;
  if (isMap2) {
    if (k === 15) t = c = 1500;
    else if (k <= 23) t = c = 500 + Math.floor((1000 * k) / 23);
    else t = c = 0;
  } else {
    t = k === 15 ? 1500 : 500;
    c = 1500;
  }
  const pcbMap = [31, 12, 11, 9, 7, 5, 3, 1, 0, 2, 4, 6, 8, 10, 13, 15, 17, 18, 20, 22, 24, 26, 28, 30, 14, 29, 27, 25, 23, 21, 19, 16];
  return {
    program, electrode, strategy, firstPhase, pulse, bands,
    t, c, xMin: 8, xMax: 21892, assignedElectrode: electrode,
    cisOrder: electrode, pcbChannel: pcbMap[k],
  };
}

function chooseDac(minT, maxC) {
  const dynamic = maxC - minT;
  let stimStart = 0;
  if (dynamic <= 400) stimStart = 0;
  else if (dynamic <= 803) stimStart = 1;
  else if (dynamic < 1203) stimStart = 2;
  else if (dynamic < 1606) stimStart = 3;
  else return { error: true };
  for (let s = stimStart; s < STIM_DACS.length; s += 1) {
    for (const o of OFFSET_DACS) {
      if (STIM_DACS[s].sat + o.sat >= maxC) return { stim: STIM_DACS[s], offset: o, error: false };
    }
  }
  return { error: true };
}

function programSummary(program, mapRows) {
  const active = mapRows.filter((r) => r.program === program && r.electrode <= r.bands);
  const minT = Math.min(...active.filter((r) => r.t > -1).map((r) => r.t));
  const maxC = Math.max(...active.map((r) => r.c));
  const cfg = chooseDac(minT, maxC);
  if (cfg.error) throw new Error(`프로그램 ${program} DAC 구성을 만들 수 없습니다.`);
  const requestedOffset = Math.min(minT, cfg.offset.sat);
  const offsetLevel = Math.floor((requestedOffset * cfg.offset.recip) / 32768);
  const actualOffset = Math.floor((cfg.offset.slopeQ5_12 * offsetLevel) / 4096);
  const first = active[0];
  let frames = Math.floor((2 * first.pulse + 14) / 41) + 1;
  let channelsPerMs = Math.floor(24 / frames);
  if (first.strategy === 2 && first.bands >= 16) {
    frames = 3;
    channelsPerMs = 8;
  }
  return {
    program, strategy: first.strategy, firstPhase: first.firstPhase,
    pulse: first.pulse, bands: first.bands, minT, maxC, dynamic: maxC - minT,
    stim: cfg.stim, offset: cfg.offset, requestedOffset, offsetLevel, actualOffset,
    frames, channelsPerMs,
  };
}

function tC255(row, summary) {
  if (row.electrode > row.bands) return { t255: 0, c255: 0 };
  return {
    t255: Math.floor(((row.t - summary.requestedOffset) * summary.stim.recip) / 32768),
    c255: Math.floor(((row.c - summary.requestedOffset) * summary.stim.recip) / 32768),
  };
}

function addCommonSheetStyle(sheet) {
  sheet.showGridLines = false;
}

async function build() {
  await fs.mkdir(OUT_DIR, { recursive: true });
  await fs.mkdir(QA_DIR, { recursive: true });
  const { hann, passBins } = await loadFftAssets();
  const corrections = Array.from({ length: 51 }, (_, i) => [i / 100, hannCorrection(hann, i / 100)]);
  const maps = [];
  for (let p = 1; p <= 4; p += 1) for (let e = 1; e <= 32; e += 1) maps.push(mapDefaults(p, e));
  const prog = Array.from({ length: 4 }, (_, i) => programSummary(i + 1, maps));
  maps.forEach((r) => Object.assign(r, tC255(r, prog[r.program - 1])));

  const wb = Workbook.create();
  const summary = wb.worksheets.add("요약");
  const id6 = wb.worksheets.add("필수성능 ID-6");
  const cm3 = wb.worksheets.add("기본 맵 및 CM3");
  const lut = wb.worksheets.add("PCM-전류 LUT");
  const constants = wb.worksheets.add("상수 및 근거");

  // -------------------------- 상수 및 근거
  title(constants, "A1:Q1", "Sound1 계산 상수 및 코드 근거");
  constants.getRange("A2:Q2").merge();
  constants.getRange("A2").values = [["노란 셀은 사용자가 바꿀 수 있는 입력이며, 이 시트의 값은 현재 소스와 HEAR 레퍼런스에서 옮긴 고정 상수입니다."]];
  constants.getRange("A2:Q2").format = { fill: C.pale, font: { name: FONT, size: 9, italic: true, color: C.darkGray }, wrapText: true };

  section(constants, "A3:G3", "오디오 볼륨 및 AGC 상수");
  constants.getRange("A4:G4").values = [["오디오 볼륨", "감쇠 y절편 [dB]", "증폭 y절편 [dB]", "잡음 y절편 [dB]", "감쇠 기울기", "xMin 게인 [Q9.15]", "xMin 게인 [배]"]];
  header(constants, "A4:G4");
  const agcRows = Array.from({ length: 10 }, (_, i) => [
    i + 1,
    AGC.attenYBiasQ8_16[i] / 65536,
    AGC.ampYBiasQ8_16[i] / 65536,
    AGC.noiseYBiasQ12_12[i] / 4096,
    AGC.attenSlopeQ8_16[i] / 65536,
    AGC.logGainQ9_15[i],
    AGC.logGainQ9_15[i] / 32768,
  ]);
  constants.getRange("A5:G14").values = agcRows;
  body(constants, "A5:G14", "center");
  constants.getRange("B5:E14").format.numberFormat = "0.0000";
  constants.getRange("G5:G14").format.numberFormat = "0.0000";

  section(constants, "I3:J3", "자극 볼륨");
  constants.getRange("I4:J4").values = [["자극 볼륨", "비율 [Q2.22]"]];
  header(constants, "I4:J4");
  constants.getRange("I5:J8").values = STIM_VOL_Q2_22.map((v, i) => [i + 1, v]);
  body(constants, "I5:J8", "center");

  section(constants, "I10:N10", "자극 DAC (ISD 1.1.2)");
  constants.getRange("I11:N11").values = [["코드", "이름", "기울기 [uA/level]", "기울기 [Q5.12]", "포화 [uA]", "역수 [Q1.15]"]];
  header(constants, "I11:N11");
  constants.getRange("I12:N15").values = STIM_DACS.map((d) => [d.code, d.name, d.slope, d.slopeQ5_12, d.sat, d.recip]);
  body(constants, "I12:N15", "center");

  section(constants, "I17:N17", "Offset DAC (ISD 1.1.2)");
  constants.getRange("I18:N18").values = [["코드", "이름", "기울기 [uA/level]", "기울기 [Q5.12]", "포화 [uA]", "역수 [Q1.15]"]];
  header(constants, "I18:N18");
  constants.getRange("I19:N20").values = OFFSET_DACS.map((d) => [d.code, d.name, d.slope, d.slopeQ5_12, d.sat, d.recip]);
  body(constants, "I19:N20", "center");

  section(constants, "A17:G17", "일반 상수");
  const general = [
    ["ADC 상한", 262143, "maxAudioInput 산술 상한"],
    ["ADC 정규화 기준", 120, "dB SPL"],
    ["완제품 기구 감쇠", 8.293, "dB"],
    ["Noise gate", -117.4972, "dB"],
    ["Rotation point", -94.7472, "dB"],
    ["로그 매핑 p", 0.15, "무차원"],
    ["xMin", 8, "기본 맵"],
    ["xMax", 21892, "기본 맵"],
    ["FFT 크기", 512, "sample"],
    ["표본화 주파수", 16000, "Hz"],
    ["FFT bin 간격", 31.25, "Hz"],
    ["기본 mute mode", 2, "2=비활성"],
    ["기본 T offset", 2, "PCM level"],
  ];
  constants.getRange("A18:C30").values = general;
  body(constants, "A18:C30");
  constants.getRange("A18:A30").format.font = { name: FONT, bold: true, size: 9 };

  section(constants, "I23:J23", "512점 Hann 주파수 오프셋 보정");
  constants.getRange("I24:J24").values = [["bin 오프셋 [0~0.5]", "정규화 크기"]];
  header(constants, "I24:J24");
  constants.getRange("I25:J75").values = corrections;
  body(constants, "I25:J75", "center");
  constants.getRange("I25:J75").format.numberFormat = "0.000000";

  section(constants, "O3:Q3", "32대역 FFT pass-bin");
  constants.getRange("O4:Q4").values = [["FFT bin", "주파수 [Hz]", "대역 번호 (1-based)"]];
  header(constants, "O4:Q4");
  constants.getRange("O5:Q260").values = passBins.map((b, i) => [i, i * 31.25, b < 0 ? 0 : b + 1]);
  body(constants, "O5:Q260", "center");
  constants.getRange("P5:P260").format.numberFormat = "0.00";

  section(constants, "A33:G33", "산출 근거");
  const sourceRows = [
    ["기본 맵", "src/2__cm3/Gen1_5/FS/ci_map.c:531-769", "프로그램 1~4의 전략, 펄스폭, 대역, T/C, xMin/xMax"],
    ["CM3 자극 계산", "src/2__cm3/Cortex-M3-src/signalProcessing/stimulationParaCal.c:32-626", "프레임 수, DAC 선택, offset, T/C 255"],
    ["ISD 1.1.2 상수", "src/2__cm3/99_includeBoard/isd_ver1_1_2.h:65-125", "DAC 기울기, 포화값, Q-format 역수"],
    ["CFX AGC", "src/1__cfx/signalProcessing/agc.c:92-335", "입력 최대값, 정상상태 이득, AGC 출력"],
    ["주파수 분석", "src/1__cfx/signalProcessing/FrequencyAnalysis.c:45-106", "vMag 대역 최대 및 >>4 스케일"],
    ["로그 매핑", "src/1__cfx/signalProcessing/nonlinearMapping.c:57-675", "볼륨, x^p, A/B, PCM Amplitude"],
    ["PCM 패킷", "src/1__cfx/signalProcessing/stimulationStrategy.c", "Amplitude 8비트와 전극 채널 결합"],
    ["Hann/pass-bin", "src/2__cm3/Gen1_5/FS/ci_fft.c:540-617", "32대역 pass-bin과 512점 Hann 계수"],
    ["HEAR", "HEAR Configurable Accelerator Reference Manual 4.14, 4.38", "Win_DFT_R_Forward BFP, vMag 2.22 및 LUT sqrt 근사"],
    ["ADC 모델", "docs/참고/인허가/사운드1/Sound1 입력 음압 대비 ADC 출력 산출 근거.md", "완제품 감쇠 8.293 dB와 dB SPL-ADC 식"],
  ];
  constants.getRange(`A34:C${33 + sourceRows.length}`).values = sourceRows;
  body(constants, `A34:C${33 + sourceRows.length}`);
  constants.getRange(`A34:A${33 + sourceRows.length}`).format.font = { name: FONT, bold: true, size: 9 };
  setWidths(constants, [20, 25, 36, 17, 15, 16, 14, 3, 18, 17, 12, 17, 14, 14, 11, 15, 19]);
  constants.freezePanes.freezeRows(4);

  // -------------------------- CM3 맵 및 계산
  title(cm3, "A1:T1", "CM3 기본 맵 및 stimulationParaCal 결과");
  cm3.getRange("A2:T2").merge();
  cm3.getRange("A2").values = [["현재 빌드(ISD_Ver_is_112)의 기본 맵을 기준으로 DAC 범위와 T/C 8비트 레벨을 정수 절삭까지 재현했습니다. 프로그램 3의 N-of-M 강제 설정도 반영했습니다."]];
  cm3.getRange("A2:T2").format = { fill: C.pale, font: { name: FONT, size: 9, italic: true }, wrapText: true };
  section(cm3, "A3:T3", "프로그램별 CM3 산출");
  const progHeaders = ["프로그램", "전략", "첫 위상", "펄스폭 [us]", "대역 수", "최소 T [uA]", "최대 C [uA]", "동적범위 [uA]", "Stim DAC", "이름", "Stim slope Q5.12", "Offset DAC", "이름", "Offset slope Q5.12", "요청 offset [uA]", "offset reg", "실현 offset [uA]", "frame/ch", "ch/ms", "상태"];
  cm3.getRange("A4:T4").values = [progHeaders];
  header(cm3, "A4:T4");
  cm3.getRange("A5:T8").values = prog.map((p) => [
    p.program, p.strategy === 1 ? "CIS" : "N-of-M", p.firstPhase === 0 ? "음(-)" : "양(+)", p.pulse, p.bands,
    p.minT, p.maxC, p.dynamic, p.stim.code, p.stim.name, p.stim.slopeQ5_12,
    p.offset.code, p.offset.name, p.offset.slopeQ5_12, p.requestedOffset, p.offsetLevel, p.actualOffset,
    p.frames, p.channelsPerMs, "정상",
  ]);
  body(cm3, "A5:T8", "center");
  cm3.getRange("A5:T8").format.fill = C.green;

  section(cm3, "A10:P10", "전극별 기본 맵과 T/C 255 변환");
  const mapHeaders = ["프로그램", "전극", "전략", "첫 위상", "펄스폭", "대역 수", "T [uA]", "C [uA]", "xMin", "xMax", "할당 전극", "CIS 순서", "T [0~255]", "C [0~255]", "PCB 채널", "활성"];
  cm3.getRange("A11:P11").values = [mapHeaders];
  header(cm3, "A11:P11");
  cm3.getRange("A12:P139").values = maps.map((r) => [
    r.program, r.electrode, r.strategy === 1 ? "CIS" : "N-of-M", r.firstPhase,
    r.pulse, r.bands, r.t, r.c, r.xMin, r.xMax, r.assignedElectrode, r.cisOrder,
    r.t255, r.c255, r.pcbChannel, r.electrode <= r.bands ? "Y" : "N",
  ]);
  body(cm3, "A12:P139", "center");
  cm3.getRange("G12:J139").format.fill = C.input;
  cm3.getRange("M12:O139").format.fill = C.green;
  cm3.getRange("P12:P139").conditionalFormats.add("cellIs", { operator: "equalTo", formula: '="N"', format: { fill: C.gray, font: { color: C.darkGray } } });
  setWidths(cm3, [9, 8, 10, 9, 10, 9, 10, 10, 9, 10, 10, 10, 11, 11, 10, 8, 11, 9, 9, 9]);
  cm3.freezePanes.freezeRows(11);

  // -------------------------- 요약
  title(summary, "A1:H1", "Sound1 dB SPL → PCM Amplitude → 예상 자극 전류");
  summary.getRange("A2:H2").merge();
  summary.getRange("A2").values = [["노란 입력 셀을 바꾸면 현재 펌웨어 기준 계산이 갱신됩니다. 예상 전류는 ISD 1.1.2 명목 DAC 상수에 따른 산출값이며 시료별 실측 오차는 포함하지 않습니다."]];
  summary.getRange("A2:H2").format = { fill: C.pale, font: { name: FONT, size: 9, italic: true }, wrapText: true };
  section(summary, "A4:C4", "시험 입력");
  summary.getRange("A5:C14").values = [
    ["프로그램", 1, "1~4"],
    ["전극 번호", 26, "1~32"],
    ["입력 주파수 [Hz]", 3815, "0~7,968.75"],
    ["입력 음압 [dB SPL]", 70, "Sound1 최대 110"],
    ["오디오 볼륨", 1, "1~10"],
    ["자극 볼륨", 4, "1~4"],
    ["완제품 감쇠 [dB]", 8.293, "Sound1 ADC 표"],
    ["HEAR 보정 계수", 1.0, "기본 1.0"],
    ["T 미만 mute mode", 2, "1=켜짐, 2=꺼짐"],
    ["T mute offset [level]", 2, "기본 2"],
  ];
  body(summary, "A5:C14");
  summary.getRange("A5:A14").format.font = { name: FONT, bold: true, size: 9 };
  summary.getRange("B5:B14").format = { fill: C.input, font: { name: FONT, bold: true, size: 10, color: C.text }, horizontalAlignment: "center", borders: { top: { style: "thin", color: "#C9B458" }, bottom: { style: "thin", color: "#C9B458" }, left: { style: "thin", color: "#C9B458" }, right: { style: "thin", color: "#C9B458" } } };
  summary.dataValidations.add({ range: "B5", rule: { type: "whole", operator: "between", formula1: 1, formula2: 4 } });
  summary.dataValidations.add({ range: "B6", rule: { type: "whole", operator: "between", formula1: 1, formula2: 32 } });
  summary.dataValidations.add({ range: "B7", rule: { type: "decimal", operator: "between", formula1: 0, formula2: 7968.75 } });
  summary.dataValidations.add({ range: "B8", rule: { type: "decimal", operator: "between", formula1: 0, formula2: 110 } });
  summary.dataValidations.add({ range: "B9", rule: { type: "whole", operator: "between", formula1: 1, formula2: 10 } });
  summary.dataValidations.add({ range: "B10", rule: { type: "whole", operator: "between", formula1: 1, formula2: 4 } });
  summary.dataValidations.add({ range: "B13", rule: { type: "list", formula1: '"1,2"' } });

  section(summary, "D4:H4", "계산 결과");
  const labels = [
    ["명목 maxAudioInput", "count", "dB SPL→ADC"],
    ["가장 가까운 FFT bin", "bin", "512점/16 kHz"],
    ["대상 주파수 대역", "1-based", "32대역 pass-bin"],
    ["AGC 입력", "dB", "CFX Q-format 등가"],
    ["AGC 구간", "", "잡음/증폭/감쇠"],
    ["AGC 정상상태 이득", "dB", "1초 유지 조건"],
    ["AGC 선형 이득", "배", ""],
    ["AGC 출력 peak", "count", ""],
    ["Hann/주파수 보정", "배", "bin 오프셋"],
    ["대역 대표값", "count", "g_freq_rep_values_scaled"],
    ["맵 T", "uA", "ci_map.c"],
    ["맵 C", "uA", "ci_map.c"],
    ["T 레벨", "0~255", "CM3 계산"],
    ["C 레벨", "0~255", "CM3 계산"],
    ["볼륨 반영 C 한계", "0~255", "자극 볼륨"],
    ["볼륨 반영 xMin", "count", "오디오 볼륨"],
    ["xMax", "count", "기본 맵"],
    ["입력 x^p", "Q8.16", "p=0.15"],
    ["xMin^p", "Q8.16", ""],
    ["xMax^p", "Q8.16", ""],
    ["로그계수 A", "Q16.8", ""],
    ["로그계수 B", "Q16.8", ""],
    ["클램프 전 Amplitude", "level", ""],
    ["PCM Amplitude", "0~255", "패킷 8비트"],
    ["예상 자극 전류", "uA", "명목 DAC"],
    ["BLE 표시 전류", "uA", "Amplitude=0이면 0 표시"],
    ["PCB 자극 채널", "0-based", "electrodeMap"],
    ["첫 펄스 위상", "0/1", "0=음, 1=양"],
    ["PCM 패킷", "decimal", "20bit"],
    ["펄스폭", "us", "phase width"],
    ["위상당 전하", "pC", "전류×펄스폭"],
    ["PCM frame/ch", "frame", ""],
    ["전송 ch/ms", "channel", ""],
    ["맵 활성 상태", "Y/N", "프로그램 대역 수 기준"],
  ];
  summary.getRange("D5:D38").values = labels.map((r) => [r[0]]);
  summary.getRange("F5:G38").values = labels.map((r) => [r[1], r[2]]);
  const f = [];
  f.push(["=INT('상수 및 근거'!$B$18*POWER(10,($B$8-'상수 및 근거'!$B$19-$B$11)/20))"]);
  f.push(["=ROUND($B$7/'상수 및 근거'!$B$28,0)"]);
  f.push(["=INDEX('상수 및 근거'!$Q$5:$Q$260,E6+1)"]);
  f.push(["=10*LOG10(MAX(1,E5)*32/POWER(2,47))"]);
  f.push(["=IF(E8<'상수 및 근거'!$B$21,\"잡음\",IF(E8>'상수 및 근거'!$B$22,\"감쇠\",\"증폭\"))"]);
  f.push(["=IF(E8<'상수 및 근거'!$B$21,E8+INDEX('상수 및 근거'!$D$5:$D$14,$B$9),IF(E8>'상수 및 근거'!$B$22,INDEX('상수 및 근거'!$E$5:$E$14,$B$9)*E8+INDEX('상수 및 근거'!$B$5:$B$14,$B$9)-E8,INDEX('상수 및 근거'!$C$5:$C$14,$B$9)))"]);
  f.push(["=POWER(10,E10/10)"]);
  f.push(["=INT(E5*E11)"]);
  f.push(["=INDEX('상수 및 근거'!$J$25:$J$75,ROUND(ABS($B$7/'상수 및 근거'!$B$28-E6)*100,0)+1)*$B$12"]);
  f.push(["=INT(E12*E13)"]);
  f.push(["=INDEX('기본 맵 및 CM3'!$G$12:$G$139,($B$5-1)*32+$B$6)"]);
  f.push(["=INDEX('기본 맵 및 CM3'!$H$12:$H$139,($B$5-1)*32+$B$6)"]);
  f.push(["=INDEX('기본 맵 및 CM3'!$M$12:$M$139,($B$5-1)*32+$B$6)"]);
  f.push(["=INDEX('기본 맵 및 CM3'!$N$12:$N$139,($B$5-1)*32+$B$6)"]);
  f.push(["=E17+INT((E18-E17)*INDEX('상수 및 근거'!$J$5:$J$8,$B$10)/4194304)"]);
  f.push(["=INT(INDEX('기본 맵 및 CM3'!$I$12:$I$139,($B$5-1)*32+$B$6)*INDEX('상수 및 근거'!$F$5:$F$14,$B$9)/32768)"]);
  f.push(["=INDEX('기본 맵 및 CM3'!$J$12:$J$139,($B$5-1)*32+$B$6)"]);
  f.push(["=IF(E14<1,0,INT(POWER(E14,0.15)*65536))"]);
  f.push(["=IF(E20<1,0,INT(POWER(E20,0.15)*65536))"]);
  f.push(["=INT(POWER(E21,0.15)*65536)"]);
  f.push(["=INT((E19-E17)*16777216/(E24-E23))"]);
  f.push(["=E19*256-INT(E25*E24/65536)"]);
  f.push(["=INT((INT(E25*E22/65536)+E26)/256)"]);
  f.push(["=IF($B$13=1,IF(E27<E17+$B$14,0,MIN(E19,E27)),MAX(E17,MIN(E19,E27)))"]);
  f.push(["=IF(INDEX('기본 맵 및 CM3'!$P$12:$P$139,($B$5-1)*32+$B$6)=\"N\",0,IF(AND($B$13=1,E28=0),0,INDEX('기본 맵 및 CM3'!$Q$5:$Q$8,$B$5)+INT(INDEX('기본 맵 및 CM3'!$K$5:$K$8,$B$5)*E28/4096)))"]);
  f.push(["=IF(E28=0,0,E29)"]);
  f.push(["=INDEX('기본 맵 및 CM3'!$O$12:$O$139,($B$5-1)*32+$B$6)"]);
  f.push(["=INDEX('기본 맵 및 CM3'!$D$12:$D$139,($B$5-1)*32+$B$6)"]);
  f.push(["=IF(OR(INDEX('기본 맵 및 CM3'!$P$12:$P$139,($B$5-1)*32+$B$6)=\"N\",AND($B$13=1,E28=0)),0,262144+E32*32768+E31*1024+E28*4)"]);
  f.push(["=INDEX('기본 맵 및 CM3'!$E$12:$E$139,($B$5-1)*32+$B$6)"]);
  f.push(["=E29*E34"]);
  f.push(["=INDEX('기본 맵 및 CM3'!$R$5:$R$8,$B$5)"]);
  f.push(["=INDEX('기본 맵 및 CM3'!$S$5:$S$8,$B$5)"]);
  f.push(["=INDEX('기본 맵 및 CM3'!$P$12:$P$139,($B$5-1)*32+$B$6)"]);
  summary.getRange("E5:E38").formulas = f;
  body(summary, "D5:G38");
  summary.getRange("D5:D38").format.font = { name: FONT, bold: true, size: 9 };
  summary.getRange("E5:E38").format.fill = C.green;
  summary.getRange("E8:E13").format.numberFormat = "0.0000";
  summary.getRange("E29:E30").format.numberFormat = "0";
  summary.getRange("D40:H40").merge();
  summary.getRange("D40").values = [["중요: HEAR vMag는 LUT 제곱근 근사이며, DAC 상수는 명목값입니다. 이 계산표는 시험 기댓값의 중앙값 산출용이고 최종 합격 허용오차는 시료 실측으로 별도 확정해야 합니다."]];
  summary.getRange("D40:H40").format = { fill: C.orange, font: { name: FONT, bold: true, size: 9 }, wrapText: true };
  setWidths(summary, [23, 16, 24, 25, 18, 13, 30, 3]);
  summary.freezePanes.freezeRows(4);

  // -------------------------- 필수성능 ID-6
  title(id6, "A1:Z1", "필수성능 ID-6 입력 음압별 예상 자극 전류 재계산");
  id6.getRange("A2:Z2").merge();
  id6.getRange("A2").values = [["프로그램 1, 전극/채널 26, 3,815 Hz, 자극 볼륨 4를 고정하고 오디오 볼륨과 음압만 바꾼 코드 모델입니다. 기존 기준서 명목 전류는 비교 열에 그대로 보존했습니다."]];
  id6.getRange("A2:Z2").format = { fill: C.pale, font: { name: FONT, size: 9, italic: true }, wrapText: true };
  const id6Headers = ["No.", "조건", "주파수 [Hz]", "오디오 볼륨", "자극 볼륨", "음압 [dB SPL]", "Sound1 ADC", "AGC peak", "FFT 보정", "대역 대표값", "대역", "PCM Amplitude", "코드 예상 전류 [uA]", "현 기준서 명목 [uA]", "차이 [uA]", "비고", "AGC 입력 [dB]", "AGC 이득 [dB]", "AGC 이득 [배]", "xMin gain", "x^p 입력", "xMin^p", "xMax^p", "계수 A", "계수 B", "Raw Amplitude"];
  id6.getRange("A4:Z4").values = [id6Headers];
  header(id6, "A4:Z4");
  const id6Rows = [
    [1, "최소 A", 3815, 1, 4, 35, 537],
    [2, "최소 B", 3815, 10, 4, 35, 596],
    [3, "일상 A", 3815, 1, 4, 70, 936],
    [4, "일상 B", 3815, 10, 4, 70, 1500],
    [5, "최대 A", 3815, 1, 4, 110, 1500],
    [6, "최대 B", 3815, 10, 4, 110, 1500],
  ];
  id6.getRange("A5:F10").values = id6Rows.map((r) => r.slice(0, 6));
  id6.getRange("N5:N10").values = id6Rows.map((r) => [r[6]]);
  id6.getRange("P5:P10").values = id6Rows.map(() => ["비교용이며 이 표 자체로 합격/불합격을 판정하지 않음"]);
  for (let row = 5; row <= 10; row += 1) {
    id6.getRange(`G${row}`).formulas = [[`=INT('상수 및 근거'!$B$18*POWER(10,(F${row}-'상수 및 근거'!$B$19-'상수 및 근거'!$B$20)/20))`]];
    id6.getRange(`Q${row}`).formulas = [[`=10*LOG10(MAX(1,G${row})*32/POWER(2,47))`]];
    id6.getRange(`R${row}`).formulas = [[`=IF(Q${row}<'상수 및 근거'!$B$21,Q${row}+INDEX('상수 및 근거'!$D$5:$D$14,D${row}),IF(Q${row}>'상수 및 근거'!$B$22,INDEX('상수 및 근거'!$E$5:$E$14,D${row})*Q${row}+INDEX('상수 및 근거'!$B$5:$B$14,D${row})-Q${row},INDEX('상수 및 근거'!$C$5:$C$14,D${row})))`]];
    id6.getRange(`S${row}`).formulas = [[`=POWER(10,R${row}/10)`]];
    id6.getRange(`H${row}`).formulas = [[`=INT(G${row}*S${row})`]];
    id6.getRange(`I${row}`).formulas = [[`=INDEX('상수 및 근거'!$J$25:$J$75,ROUND(ABS(C${row}/31.25-ROUND(C${row}/31.25,0))*100,0)+1)`]];
    id6.getRange(`J${row}`).formulas = [[`=INT(H${row}*I${row})`]];
    id6.getRange(`K${row}`).formulas = [[`=INDEX('상수 및 근거'!$Q$5:$Q$260,ROUND(C${row}/31.25,0)+1)`]];
    id6.getRange(`T${row}`).formulas = [[`=INT(8*INDEX('상수 및 근거'!$F$5:$F$14,D${row})/32768)`]];
    id6.getRange(`U${row}`).formulas = [[`=IF(J${row}<1,0,INT(POWER(J${row},0.15)*65536))`]];
    id6.getRange(`V${row}`).formulas = [[`=IF(T${row}<1,0,INT(POWER(T${row},0.15)*65536))`]];
    id6.getRange(`W${row}`).formulas = [[`=INT(POWER(21892,0.15)*65536)`]];
    id6.getRange(`X${row}`).formulas = [[`=INT((INDEX('기본 맵 및 CM3'!$N$12:$N$139,26)-INDEX('기본 맵 및 CM3'!$M$12:$M$139,26))*16777216/(W${row}-V${row}))`]];
    id6.getRange(`Y${row}`).formulas = [[`=INDEX('기본 맵 및 CM3'!$N$12:$N$139,26)*256-INT(X${row}*W${row}/65536)`]];
    id6.getRange(`Z${row}`).formulas = [[`=INT((INT(X${row}*U${row}/65536)+Y${row})/256)`]];
    id6.getRange(`L${row}`).formulas = [[`=MAX(INDEX('기본 맵 및 CM3'!$M$12:$M$139,26),MIN(INDEX('기본 맵 및 CM3'!$N$12:$N$139,26),Z${row}))`]];
    id6.getRange(`M${row}`).formulas = [[`=INDEX('기본 맵 및 CM3'!$Q$5:$Q$8,1)+INT(INDEX('기본 맵 및 CM3'!$K$5:$K$8,1)*L${row}/4096)`]];
    id6.getRange(`O${row}`).formulas = [[`=M${row}-N${row}`]];
  }
  body(id6, "A5:Z10", "center");
  id6.getRange("A5:F10").format.fill = C.input;
  id6.getRange("G5:M10").format.fill = C.green;
  id6.getRange("N5:P10").format.fill = C.orange;
  id6.getRange("I5:I10").format.numberFormat = "0.000000";
  id6.getRange("Q5:S10").format.numberFormat = "0.0000";
  id6.getRange("O5:O10").conditionalFormats.add("colorScale", { colors: [C.green, C.input, C.red] });
  id6.getRange("A12:P12").merge();
  id6.getRange("A12").values = [["해석: 기존 기준서 값은 1.5세대 선행 실측 기반이고, 코드 예상값은 Sound1 완제품 ADC 모델과 현행 CFX/CM3 수식을 결합한 명목값입니다. 차이가 큰 조건은 기준서의 예상 전류를 실측 전에 재검토해야 합니다."]];
  id6.getRange("A12:P12").format = { fill: C.orange, font: { name: FONT, bold: true, size: 9 }, wrapText: true };
  setWidths(id6, [7, 12, 12, 11, 11, 13, 12, 12, 11, 13, 8, 14, 16, 16, 12, 28, 14, 14, 13, 11, 12, 12, 12, 12, 12, 13]);
  id6.freezePanes.freezeRows(4);

  // -------------------------- PCM LUT
  title(lut, "A1:K1", "PCM Amplitude → 예상 출력 전류 LUT");
  lut.getRange("A2:K2").merge();
  lut.getRange("A2").values = [["요약 시트에서 선택한 프로그램의 DAC 설정과 mute mode를 사용합니다. Amplitude 0도 자극 패킷이 출력되면 offset 전류가 존재하지만, mute가 켜져 있으면 패킷 자체가 대체됩니다."]];
  lut.getRange("A2:K2").format = { fill: C.pale, font: { name: FONT, size: 9, italic: true }, wrapText: true };
  lut.getRange("A4:G4").values = [["PCM Amplitude", "패킷 비트값", "패킷 출력 시 전류 [uA]", "mute 반영 전류 [uA]", "BLE 표시 전류 [uA]", "전 level 대비 [uA]", "위상당 전하 [pC]"]];
  header(lut, "A4:G4");
  lut.getRange("A5:A260").values = Array.from({ length: 256 }, (_, i) => [i]);
  for (let row = 5; row <= 260; row += 1) {
    lut.getRange(`B${row}`).formulas = [[`=A${row}*4`]];
    lut.getRange(`C${row}`).formulas = [[`=INDEX('기본 맵 및 CM3'!$Q$5:$Q$8,'요약'!$B$5)+INT(INDEX('기본 맵 및 CM3'!$K$5:$K$8,'요약'!$B$5)*A${row}/4096)`]];
    lut.getRange(`D${row}`).formulas = [[`=IF(AND('요약'!$B$13=1,A${row}=0),0,C${row})`]];
    lut.getRange(`E${row}`).formulas = [[`=IF(A${row}=0,0,C${row})`]];
    lut.getRange(`F${row}`).formulas = [[row === 5 ? "=0" : `=C${row}-C${row - 1}`]];
    lut.getRange(`G${row}`).formulas = [[`=D${row}*INDEX('기본 맵 및 CM3'!$D$5:$D$8,'요약'!$B$5)`]];
  }
  body(lut, "A5:G260", "center");
  lut.getRange("A5:A260").format.fill = C.input;
  lut.getRange("C5:G260").format.fill = C.green;
  const chart = lut.charts.add("line", lut.getRange("A4:D260"));
  chart.title = "PCM Amplitude별 패킷 비트값 및 예상 전류";
  chart.titleTextStyle.typeface = FONT;
  chart.hasLegend = true;
  chart.legend = { position: "top", textStyle: { typeface: FONT } };
  chart.xAxis = { axisType: "textAxis", textStyle: { typeface: FONT, fontSize: 9 } };
  chart.yAxis = { numberFormatCode: "0", numberFormatSourceLinked: false, textStyle: { typeface: FONT, fontSize: 9 } };
  chart.setPosition("I4", "Q22");
  setWidths(lut, [15, 13, 21, 19, 18, 16, 18, 3, 14, 14, 14]);
  lut.freezePanes.freezeRows(4);

  // 공통 마감
  for (const s of [summary, id6, cm3, lut, constants]) addCommonSheetStyle(s);
  wb.recalculate();

  const inspectSummary = await wb.inspect({ kind: "region", sheetId: "요약", range: "A1:G40", maxChars: 12000, tableMaxRows: 45, tableMaxCols: 8 });
  const inspectId6 = await wb.inspect({ kind: "region", sheetId: "필수성능 ID-6", range: "A1:P12", maxChars: 10000, tableMaxRows: 15, tableMaxCols: 16 });
  const errorScan = await wb.inspect({ kind: "match", searchTerm: "#REF!|#DIV/0!|#VALUE!|#NAME\\?|#N/A", options: { useRegex: true, maxResults: 100 }, maxChars: 8000 });
  await fs.writeFile(path.join(QA_DIR, "inspect-summary.txt"), `${inspectSummary.ndjson}\n\n${inspectId6.ndjson}\n\nERRORS\n${errorScan.ndjson}`, "utf8");

  // 입력 변경 재계산 시험 후 원복
  const before = summary.getRange("E5:E38").values;
  summary.getRange("B8").values = [[35]];
  summary.getRange("B9").values = [[10]];
  wb.recalculate();
  const changed = summary.getRange("E5:E38").values;
  summary.getRange("B5:B10").values = [[3], [15], [7968.75], [110], [10], [1]];
  summary.getRange("B13").values = [[1]];
  wb.recalculate();
  const highBoundary = summary.getRange("E5:E38").values;
  summary.getRange("B5:B10").values = [[2], [25], [7968.75], [110], [10], [1]];
  summary.getRange("B13").values = [[2]];
  wb.recalculate();
  const inactiveBoundary = summary.getRange("E5:E38").values;
  summary.getRange("B5:B10").values = [[1], [26], [3815], [70], [1], [4]];
  summary.getRange("B13").values = [[2]];
  wb.recalculate();
  const restored = summary.getRange("E5:E38").values;
  await fs.writeFile(path.join(QA_DIR, "input-change-test.json"), JSON.stringify({ before, changed, highBoundary, inactiveBoundary, restored }, null, 2), "utf8");

  for (const sheetName of ["요약", "필수성능 ID-6", "기본 맵 및 CM3", "PCM-전류 LUT", "상수 및 근거"]) {
    const preview = await wb.render({ sheetName, autoCrop: "all", scale: 0.8, format: "png" });
    await fs.writeFile(path.join(QA_DIR, `${sheetName}.png`), new Uint8Array(await preview.arrayBuffer()));
  }

  const xlsx = await SpreadsheetFile.exportXlsx(wb);
  await xlsx.save(OUT_FILE);
  const reopened = await SpreadsheetFile.importXlsx(await FileBlob.load(OUT_FILE));
  reopened.recalculate();
  const reopenSummary = await reopened.inspect({ kind: "region", sheetId: "요약", range: "B5:E38", maxChars: 8000, tableMaxRows: 40, tableMaxCols: 4 });
  const reopenErrors = await reopened.inspect({ kind: "match", searchTerm: "#REF!|#DIV/0!|#VALUE!|#NAME\\?|#N/A", options: { useRegex: true, maxResults: 100 }, maxChars: 8000 });
  await fs.writeFile(path.join(QA_DIR, "reopen-check.txt"), `${reopenSummary.ndjson}\n\nERRORS\n${reopenErrors.ndjson}`, "utf8");
  console.log(JSON.stringify({ outFile: OUT_FILE, qaDir: QA_DIR, programSummary: prog, id6: id6.getRange("A4:O10").values, errors: errorScan.ndjson, reopenErrors: reopenErrors.ndjson }, null, 2));
}

await build();
