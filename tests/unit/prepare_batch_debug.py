"""Print current-build debugger setup, never connect/download/write a device."""
from pathlib import Path
import argparse
import hashlib
import json
import re
from build_target_tests import PROJECT_FOLDERS, artifact_paths, artifact_snapshot, configuration, input_snapshot

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "tmp/unit-tests/batch"


def current_build(core, root=None):
    """Reject legacy/incomplete records and changed inputs OR linked outputs."""
    root = ROOT if root is None else root
    folder = root / "tmp/unit-tests/target" / core / "test"
    try:
        checks = json.loads((folder / "checks.json").read_text(encoding="utf-8"))
        if (checks.get("core") != core or checks.get("test_enabled") is not True or
                checks.get("build_complete") is not True or
                not checks.get("link") or checks["link"]["returncode"] != 0):
            raise ValueError(f"{core}: 현재 시험 이미지를 먼저 빌드하세요")
        project = root / "src" / PROJECT_FOLDERS[core]
        sources, _ = configuration(project, core)
        records = checks.get("compile")
        if (not sources or not isinstance(records, list) or len(records) != len(sources) or
                any(r["returncode"] != 0 for r in records)):
            raise ValueError(f"{core}: 완전한 빌드 기록이 없습니다")
        current = input_snapshot(project, sources, root)
        if checks.get("inputs") != current or checks.get("sources") != {
                str(p.relative_to(root)): current[str(p.relative_to(root))] for p in sources}:
            raise ValueError(f"{core}: 빌드 뒤 소스/헤더/파일 목록이 변경됐습니다. 다시 빌드하세요")
        # Compare against the stored successful-link hashes. Computing a new
        # hash only for display would bless a replaced image or an old map.
        if checks.get("artifacts") != artifact_snapshot(core, folder, root):
            raise ValueError(f"{core}: 이미지/map이 빌드 기록과 다릅니다. 다시 빌드하세요")
    except (OSError, KeyError, TypeError, AttributeError, json.JSONDecodeError) as exc:
        raise ValueError(f"{core}: 빌드 기록/이미지/map을 확인할 수 없습니다. 다시 빌드하세요: {exc}") from exc
    return folder


def mailbox_address(text):
    found = re.findall(r"^\s*0x([0-9a-fA-F]+)\.\.0x([0-9a-fA-F]+)[^\n]*::g_tdc_ut_cfx_batch_mailbox \(Data, Global, \.bss\.XMEM\.1\)", text, re.M)
    if len(found) != 1:
        raise ValueError("CFX XMEM mailbox 기호를 하나로 확정할 수 없습니다")
    start, end = map(lambda n: int(n, 16), found[0])
    header = (ROOT / "src/1__cfx/test/tdc_ut_batch_cfx.h").read_text(encoding="utf-8")
    # The C contract owns the size; do not duplicate the enum count here.
    enum = header.split("enum tdc_ut_batch_word", 1)[1].split("};", 1)[0]
    enum = re.sub(r"/\*.*?\*/", "", enum, flags=re.S)
    count = len(re.findall(r"\bUTB_[A-Z_]+\b", enum)) - 1
    if end - start + 1 != count:
        raise ValueError("CFX mailbox 크기가 현재 헤더와 다릅니다")
    return start


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--token", type=int, default=1,
                        help="fresh arm identifier 1..8388607; change for each re-arm")
    parser.add_argument("--hear-ready", action="store_true",
                        help="include HEAR-ready=1 in instructions ONLY after hardware preparation")
    args = parser.parse_args()
    if not 1 <= args.token <= 0x7fffff:
        parser.error("token must be 1..8388607")
    cfx = current_build("CFX")
    cm3 = current_build("CM3")
    address = mailbox_address((cfx / "test_image.map").read_text(encoding="utf-8"))
    # These four files were compared with their saved link hashes above.
    images = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
              for core, folder in (("CFX", cfx), ("CM3", cm3)) for p in artifact_paths(core, folder)}
    commands = [
        "# 현재 시험 이미지 전용 준비 명령",
        "",
        "이 파일을 생성하는 것만으로 장치에 연결하거나 시험을 실행하지 않는다.",
        "시험 지그/현재 이미지/정상 루프 및 공유 RAM 생산자 정지를 먼저 확인한다.",
        "HEAR 준비를 확인하지 않았으면 --hear-ready 옵션을 사용하지 않는다.",
        "",
        "1. CFX 디버거에서 호출하고 0 반환을 확인:",
        f"   tdc_ut_cfx_batch_start({args.token}, 1, {int(args.hear_ready)})",
        "2. CM3 디버거에서 호출하고 0 반환을 확인 (주소는 XMEM 워드):",
        f"   call tdc_ut_cfx_bridge_configure(0x{address:x}, {args.token})",
        "3. CFX에서 아래 서비스를 실행. CM3와 독립적으로 계속 실행돼야 한다:",
        "   tdc_ut_cfx_batch_service(8388607, 4096)",
        "4. CM3에서 아래 콘솔을 실행. 반환을 기다리지 말고 RTT GUI로 이동:",
        "   call tdc_ut_cm3_console_serve(1)",
        "5. RTT GUI에서 [UT-CONSOLE] READY 확인 후 명령 한 줄:",
        "   ut auto 1",
        "",
        "종료 순서:",
        "1. ut quit로 요청하고 콘솔의 로그 배출/취소가 끝날 때까지 기다린다.",
        "2. CFX 서비스는 계속 실행한 채 CM3에서 복원 상태를 확인한다:",
        "   call tdc_ut_cfx_bridge_restored()",
        "3. 반드시 반환값 1을 확인한 뒤에만 CFX 서비스 중단을 요청한다:",
        "   g_tdc_ut_cfx_batch_service_stop = 1",
        "4. CFX 서비스가 반환하고 정지한 것을 확인한 후 시험용 장치를 재기동한다.",
        "복원값이 0이면 아직 정리 중이거나 확인 불가다. 서비스 중단/정상 코드 복귀/다음 시험을 하지 않는다.",
        "HEAR 완료 처리를 포함한 CFX 서비스를 계속 실행해 복원을 기다린다.",
        "완료되지 않으면 임의로 ready/done 값을 바꾸지 말고 지그의 안전한 상태를 확보한 뒤 통제된 재기동으로 종료한다.",
        "프로그램 카운터 강제 변경으로 시험 중간에 빠져나오지 않는다.",
        "CFX start의 ready 인자는 준비를 확인했다는 표시이지 하드웨어 정지 명령이 아니다.",
        "",
        "이미지 SHA256:", *[f"{p}: {sha}" for p, sha in images.items()], ""]
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "현재-빌드-시작명령.txt").write_text("\n".join(commands), encoding="utf-8")
    print("\n".join(commands))


if __name__ == "__main__": main()
