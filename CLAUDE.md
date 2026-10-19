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
> **세션 시작 시**: ① 루트 `E:\workspace\rules\CLAUDE.md` 명시 Read → "루트 지침 확인 완료: <한 줄 요약>" 보고, ② 루트 `E:\workspace\rules\지침\작업 규칙.md` 확인, ③ **[`docs/상시 점검 대장.md`](docs/상시%20점검%20대장.md) Read → §1 «매번 알림» 항목의 현재 상태를 코드로 확인해 첫 응답에 보고.**

> [!CAUTION]
> **[`docs/상시 점검 대장.md`](docs/상시%20점검%20대장.md) 는 Claude 의 의무 체크리스트다.**
>
> | 시점 | 할 일 |
> |---|---|
> | 세션 시작 | 대장 Read → §1 «매번 알림» 현재 상태 코드 확인 → 보고 |
> | **완료 보고마다** | §1 «매번 알림» 항목을 **한 줄이라도 반드시 언급** |
> | 작업 착수 전 | §2 «잠재 위험» 이 작업 범위와 겹치는지 확인 |
> | **세션 종료 · compact 직전** | **[`docs/세션_인수인계/`](docs/세션_인수인계/README.md) 에 요약 작성** - 지금 상태·남은 것·**자산**(이번에 알아낸 함정과 규칙)을 담는다. 규약은 그 폴더 README |
>
> 2026-07-30 은수님 지시로 신설. **2026-08-05 현재 «매번 알림» 은 «실기 미검증 병합분 누적» 1건**(은수님 원격 근무로 실기를 나중에 몰아서 한다)이고, **«잠재 위험» 은 0건**이다(2026-08-06 impedance 마스크까지 해소).

E8300 임베디드 펌웨어 프로젝트.

## Layout

```
Sound1/
├── CLAUDE.md       이 파일
├── .editorconfig   인코딩 · 개행 통일 (charset=utf-8) — 아래 §소스 인코딩 규칙
├── .githooks/      pre-commit — CP949 손실 문자 커밋 차단
├── src/            Eclipse 임베디드 프로젝트 (펌웨어 소스)
│   ├── 0__bootloader/
│   ├── 1__cfx/
│   ├── 2__cm3/
│   ├── 3__hear/
│   ├── 4__eeprom/
│   ├── 5__calibration/
│   │   └── .settings/      Eclipse 인코딩 UTF-8 고정 (각 프로젝트 공통)
│   ├── .metadata/          Eclipse workspace (경로 이동 시 재-import 필요)
│   ├── .clang-format
│   ├── .gitignore
│   └── CODEOWNERS
├── tests/          단위 · 통합 테스트 (초기: 빈 폴더)
└── docs/           SW 문서 — 루트 컨벤션 적용 (참고/·사용방법/·tasks/<모듈>/<작업>/)
```

## 소스 인코딩 규칙 (필수)

> [!CAUTION]
> **소스(`.c` / `.h`) 주석에 CP949 비호환 문자 금지.** 한글은 안전하지만 `—`(em dash) 등은 **영구 손실**된다.
>
> **원인**: Eclipse clang-format 플러그인은 clang-format 을 외부 프로세스로 실행하면서 stdin/stdout 인코딩을 명시하지 않아 플랫폼 기본값(한국어 Windows = **CP949**)을 쓴다. 이때 `UTF-8 → CP949 → UTF-8` 왕복이 일어나고, CP949 에 매핑이 없는 문자는 `?`(0x3F)로 대체되어 되돌릴 수 없다. **파일은 UTF-8 을 유지한 채 특정 문자만 죽기 때문에 알아채기 어렵다.**
>
> **실제 피해 (2026-07-15)**: `—` 148건이 시한폭탄으로 잠복, 그중 75건이 이미 `?` 로 손상된 상태였다. 전량 ASCII 치환으로 해소.

| 금지 문자 | 대체 | 비고 |
|---|---|---|
| `—` (U+2014) · `–` (U+2013) | `-` | 피해 대부분이 이것 |
| `≈` (U+2248) | `~` | |
| `µ` (U+00B5) | `u` | `µs` → `us`. **`?` 가 아니라 `μ`(U+03BC)로 조용히 변질**되어 더 위험 |
| `►` (U+25BA) | `>` | |

한글 · `·` · `→` · `×` 는 CP949 에 존재하므로 **안전하다**. 문서(`.md`)는 Eclipse 를 거치지 않으므로 이 규칙의 대상이 아니다.

## 코드 포맷 규칙 (신규 코드 작성 시)

> [!IMPORTANT]
> **새 코드를 쓰기 전 [`docs/참고/clang-format/README.md`](docs/참고/clang-format/README.md) 를 따른다.** 은수님이 정한 조건 11개 중 **clang-format 이 자동 처리하는 것은 4개뿐이고 7개는 작성자 책임**이다.
>
> 특히 도구가 만들어 주지 않는 것: **모든 제어문에 `{}`** · **`case` 는 `{}` 블록 안, `break` 와 다음 `case` 사이 공백 줄** · **제어문 바로 위 코드/주석과 제어문 사이 공백 줄** · **`{` 다음에 빈 줄 두지 않기**.
>
> 본보기는 `docs/참고/clang-format/example.c` = **`src/2__cm3/source/main.c` 와 바이트 동일**하다.

## 소스 인코딩 - 방어 장치 (3중)

1. `.editorconfig` — `charset=utf-8` (에디터 무관)
2. `src/<프로젝트>/.settings/org.eclipse.core.resources.prefs` — Eclipse 인코딩 UTF-8 고정
3. `.githooks/pre-commit` — 위반 시 커밋 차단. **clone 후 1회 설치 필요**:
   ```sh
   git config core.hooksPath .githooks
   ```

> [!NOTE]
> 방어 장치 1·2 는 보조다. Eclipse 의 외부 프로세스 I/O 는 파일 인코딩 설정과 **별개 경로**라서 설정만으로는 완전히 막히지 않는다. **애초에 해당 문자를 쓰지 않는 것**이 유일한 확실한 해결책이며, 훅이 그것을 강제한다.
