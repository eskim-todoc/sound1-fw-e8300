---
name: Sound1 프로젝트 컨텍스트
purpose: E8300 임베디드 펌웨어 프로젝트 진입점 — 루트 지침 절대 우선
type: 메타
applies_to: [Sound1]
tags: [project, e8300, firmware, embedded]
---

# Sound1

**TL;DR**: E8300 임베디드 펌웨어 프로젝트. 루트 [`../../CLAUDE.md`](../../CLAUDE.md)와 그 지침 체계를 **절대 우선**으로 따른다. 본 파일은 Sound1 고유 정보(repo·기여 흐름·폴더 구조·작업 라우팅)만 담는다.

> [!IMPORTANT]
> **Sound1은 루트 [`../../CLAUDE.md`](../../CLAUDE.md)와 그 지침 체계를 절대 우선으로 따른다.** 본 파일은 Sound1 고유 정보(repo·메타·기여 흐름·폴더 구조·작업 라우팅)만 담고, 일반 규칙은 모두 루트에서 상속한다.
>
> **세션 시작 시**: ① 루트 `CLAUDE.md` 명시 Read → "루트 지침 확인 완료: <한 줄 요약>" 보고, ② 루트 [`docs/지침/작업 규칙.md`](../../docs/지침/작업%20규칙.md) 확인.
>
> **충돌 정책**:
> - 본 CLAUDE.md ↔ 루트 CLAUDE.md 충돌 시 → **루트 우선**. 본 파일이 잘못된 것으로 간주, 사용자에게 즉시 보고.
> - 본 프로젝트 `docs/지침/` ↔ 루트 `docs/지침/` 충돌 시 → 루트 [`문서 작성 규칙.md §2.1`](../../docs/지침/일반/문서%20작성%20규칙.md)에 따라 **프로젝트 우선** (의도적 도메인 특화 예외).

E8300 임베디드 펌웨어 프로젝트.

## 메타

- Repository (`origin`, 본인): `eskim-todoc/sound1-fw-e8300` (평소 push 대상)
- Repository (`upstream`, 원본): `todoc-dev/sound1-fw-e8300` (PR 기여 대상)
- Main branch: `claude_main` (origin 기준, 일상 작업 베이스) · `Develop` (upstream 동기화용 baseline)
- 담당 범위: SW/펌웨어 코드 + 문서 작성

## 원본 기여 흐름 (Fork-and-PR)

본인 저장소(`origin`)를 중간 단계로 사용. upstream 직접 push 권한 제한 대응.

- 일상 작업: `claude_<설명>` 브랜치 → `claude_main` 머지 → `git push origin claude_main`
- upstream 변경 받아오기: `git fetch upstream && git checkout Develop && git merge upstream/Develop && git push origin Develop`
- 원본 기여: GitHub 웹에서 `eskim-todoc/sound1-fw-e8300:claude_main` → `todoc-dev/sound1-fw-e8300:Develop` cross-repo PR (빈 repo로 만든 저장소라 공식 fork 관계는 없지만 공통 커밋 조상이 있어 PR 가능)

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
└── docs/           SW 문서 — 루트 컨벤션 적용 (지침/·참고/·사용방법/·tasks/<모듈>/<작업>/)
```

## 작업 라우팅

활성·완료 작업 목록은 [`docs/tasks/작업 목록.md`](docs/tasks/작업 목록.md) 참조. 신규 작업 시 먼저 조회해 기존 폴더 이어가기 또는 신규 분리 결정.
