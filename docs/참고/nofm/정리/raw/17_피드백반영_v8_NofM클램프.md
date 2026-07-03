---
name: nofm-output-pattern-피드백반영-v8-nofm클램프
description: 7차 피드백 — NofM duration>54usec를 "불가" 에러 대신 54usec로 클램프하고 제목에 빨간 경고만 표시
metadata:
  type: raw-working-doc
---

## TL;DR

54usec는 NofM이 정상 지원하는 "정확히 한계값"인데도 이전 로직은 `frames(p)>3`를 그대로 사용해 표시 오류(스크린샷상 54usec에서도 "duration=55usec 초과" 문구가 뜸)가 있었다. 근본적으로 설계를 바꿔, **duration>54usec는 더 이상 "불가"로 막지 않고 NofM 시뮬레이션 자체를 54usec로 고정(clamp)**하도록 수정했다.

## 반영 내역

- `nofmAvailability(K)`: 이제 채널 수(K&lt;16)만으로 판정 — duration은 더 이상 NofM을 "불가"로 만들지 않는다.
- `nofmClampedP(p) = Math.min(p, 54)`: NofM 관련 모든 렌더링(메인 타임라인 COLA·자극펄스, 2펄스 상세 그림)에 실제 `p` 대신 이 클램프값을 전달.
- NofM 패널 제목에 `(54usec 초과하여 54usec로 고정)` 문구를 붉은 계열(`--danger-fg`)로 추가 — duration&gt;54일 때만 표시, 그 외엔 숨김.
- 통계 요약 문구도 "NofM 불가" 대신 "(NofM은 3패킷 한계인 54usec로 고정해 시뮬레이션합니다)"로 정정.
- datalist 눈금에서 "55" 제거(54가 유일한 의미있는 경계이므로 인접 눈금 혼동 소지 제거).

## 검증

Python으로 p=13/47/54/55/100/255에서 `nofmClampedP`와 제목 경고 표시 조건을 재확인 — p≤54는 클램프 없음·경고 없음, p&gt;54는 항상 54로 클램프·경고 표시로 정확히 일치.

## 한계

여전히 브라우저 렌더링 확인은 못했다. 은수님이 지적하신 "54usec인데 왜 에러가 뜨는가"의 정확한 근본 원인(값 자체의 계산 오류였는지, datalist 인접 눈금(54/55) 스냅 관련 UI 이슈였는지)은 완전히 특정하지 못했지만, 이번 설계 변경(duration 기반 불가 상태 자체를 제거)으로 그 현상 자체가 재발할 수 없는 구조로 바뀌었다.
