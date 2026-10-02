# Sound1 SOUP 사용 및 검토

2026-09-30 기준으로 FatFs, tiny-AES-c, onsemi CRC 지원 코드, SEGGER RTT를 식별하고 사용 경로를 확인했다. **외부 코드의 이름을 나열하는 데서 끝내지 않고, 사용 조건과 실제 확인 근거, 남은 검증을 함께 관리한다.** 펌웨어와 SDK는 수정하지 않았다.

AES 기본 연산은 실제 소스로 호스트 시험을 실행하여 정답과 일치했다. 파일 오류/전원 차단, CRC 하드웨어, RTT의 타깃 동작은 이번에 실행하지 않았다. 아래 항목을 모두 제품 수준에서 검증 완료한 것으로 해석하지 않는다. 이번 범위 밖의 CMSIS/NVM/HAL 나머지 기능, C 런타임, CFX/HEAR 및 QCC 의존성이 SOUP에서 제외된다는 뜻도 아니다.

## 1 식별과 사용 범위

| 항목 | 공급자와 확인한 버전 | 실제 사용 |
|---|---|---|
| FatFs | ChaN, onsemi SDK 배포본. 설치 SDK REVISION 4.5.0.107. `ff.c` R0.15 w/patch1, `ff.h` R0.15 / FF_DEFINED 80286 | CM3의 내부기 정보/맵/설정/OTA 파일, 부트로더의 실행 이미지와 부트 상태 파일 |
| tiny-AES-c | kokke/tiny-AES-c. 부트로더 `conanfile.py`에 1.0.0 표기. 원본 태그 일치는 미확인. CM3에는 `ci_aes_*` 래퍼 추가 | AES-128 ECB, 16바이트 블록. 부트 이미지 복호화와 CM3 내부기 정보 파일의 암복호화. 모든 맵 파일이 암호화되는 것은 아님 |
| CRC 지원 코드 | onsemi SDK 4.5.0.107의 `crc.h`와 제공 부트로더 CRC 코드. 자체 CM3 래퍼 `ci_crc_ccitt_calc`는 분리 관리 | E8300 CRC 하드웨어 설정/입력/결과 읽기. 파일 CRC16, 부트 헤더/섹션/매니페스트 검사. 독립적인 일반 CRC 계산 라이브러리로 잘못 분류하지 않음 |
| SEGGER RTT | SEGGER Microcontroller GmbH, 소스 헤더 7.56a. 본체 Revision 23622, 설정 파일 Revision 24316 | 부트로더 디버그 출력, CM3 디버그 출력과 UI 명령 입력. 로그 출력 전용으로 한정할 수 없음 |

FatFs와 RTT는 소스에 저작권 고지/재배포 조건이 있고, tiny-AES-c 부트로더 폴더에는 Unlicense가 있다. onsemi 코드는 별도 계약의 적용 대상이라는 고지가 있다. 라이선스 고지는 제품 안전성 증거가 아니며 외부 원문을 이 문서에 복제하지 않는다.

### 적용 파일을 구분해야 하는 이유

- FatFs SDK 소스 폴더의 `ffconf.h`는 `FF_VOLUMES=1`, `FF_FS_RPATH=0`이지만, 실제 프로젝트 include 경로의 설정은 각각 2, 1이다. 설치된 `libfatfs.a`가 어느 설정으로 만들어졌는지는 소스 이름만으로 확정할 수 없다. 빌드 옵션/생성 이력과 연결해야 한다.
- 부트로더 `.cproject`의 C 링커 목록에는 `evFATlib`가 있으나, C++ 링커 목록과 저장된 Debug `makefile/objects.mk/map`는 `fatfs`를 사용한다. 저장된 makefile의 실제 링크 명령은 `arm-none-eabi-g++`다. 이를 서로 다른 두 라이브러리가 동시에 사용된 증거로 해석하지 않는다.
- 저장된 map의 수정 시각은 부트로더 2026-08-12, CM3 2026-09-16이다. 현재 소스의 새 빌드나 실제 출하 이미지 확인 자료가 아니다.
- 소스별 SHA256, SDK 파일/라이브러리 SHA256, 시험 명령과 결과는 [SOUP 확인 근거](../../tasks/인허가/20260929_1755_DHF-펌웨어-설계출력/SOUP%20확인%20근거.json)에 기록했다. 해시는 같은 파일인지 구분하는 값이며 정확성/안전성을 보증하지 않는다. AES 키 값은 기록하지 않았다.

## 2 확인된 보호 처리와 한계

### FatFs

[CM3 파일 처리](../../../src/2__cm3/Gen1_5/FS/ci_filesystem.c)의 `ci_filesystem_read_with_crc_and_aes128`는 파일 열기, 읽기 결과와 실제 바이트 수를 확인하고 CRC 불일치 시 -1을 반환한다. 대응 쓰기 함수는 길이/패딩 조건과 블록별 쓰기 수를 확인한다. [OTA 수신](../../../src/2__cm3/Cortex-M3-src/BleCommunication/ci_ble_control_ota.c)의 `_fetch_packet_data`는 순서가 틀린 패킷과 쓰기 실패/짧은 쓰기를 오류 응답으로 처리한다.

그러나 일부 경로의 `f_sync`/`f_close` 반환값은 확인하지 않는다. 일반 `ci_filesystem_write`에서는 `f_write`의 반환값이 뒤의 `f_stat` 결과로 덮어써진다. 바이트 수 검사는 남아 있지만 저장 성공을 모든 오류 조건에서 보증한다고 쓸 수 없다. [부트 상태 저장](../../../src/0__bootloader/source/bootmgr/ci_boot.c)에서도 최종 저장 완료와 전원 차단 복구는 별도 확인해야 한다.

현재 include 설정은 `FF_FS_REENTRANT=0`, `FF_FS_LOCK=0`, `FF_USE_LFN=1`, `FF_FS_EXFAT=0`이다. CM3는 공유 파일 핸들을 사용한다. 같은 파일 시스템의 중첩/동시 접근을 허용한다는 전제로 설명하지 않으며, ISR과 디버그 명령을 포함한 접근 직렬화는 별도 확인한다.

### AES

[부트로더](../../../src/0__bootloader/source/bootloader.c)의 `bootloader_initialize_aes_ctx`는 NULL 입력을 검사한다. `bootloader_decrypt_data`의 `len`은 32비트 워드 수이며 4의 배수인지 검사한 뒤 바이트 순서를 변환하고 16바이트씩 복호화한다. 호출부의 크기 계산까지 같은 단위인지 구분해야 한다.

[CM3 맵 파일 처리](../../../src/2__cm3/Gen1_5/FS/ci_map.c)는 내부기 정보에 CRC/AES를 켜고 사용자 설정, 날짜와 프로그램 맵에는 CRC를 사용하면서 AES는 끈다. `ci_aes_init` 이후 `ci_aes_encrypt/decrypt`가 내부 컨텍스트를 사용한다. [파일 처리](../../../src/2__cm3/Gen1_5/FS/ci_filesystem.c)는 마지막 블록의 데이터/CRC/패딩 구성을 확인한다.

암복호화 정답 일치가 키 관리나 진본성 검증을 뜻하지 않는다. ECB와 CRC만으로 공격자의 변조를 인증된 것으로 걸러낸다고 설명하지 않는다. 공급자 README도 ECB의 용도 제약과 라이브러리 자체의 입력 경계 보호 부재를 명시한다.

### CRC

[CM3 CRC 래퍼](../../../src/2__cm3/Gen1_5/crc/ci_crc.c)는 CRC-CCITT 설정 후 4바이트를 리틀엔디언으로 조합해 입력하고 나머지 1~3바이트는 바이트 단위로 처리한다. CRC 레지스터는 공유 자원이므로 중첩 호출 여부를 확인해야 한다.

[부트로더 CRC](../../../src/0__bootloader/source/bootloader_crc.c)는 onsemi 제공 구현이다. 함수 정의의 `uint32_t *`와 [선언](../../../src/0__bootloader/include/bootloader/bootloader_internal.h)의 `uint8_t *`가 다르다. 마지막 부분 워드는 32비트로 읽으므로 입력 정렬과 실제 버퍼 여유까지 확인해야 한다. 소스에 `crc32`라는 필드 이름이 있다는 사실만으로 CRC-32로 단정하지 않는다.

부트로더는 CRC 결과로 오류를 반환하는 경로가 있지만 `APPDATA_FLAGS_VOLATILE` 섹션에서는 CRC 불일치를 0으로 바꾸는 예외가 있다. 따라서 “모든 CRC 오류에서 이미지 실행을 차단한다”는 문장은 현재 코드의 근거 범위를 넘는다.

### SEGGER RTT

양쪽 RTT의 기본 채널 0은 `SEGGER_RTT_MODE_NO_BLOCK_SKIP`이다. 전송 공간이 부족하면 로그를 버리는 방식으로 동작한다. 이것만으로 전체 실행시간이나 자극 처리 무간섭이 입증되는 것은 아니다.

CM3의 [UI 명령 처리](../../../src/2__cm3/Gen1_5/ui/tdc_ui_command.c)는 RTT를 통해 입력을 받고, [정상 반복 루프](../../../src/2__cm3/Cortex-M3-src/main.c)는 `tdc_ui_command_poll`을 호출한다. 디버그 입력은 맵 삭제, 표시/상태 강제 설정 등에도 연결되므로 실제 출하 빌드에서 접근 정책과 포함 범위를 확인한다.

[자체 printf 래퍼](../../../src/2__cm3/Gen1_5/SEGGER_RTT/SEGGER_RTT_Wrapper.c)는 256바이트 버퍼에 `vsnprintf`한 반환 길이를 제한하지 않고 `SEGGER_RTT_Write`에 넘긴다. 잘린 긴 문자열 또는 음수 반환에서는 전달 길이가 실제 버퍼를 벗어날 수 있다. 실제 OTA 출력이 이 래퍼를 사용하므로, 공급자 본체와 별개의 자체 연계 코드 확인 항목으로 관리한다. 현재 입력에서 실제 메모리 초과 읽기가 발생했다는 실측 결과는 아니다.

## 3 이번에 수행한 검사

2026-09-30, Windows 호스트의 GCC 16.1.0에서 두 `aes.c`를 각각 원본 그대로 빌드했다. NIST SP 800-38A F.1의 공개 AES-128 ECB 시험 키와 4블록 벡터를 사용했다. 복호화도 앞선 암호화 출력이 아니라 고정된 공식 암호문으로 시작했다.

| 대상 | 입력/비교 | 관찰 결과 | 판정 |
|---|---|---|---|
| 부트로더 tiny-AES-c | 공식 평문 4건→암호문, 공식 암호문 4건→평문 | 8건 모두 바이트 단위 일치, 종료 코드 0 | 호스트 기본 연산 통과 |
| CM3 tiny-AES-c | 같은 독립 정답 8건 | 8건 모두 바이트 단위 일치, 종료 코드 0 | 호스트 기본 연산 통과 |
| 분석 대상 파일 | 실행 전후 SHA256 비교 | 소스/SDK 입력 파일 불변 | 통과 |

시험 코드: [soup_aes_kat.c](../../tasks/인허가/20260929_1755_DHF-펌웨어-설계출력/soup_aes_kat.c), 실행/기록: [check_soup.py](../../tasks/인허가/20260929_1755_DHF-펌웨어-설계출력/check_soup.py). 이 시험은 CM3 고정 키 래퍼, 파일 암호화 형식, 부트로더 워드 순서 변환, 타깃 컴파일러, 전원 차단, CRC 주변장치 또는 RTT 시험이 아니다.

## 4 공급자 알려진 문제 대조

확인일은 2026-09-30이다. 이번에 열람한 공식 자료 범위만 기록한다. “공개된 문제가 없다”와 “안전성 검증을 완료했다”는 같은 뜻이 아니다.

| 공급자 자료 | 확인 내용 | 적용 판단 |
|---|---|---|
| [FatFs 갱신 이력](https://elm-chan.org/fsw/ff/updates.html) | R0.15a의 파일 잠금/재진입 관련 수정 | 현재 include의 LOCK=0, REENTRANT=0에서는 기재된 조건과 다름. 설치 라이브러리의 실제 빌드 설정과 일치시키기 전에는 최종 제외 판정 보류 |
| 같은 갱신 이력 | R0.15b 디렉터리 종료 오류, R0.15a 이후 FsInfo 오류 | 소스 표기의 R0.15 patch1과 발생 버전이 다름. 벤더 수정/바이너리 일치 여부는 별도 관리 |
| [tiny-AES-c README](https://github.com/kokke/tiny-AES-c), [Security](https://github.com/kokke/tiny-AES-c/security) | ECB/입력 경계 주의사항. Security 페이지에 게시된 advisory 없음 | ECB 용도와 입력 길이는 호출부에서 관리. advisory 부재로 결함 없음 판정하지 않음 |
| [SEGGER RTT 설명](https://kb.segger.com/RTT), [배포 기록](https://github.com/SEGGERMicro/RTT/releases) | 블로킹/비블로킹 모드와 초기화 조건 확인 | NO_BLOCK_SKIP 설정과 대조. 7.56a 전체 결함 목록과 타깃 영향 평가는 미완료 |
| onsemi 설치 SDK의 crc.h, REVISION | CRC API 계약과 SDK 버전 확인 | CRC 관련 공급자 알려진 문제 목록의 완결된 검토는 미완료 |

공식 시험 벡터: [NIST SP 800-38A](https://csrc.nist.gov/pubs/sp/800/38/a/final), F.1.1/F.1.2. 외부 원문은 참고 자료이며 현재 코드와의 대응은 위의 로컬 근거로 확인한다.

## 5 관리와 잔여 확인

담당 기능을 변경하거나 SOUP 버전/설정/컴파일러를 변경할 때 이 목록의 버전과 해시, 사용 경로, 공급자 알려진 문제를 다시 확인한다. 해당 SUVC/SITC 시험의 입력과 예상 결과를 연결하고, 실제 실행 기록/빌드 식별값을 확보한 뒤 결과 보고서에 판정한다. 아래 항목은 이번에 발견하여 등록한 미결이며 이미 수행된 조치로 적지 않는다.

| 관리 ID | 확인할 사항 | 필요한 종료 근거 |
|---|---|---|
| SOUP-C01 | FatFs 소스/include 설정과 실제 링크 바이너리 일치 | 출하 대상 빌드의 SDK/설정/라이브러리/이미지 식별값 |
| SOUP-C02 | 최종 저장 오류 및 중첩 접근 | f_sync/f_close 오류와 전원 차단/재기동 시험, 직렬화 검토 |
| SOUP-C03 | CRC 입력 경계와 예외 처리 | 선언/정의 및 정렬/부분 워드 검토, CRC 정답/변조 시험, VOLATILE 예외의 요구사항 근거 |
| SOUP-C04 | AES 제품 연계와 키/진본성 | 실제 파일/이미지의 정상/손상/잘못된 키 처리 결과와 보안 위험평가 |
| SOUP-C05 | RTT 래퍼 길이와 출하 접근 | 긴 출력/오류 반환 시험, 버퍼 포화 시험, 출하 디버그 명령 접근 정책 확인 |
| SOUP-C06 | 공급자 알려진 문제 및 나머지 외부 코드 | 적용 버전별 결함 영향 평가와 전체 SOUP 목록 검토 |

이 문서로 SOUP의 사용처와 관리 대상을 추적할 수 있다. **제품 사용에 문제가 없다는 최종 결론은 위 미결의 처리 근거와 해당 제품 검증 결과를 연결한 뒤 내린다.**
