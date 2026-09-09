# -*- coding: utf-8 -*-
"""SDDD 부트로더 순서도 5점"""
import sys
from draw_flow import draw   # draw_flow 가 stdout 인코딩을 설정한다

OUT = u'E:/workspace/projects/sound1-fw-e8300-backup/docs/tasks/인허가/20260909_1419_sddd-sound1/그림'

# ---------------------------------------------------------------- SI-ID-BOOT-001
draw([
    {'t': 'start', 'text': u'전원 인가'},
    {'t': 'proc', 'text': u'인터럽트 차단\n미처리 인터럽트 해제'},
    {'t': 'proc', 'text': u'표시등 · 저장장치 · 디버그 접근\n입출력 설정'},
    {'t': 'proc', 'text': u'무선 칩셋 절전 설정\n내부기 연결 미표시'},
    {'t': 'proc', 'text': u'코어 간 공유 메모리\n적재 완료 표시 해제'},
    {'t': 'proc', 'text': u'입출력 안정 대기'},
    {'t': 'end', 'text': u'동작 모드 판정 항목으로'},
], OUT + u'/그림15_BOOT_시스템초기화.png')

# ---------------------------------------------------------------- SI-ID-BOOT-002
draw([
    {'t': 'start', 'text': u'동작 모드 판정 시작'},
    {'t': 'proc', 'text': u'판정 핀 끌어올림'},
    {'t': 'dec', 'text': u'서비스 모드 레벨?', 'yes': u'예',
     'side': u'판정 핀 설정 복원', 'no': u'아니오', 'join': 5},
    {'t': 'proc', 'text': u'대기 후 재확인'},
    {'t': 'dec', 'text': u'두 번 모두 동일?', 'yes': u'예',
     'side': u'판정 핀 설정 복원', 'no': u'아니오', 'join': 5},
    {'t': 'sub', 'text': u'서비스 모드 실행\n(응용 기동 없음)', 'loop_to': 0,
     'loop_label': u'재기동'},
    {'t': 'end', 'text': u'기동 실행 항목으로'},
], OUT + u'/그림16_BOOT_동작모드판정.png')

# ---------------------------------------------------------------- SI-ID-BOOT-003
draw([
    {'t': 'start', 'text': u'기동 실행 시작'},
    {'t': 'dec', 'text': u'저장장치 초기화 성공?', 'side': u'기동 중단 · 오류 기록',
     'no': u'아니오', 'join': 'end'},
    {'t': 'proc', 'text': u'기동 정보 및 제조 정보 표 읽기'},
    {'t': 'proc', 'text': u'적재 대상 이름 결정'},
    {'t': 'sub', 'text': u'적용 대상 보관 위치 선택'},
    {'t': 'proc', 'text': u'해당 보관 위치로 이동 후 파일 열기'},
    {'t': 'dec', 'text': u'파일 헤더 정상?', 'side': u'기동 중단 · 오류 기록',
     'no': u'아니오', 'join': 'end'},
    {'t': 'dec', 'text': u'파일 종류?', 'yes': u'응용 파일',
     'side': u'목록 파일: 나열된 파일을\n차례로 적재', 'no': u'목록 파일', 'join': 9},
    {'t': 'sub', 'text': u'구획 처리 (검증 · 복호화 · 전송)'},
    {'t': 'proc', 'text': u'저장장치 동기화'},
    {'t': 'end', 'text': u'코어 기동 항목으로'},
], OUT + u'/그림17_BOOT_기동실행.png')

# ---------------------------------------------------------------- SI-ID-BOOT-004
draw([
    {'t': 'start', 'text': u'구획 적재 시작'},
    {'t': 'proc', 'text': u'구획 헤더 읽기'},
    {'t': 'dec', 'text': u'헤더 검사 값 일치?', 'side': u'적재 중단 · 오류 기록',
     'no': u'아니오', 'join': 'end'},
    {'t': 'proc', 'text': u'내장 키로 복호화 문맥 준비'},
    {'t': 'proc', 'text': u'구획 데이터 읽기 · 복호화'},
    {'t': 'proc', 'text': u'대상 메모리 영역 활성화 후 전송'},
    {'t': 'dec', 'text': u'구획 검사 값 일치?', 'side': u'적재 중단 · 오류 기록',
     'no': u'아니오', 'join': 'end'},
    {'t': 'proc', 'text': u'전송 완료 영역 쓰기 금지 잠금'},
    {'t': 'dec', 'text': u'남은 구획 있음?', 'yes': u'예', 'side': u'기동 구획 정보 확정',
     'no': u'아니오', 'join': 'end', 'loop_to': 1, 'loop_label': u'다음 구획'},
    {'t': 'end', 'text': u'적재 완료'},
], OUT + u'/그림18_BOOT_무결성검증복호화.png')

# ---------------------------------------------------------------- SI-ID-BOOT-005
draw([
    {'t': 'start', 'text': u'적용 대상 선택 시작'},
    {'t': 'dec', 'text': u'상태 파일 존재 · 유효?',
     'side': u'초기값으로 생성 후 기록', 'no': u'아니오', 'join': 3},
    {'t': 'proc', 'text': u'상태 파일 읽기'},
    {'t': 'dec', 'text': u'상태 = 갱신 시도?', 'yes': u'예',
     'side': u'기본 보관 위치 사용', 'no': u'아니오', 'join': 'end'},
    {'t': 'proc', 'text': u'시도 횟수 증가'},
    {'t': 'dec', 'text': u'시도 횟수 상한 초과?', 'yes': u'예',
     'side': u'대체 보관 위치 사용', 'no': u'아니오', 'join': 6},
    {'t': 'proc', 'text': u'기본 보관 위치로 복귀\n갱신 실패 기록'},
    {'t': 'end', 'text': u'상태 파일 갱신 후 반환'},
], OUT + u'/그림19_BOOT_적용대상선택.png')
