---
name: Sound1 프로젝트 컨텍스트
purpose: E8300 임베디드 펌웨어 프로젝트 진입점 — 루트 지침 절대 우선
type: 메타
applies_to: [Sound1]
tags: [project, e8300, firmware, embedded]
---

# Sound1

**TL;DR**: E8300 임베디드 펌웨어 프로젝트. 루트 헌법/지침(글로벌 `~/.claude/CLAUDE.md`가 지정하는 `E:\workspace\rules\CLAUDE.md` 및 `E:\workspace\rules\지침\`)을 **절대 우선**으로 따른다.

> [!IMPORTANT]
> **Sound1은 루트 헌법/지침을 절대 우선으로 따른다.** 루트 진입점은 글로벌 `~/.claude/CLAUDE.md`가 지정하는 `E:\workspace\rules\CLAUDE.md`이며, 그 지침 체계(`E:\workspace\rules\지침\`)를 상속한다.
>
> **세션 시작 시**: ① 루트 `E:\workspace\rules\CLAUDE.md` 명시 Read → "루트 지침 확인 완료: <한 줄 요약>" 보고, ② 루트 `E:\workspace\rules\지침\작업 규칙.md` 확인.

E8300 임베디드 펌웨어 프로젝트.

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
└── docs/           SW 문서 — 루트 컨벤션 적용 (참고/·사용방법/·tasks/<모듈>/<작업>/)
```
