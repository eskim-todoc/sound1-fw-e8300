"""Align the integration plan with the current detailed design, preserving redlines."""
from copy import deepcopy
from datetime import datetime, timezone
from hashlib import sha256
from pathlib import Path
import json
import zipfile
from lxml import etree as E
from inspect_sip_alignment import TASK, OUT, W, NS, text

QA = TASK / 'qa/sip-sddd-alignment'
LIVE = OUT / 'SIP-FW-TD24A 소프트웨어 통합 계획서.docx'
BASE = '86ea1387389508ce2b16a8190e2b703dfc9f9bd4f0da72b5c5dc6c929fea1131'
DESIGN = 'e504352f8f6e374615aa81527d1ec6044e54eaa5df5c41afe15e3d8f3fdf3312'

# Original paragraph indices are pinned by BASE, not inferred from similar labels.
EDITS = {
170:'본 문서는 TD24A-A-EXT 제품의 소프트웨어 개발 절차로서 소프트웨어 통합 계획을 수립한다.',
173:'인공달팽이관 외부장치 소프트웨어 모듈들의 통합 과정을 계획하여, 소프트웨어 개발 절차에 따라 각 모듈이 안전하고 일관되게 동작하도록 하는 것을 목적으로 한다.',
176:'TD24A-A-EXT 제품의 펌웨어 개발 프로젝트에 참여하는 모든 구성원은 이 문서를 참고하도록 한다.',
214:'인공달팽이관 외부장치 소프트웨어의 기능 모듈과 코어별 역할은 <그림 1.>과 같이 구성한다.',
226:'소프트웨어 모듈의 구현 과정에서 아직 준비되지 않은 상대 기능은 테스트 코드로 대신할 수 있다.',
230:'부트로더의 기초 하드웨어 설정과 이미지 적재를 바탕으로 CFX의 인터럽트, 공유 상태 및 반복 실행 제어를 먼저 구현한다. 이후 HEAR, 마이크, PCM과 I2S 경로를 준비한다.',
234:'Signal Processing Module과 Internal Stimulation Device Execution Module은 독립적으로 구현할 수 있다. 다만 신호처리 결과를 자극 패킷으로 전달하는 단계에서는 맵, T/C 값과 프레임 조건을 함께 준비한다.',
238:'HEAR의 실행 준비와 함수체인 호출, 완료 이벤트 처리는 CFX의 System Control Module에서 구현한다.',
240:'CFX의 반복 실행과 입력 버퍼가 준비된 상태에서 절대값 최대값 연산과 창 적용 FFT/크기 연산을 구현한다. CFX의 AGC, 분석 입력 갱신과 대역 대표값 처리에 연결한다.',
244:'CM3 주변장치와 공유 메모리, 터치 센서 및 QCC 제어 핀을 먼저 준비한다. CFX의 기동 상태를 확인하고 공유 자료를 설정한 뒤 CFX의 반복 실행을 허가하도록 구현한다.',
247:'Signal Processing Module, Internal Stimulation Device Execution Module과 Bluetooth Communication Module은 독립적으로 구현할 수 있다. 맵 읽기와 변경 요청, 자극 파라미터 계산, 내부기 설정 및 명령 응답의 연결은 통합 단계에서 확인한다.',
251:'E8300 측에서는 CM3의 SPI 명령/응답과 제어 핀, CFX의 I2S 입력 경로를 구현한다. QCC 내부 모듈의 구현과 구분한다.',
252:'[자료 추가 위치] 여기에 QCC 내부 시스템 제어 모듈의 구현 순서와 선행 조건을 입력하세요. E8300의 상태 전달과 제어 핀 처리 조건을 QCC 자료와 대조한다.',
257:'E8300에서는 리모콘과 매핑 명령의 수신, 처리 및 응답 경로를 구현한다.',
261:'기초 하드웨어 설정, 저장장치 준비, 이미지 헤더/구획 검사와 메모리 적재를 먼저 구현한다. 이후 응용 코어 기동, 서비스 모드, 기본/대체 슬롯 선택과 복구 처리를 연결한다. 갱신 데이터 수신/저장은 CM3 응용 프로그램에서 구현한다.',
263:'각 코어의 모듈을 먼저 통합한 뒤 공유 메모리와 완료 이벤트를 사용하는 코어 간 경로를 연결한다. 실행 순서는 부트로더의 이미지 적재, 응용 코어 준비, 맵/내부기 설정, 실시간 자극의 순서로 확인한다.',
264:'상대 코어의 기능이 준비되지 않은 단계에서는 테스트 코드로 입력과 응답을 대신할 수 있다. 최종 통합에서는 실제 코어와 장치를 연결하여 같은 데이터와 완료 상태가 전달되는지 확인한다.',
265:'각 통합 단계의 입력, 전달 결과와 상태 전환을 확인한 뒤 다음 단계로 진행한다. 결과가 설계와 다르면 해당 유닛과 연결 부분을 수정하고 같은 조건으로 다시 확인한다.',
268:'CFX의 System Control Module을 기준으로 신호처리와 내부기 실행 모듈을 각각 통합한다. 상대 모듈의 입력은 준비된 맵 자료나 테스트 코드로 제공할 수 있다.',
270:'맵의 방향/마이크 수와 입력 상태에 따라 단일 입력, 두 마이크 빔포밍 또는 I2S 혼합 경로가 선택되는지 확인한다. 오디오/자극 볼륨을 변경하여 AGC 이득, C 상한과 매핑 계수의 반영을 확인한다.',
272:'CM3의 내부기 연결 상태, PCM 실행 모드와 지정 명령이 CFX의 출력 경로에 반영되는지 확인한다. 명령 요청과 전송 완료를 구분하고 대기/초기화, 연결 검사 및 실시간 자극 모드를 각각 확인한다.',
275:'신호처리와 내부기 실행 경로를 연결하고 설정 변경이 두 모듈에 반영된 뒤 자극 데이터를 출력하도록 통합한다.',
277:'입력 혼합, HEAR 최대값 연산, AGC, FFT/크기 연산, 대역 대표값/매핑과 PCM 출력을 순서대로 연결한다. 선택한 맵의 디지털 T/C, 전극과 프레임 조건이 자극 패킷에 반영되는지 확인한다.',
280:'HEAR 통합은 CFX 신호처리 통합에 포함한다. 혼합된 16표본의 절대값 최대값 연산이 끝나면 Event 0으로 CFX의 AGC와 FFT 입력 갱신을 진행하고, 512표본 FFT/크기 연산이 끝나면 Event 1으로 후속 대역 처리와 PCM 출력을 진행하는지 확인한다.',
284:'CM3의 System Control Module을 기준으로 신호처리, 내부기 실행과 블루투스 통신 모듈을 각각 통합한다. 이 단계에서는 상대 모듈의 상태와 응답을 준비하여 개별 연결을 확인한다.',
286:'맵의 펄스폭, 채널 수와 T/C 전류로 DAC 범위, 디지털 T/C 및 전송량이 계산되는지 확인한다. 선택한 내부기 파라미터와 계산 결과가 공유 메모리의 동일한 맵 자료에 대응하는지 확인한다.',
289:'내부기 연결 요청에 따라 FPGA/송신 전원 설정과 연결 확인이 진행되는지 확인한다. CM3의 명령 요청, CFX의 PCM 전송과 완료 통보, FPGA의 응답 처리를 연결하고 해제/재연결 시 상태 갱신도 확인한다.',
292:'리모콘의 연결 확인과 일반 명령 허용 상태를 구분하여 처리/응답을 확인한다. 매핑 연결/종료, 게인 변경, 배터리/충전/크래들 뚜껑 및 LED 요청이 해당 시스템 상태에 반영되는지 확인한다.',
295:'먼저 맵 처리와 내부기 실행을 시스템 제어에 연결하고, 그 다음 블루투스 명령과 상태 전달을 추가한다.',
297:'CM3의 맵 변경 요청, CFX의 맵 반영 완료, CM3의 자극 파라미터 계산과 CFX의 결과 적용을 순서대로 확인한다. 내부기 설정과 준비 상태를 확인한 뒤 Live PCM 모드로 전환되는지 확인한다.',
299:'입력 상태 수집 후 시스템 제어, 맵 갱신, 내부기 실행과 블루투스 통신 처리가 순서대로 연결되는지 확인한다. 맵/볼륨 변경, 연결 해제와 절전 요청이 겹치는 조건도 확인하고, 절전 시 CFX 정지 확인 후 CM3 대기로 전환되며 터치 입력으로 재시작되는지 확인한다.',
305:'SPI의 0x34 배터리/충전/크래들 뚜껑, 0x35 LED/갱신 연결, 0x36 오디오 연결 종류를 전달하여 CM3의 상태 반영과 응답을 확인한다. 리모콘/매핑 명령도 수신, 실행과 SPI 응답까지 연결하여 확인한다.',
309:'정상/손상 이미지의 헤더, 구획 크기와 CRC 검사, 암호화 구획의 복호화 및 메모리 적재를 연결한다. 검사 오류 시 해당 이미지의 응용 기동이 진행되지 않는지 확인하고, 기본/대체 슬롯의 시도 횟수와 실행 성공 상태에 따라 다음 기동 위치가 선택되는지 확인한다.',
311:'모듈의 구현 및 통합은 위의 계획에 따라 진행한다. 하위 항목과 유닛은 소프트웨어 상세 설계서에 정의한 목적, 입력/출력, 처리 및 동작 조건을 기준으로 구현하고 연결한다.',
314:'소프트웨어 상세 설계서의 항목과 유닛은 계층적으로 구성한다. 다른 유닛을 호출하거나 결과를 사용하는 항목은 필요한 하위 유닛을 먼저 구현한다. 독립적인 유닛은 병렬로 구현할 수 있다.',
317:'유닛의 입력/출력과 동작 조건을 확인한 뒤 같은 항목 안의 유닛, 기능 모듈, 코어 간 경로의 순서로 통합한다. 데이터를 전달할 때는 단위, 저장 형식과 배열 크기를 맞추고, 요청/완료 상태를 확인하여 다음 처리를 진행한다.',
}

ADDITIONS = {
257:[('[자료 추가 위치] 여기에 QCC 내부 블루투스 모듈의 구현 계획을 입력하세요. 무선 연결, 오디오 전달과 명령/응답 처리에 필요한 내부 유닛과 구현 순서를 작성한다.',True)],
277:[('CIS는 맵의 펄스폭에서 계산한 프레임 수와 전송량을 사용한다. NofM은 대역 수 16 이상인 맵에서 16개 대역 선택, 교차 배치와 시작점 조정 후 자극당 3프레임, 1ms당 8개씩 두 구간으로 출력하는지 확인한다.',False)],
280:[('Event 0 전에는 최대값 결과를, Event 1 전에는 FFT/크기 결과를 사용하지 않는지 확인한다. CM3가 읽는 maxAudioInput은 혼합 후 AGC 전의 최근 16표본 절대값 최대이며, RMS나 장시간 누적 최대값과 구분한다.',False)],
305:[('I2S 오디오를 입력하여 CFX의 마이크 혼합과 자극 출력을 확인한다. 일반 스트리밍은 I2S 게인 1, 크래들 마이크는 Gain B, 매핑 연결 중에는 마이크와 I2S 게인 1을 적용하는지 확인하고 시작/중단 전환도 확인한다.',False),
     ('CM3가 QCC_CTRL로 동작/종료 요청을, ISD_CHECK로 내부기 연결 상태를 전달하는지 확인한다. SPI의 오디오 종류와 I2S 입력 유효 상태가 CFX의 경로 선택에 맞게 반영되는지 확인한다.',False),
     ('[자료 추가 위치] 여기에 QCC 내부 모듈 간 통합 순서와 확인 조건을 입력하세요. QCC 자료를 받은 뒤 위의 SPI/I2S/제어 핀 조건을 양측 설계와 대조하고 필요한 차이만 반영한다.',True)],
309:[('CM3의 갱신 시작/데이터/종료 명령을 파일 기록 및 기동 위치 선택에 연결하고, 재기동 후 부트로더의 검사/적재까지 확인한다. OTA 명령의 슬롯 1~2와 부트로더 상태의 슬롯 1~4를 구분하며 수신 순서 오류, 저장 실패와 불완전 이미지 조건도 확인한다.',False),
     ('[자료 추가 위치] 여기에 QCC 측 E8300 갱신 데이터 전달 조건을 대조한 내용을 입력하세요. QCC 자체 펌웨어 갱신은 E8300 이미지 갱신과 구분하여 별도 계획을 추가한다.',True)],
}

def main():
    QA.mkdir(parents=True,exist_ok=True)
    raw=LIVE.read_bytes()
    assert sha256(raw).hexdigest()==BASE,'사용자 편집 감지: 기준을 다시 읽는다.'
    assert sha256((OUT/'SDDD-FW-TD24A 소프트웨어 상세 설계서.docx').read_bytes()).hexdigest()==DESIGN
    backup=QA/'before.docx'
    if backup.exists(): assert backup.read_bytes()==raw
    else: backup.write_bytes(raw)
    with zipfile.ZipFile(LIVE) as z:parts={n:z.read(n) for n in z.namelist()}
    root=E.fromstring(parts['word/document.xml'])
    ps=root.xpath('//w:body//w:p[not(ancestor::w:del)]',namespaces=NS)
    stamp=datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')
    seq=max(int(v) for v in root.xpath('//@w:id',namespaces=NS) if v.isdigit())+1
    def rev(tag):
        nonlocal seq
        e=E.Element(W+tag,{W+'id':str(seq),W+'author':'김은수',W+'date':stamp})
        seq+=1
        return e
    def run(value,rp=None,red=False):
        r=E.Element(W+'r')
        if rp is not None:r.append(deepcopy(rp))
        if red:
            pr=r.find(W+'rPr')
            if pr is None:pr=E.SubElement(r,W+'rPr')
            c=pr.find(W+'color')
            if c is None:c=E.SubElement(pr,W+'color')
            c.set(W+'val','C00000')
        t=E.SubElement(r,W+'t');t.set('{http://www.w3.org/XML/1998/namespace}space','preserve');t.text=value
        return r
    changes=[]
    for idx,value in EDITS.items():
        assert '·' not in value
        p=ps[idx];old=text(p)
        assert not p.xpath('.//w:fldChar|.//w:drawing|.//w:hyperlink',namespaces=NS)
        rp=p.find('.//'+W+'r/'+W+'rPr')
        rp=deepcopy(rp) if rp is not None else None
        # Preserve all original deletions. Original unmodified runs become new
        # deletions; earlier pending insertions are replaced, never nested.
        for child in list(p):
            if child.tag==W+'ins':
                assert child.get(W+'author')=='김은수'
                p.remove(child)
            elif child.tag==W+'r':
                deletion=rev('del');position=p.index(child);p.remove(child)
                for t in child.findall(W+'t'):t.tag=W+'delText'
                deletion.append(child);p.insert(position,deletion)
            else:assert child.tag in (W+'pPr',W+'del',W+'bookmarkStart',W+'bookmarkEnd'),child.tag
        ins=rev('ins');ins.append(run(value,rp,idx==252));p.append(ins)
        changes.append({'index':idx,'before':old,'after':value,'red':idx==252})
    for idx,items in ADDITIONS.items():
        anchor=ps[idx]
        for value,red in items:
            p=E.Element(W+'p');pr=deepcopy(anchor.find(W+'pPr'))
            if pr is None:pr=E.Element(W+'pPr')
            # Retain the original hyphen-list layout where the anchor uses it.
            # Word otherwise normalizes a new List Paragraph on accepting its
            # paragraph-mark insertion and discards the manual list indent.
            for tag in ('rPr','pageBreakBefore'):
                for e in pr.findall(W+tag):pr.remove(e)
            pr.insert(1 if pr.find(W+'pStyle') is not None else 0,E.Element(W+'keepLines'))
            rpr=E.SubElement(pr,W+'rPr');rpr.append(rev('ins'));p.append(pr)
            rp=anchor.find('.//'+W+'r/'+W+'rPr')
            ins=rev('ins');ins.append(run(value,rp,red));p.append(ins)
            anchor.addnext(p);anchor=p
            changes.append({'after_index':idx,'after':value,'red':red})
    # Track the few pagination properties needed by the expanded content.
    # Normal text and heading sizes remain unchanged; no hidden spacer is used.
    for idx,tags in ((276,('keepNext','keepLines')),(277,('keepLines',)),
                     (289,('keepLines',)),(305,('keepLines',)),
                     (310,('pageBreakBefore',))):
        pr=ps[idx].find(W+'pPr')
        assert pr is not None and pr.find(W+'pPrChange') is None
        old_pr=deepcopy(pr)
        # Schema order: keepLines/pageBreakBefore precede numPr and spacing.
        at=1 if pr.find(W+'pStyle') is not None else 0
        for tag in reversed(tags):
            if pr.find(W+tag) is None:pr.insert(at,E.Element(W+tag))
        change=rev('pPrChange');change.append(old_pr);pr.append(change)
    parts['word/document.xml']=E.tostring(root,encoding='UTF-8',xml_declaration=True,standalone=True)
    settings=E.fromstring(parts['word/settings.xml'])
    assert settings.find(W+'trackRevisions') is not None
    candidate=QA/'candidate.docx'
    with zipfile.ZipFile(candidate,'w',zipfile.ZIP_DEFLATED) as z:
        for name,data in parts.items():z.writestr(name,data)
    report={'baseline':BASE,'detailed_design':DESIGN,'changes':changes,
            'revised_paragraphs':len(EDITS),'added_paragraphs':sum(map(len,ADDITIONS.values())),
            'red_markers':sum(c['red'] for c in changes),'candidate_sha256':sha256(candidate.read_bytes()).hexdigest()}
    (QA/'patch.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k!='changes'},ensure_ascii=False))

if __name__=='__main__':main()
