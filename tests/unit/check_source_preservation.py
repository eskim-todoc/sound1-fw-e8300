"""Check product source bytes against a pre-work snapshot; never restore files.

Test-only directories may change. Existing product changes made before the
snapshot are deliberately retained, so this is not a comparison with Git HEAD.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import subprocess

ROOT = Path(__file__).resolve().parents[2]
TEST_ROOTS = (
    "src/test/", "src/0__bootloader/source/test/", "src/1__cfx/test/",
    "src/2__cm3/Cortex-M3-src/test/",
)


def is_test(path: str) -> bool:
    return any(path.startswith(prefix) for prefix in TEST_ROOTS)


def snapshot(root: Path) -> dict:
    """Read source/build settings only; no Git mutation or device access."""
    files = []
    # Use the same inventory rule as the initial snapshot: ignore generated IDE
    # and Release artifacts. Build settings are explicit globs, as at capture.
    command = ["rg", "--files", "src"]
    for pattern in ("*.c", "*.h", "*.ld", "*.hct", "*.s", "*.S", "*.asm", "*.bcf", ".cproject", ".project"):
        command += ["-g", pattern]
    result = subprocess.run(command, cwd=root, capture_output=True, text=True,
                            encoding="utf-8", check=True)
    for relative in sorted(result.stdout.splitlines()):
        path = root / relative
        files.append({"path": path.relative_to(root).as_posix(),
                      "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
    if not files:
        raise ValueError("No source files found; wrong project root")
    return {"scope": "source/build settings before test-only work", "files": files}


def compare(baseline: dict, current: dict) -> dict:
    def index(data: dict) -> dict[str, str]:
        result = {}
        for item in data["files"]:
            path = item["path"].replace("\\", "/")
            parts = PurePosixPath(path).parts
            if not path.startswith("src/") or ".." in parts or path in result:
                raise ValueError("Invalid or duplicate source path: " + path)
            digest = item["sha256"].lower()
            if len(digest) != 64 or any(c not in "0123456789abcdef" for c in digest):
                raise ValueError("Invalid digest: " + path)
            result[path] = digest
        if not result:
            raise ValueError("Empty source baseline")
        return result

    before, after = index(baseline), index(current)
    changed, allowed = [], []
    for path in sorted(before.keys() | after.keys()):
        if before.get(path) == after.get(path):
            continue
        entry = {"path": path, "change": "added" if path not in before else
                 "removed" if path not in after else "modified"}
        (allowed if is_test(path) else changed).append(entry)
    return {"product_source_preserved": not changed,
            "product_files_checked": sum(not is_test(p) for p in before),
            "product_changes": changed, "test_changes": allowed,
            "device_execution": False}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("baseline", type=Path)
    parser.add_argument("--capture", action="store_true", help="create new baseline; refuses overwrite")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    current = snapshot(ROOT)
    if args.capture:
        args.baseline.parent.mkdir(parents=True, exist_ok=True)
        with args.baseline.open("x", encoding="utf-8") as stream:
            json.dump(current, stream, ensure_ascii=False, indent=2)
        print("SOURCE_BASELINE", len(current["files"]), args.baseline)
        return
    report = compare(json.loads(args.baseline.read_text(encoding="utf-8")), current)
    report["baseline_sha256"] = hashlib.sha256(args.baseline.read_bytes()).hexdigest()
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False))
    if not report["product_source_preserved"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
