# Sound1

E8300 임베디드 펌웨어 프로젝트.

## 메타
- Repository (`origin`, 본인): `eskim-todoc/sound1-fw-e8300` (평소 push 대상)
- Repository (`upstream`, 원본): `todoc-dev/sound1-fw-e8300` (PR 기여 대상)
- Git Workflow: 루트 `E:\Claude\CLAUDE.md`의 **Git Workflow (전역 규칙)** 참조
- Main branch: `claude_main` (origin 기준, 일상 작업 베이스) · `Develop` (upstream 동기화용 baseline)
- 담당 범위: SW/펌웨어 코드 + 문서 작성

## 원본 기여 흐름 (Fork-and-PR)
본인 저장소(`origin`)를 중간 단계로 사용. upstream 직접 push 권한 제한 대응.

- 일상 작업: `claude_<설명>` 브랜치 → `claude_main` 머지 → `git push origin claude_main`
- upstream 변경 받아오기: `git fetch upstream && git checkout Develop && git merge upstream/Develop && git push origin Develop`
- 원본 기여: GitHub 웹에서 `eskim-todoc/sound1-fw-e8300:claude_main` → `todoc-dev/sound1-fw-e8300:Develop` cross-repo PR 생성 (빈 repo로 만든 저장소라 공식 fork 관계는 없지만 공통 커밋 조상이 있어 PR 가능)

## Layout

```
Sound1/
├── CLAUDE.md       이 파일
├── src/            Eclipse 임베디드 프로젝트 (펌웨어 소스)
│   ├── 0__bootloader/
│   ├── 1__cfx/
│   ├── 2__cm3/
│   ├── 3__hear/
│   ├── 4__eeprom/
│   ├── 5__calibration/
│   ├── .metadata/          Eclipse workspace (경로 이동 시 재-import 필요)
│   ├── .clang-format
│   ├── .gitignore
│   └── CODEOWNERS
├── tests/          단위 · 통합 테스트 (초기: 빈 폴더)
└── docs/           SW 문서 (초기: 빈 폴더)
```

## 문서 작성
`docs/` 안의 파일명은 루트 CLAUDE.md의 **파일명 prefix 컨벤션**을 따름.

회사 실제 파일명 형식:
```
[세대] 코드-FW-모델 문서이름 Rev.N by 작성자.확장자
```

예시:
```
[1.5세대] SRS-FW-TD20B 소프트웨어 요구사항 사양서 Rev.1 by 김은수.md
[1.5세대] SDP-FW-TD20B 소프트웨어 개발 계획서 Rev.1 by 김은수.md
[분석] LED 운용 방식 분석 및 수정 방향 Rev.4 by 김은수.md
```

문서 기본 포멧은 Markdown (`.md`). 회사 공유폴더로 전달 시점에 `.docx`/`.xlsx` 변환 요청 가능. 자세한 규칙과 prefix 목록(`[분석]`, `[상세설계]`, `[테스트]` 포함)은 루트 `CLAUDE.md` 참조.

## 외부 의존 문서 (본인이 작성하지 않음)
다른 담당자가 작성해서 가져오는 참고 문서:
- IRS — 설계 입력 요구사항 사양서
- IOVM — 설계 입력 검증 매트릭스
- EPTC, EPTR — 필수성능시험 기준서/보고서

## 완성본 공유
작성 완료된 문서는 Dropbox 공유폴더로 **수동 복사**.
대상: `(주)토닥 Dropbox\인공와우\공유용 폴더[김은수] 1.5세대 FW 소별문서 공유폴더\...`

작업 중인 파일은 Sound1 repo에서만 관리하고, Rev 확정 후에 복사.
