"""Read-only verification of the cfx-pure expected-I/O documents.

Does not run/build product tests, create reports, modify sources or contact devices.
The manifest's prose is manually bound to source expectations; independent numeric
bounds are additionally recomputed without access to any observation file.
"""
from __future__ import annotations

import argparse
import builtins
import hashlib
import io
import json
import math
from pathlib import Path
import re
import sys
from unittest.mock import patch
from urllib.parse import unquote

sys.dont_write_bytecode = True
ROOT = next(p for p in Path(__file__).resolve().parents if (p / "tests/unit/check_cfx_vectors.py").is_file())
TASK = Path(__file__).resolve().parents[1]
MANIFEST = TASK / "qa/expected-io/cfx-pure.json"
OWNED = [1,2,3,4,5,6,8,9,13,14,16,17,18,20,21,22,23,24,25,28,29,30,31,32,34,35,38,39,41,42]
PURE = [8,13,16,17,18,20,22,23,24,25,28,29,30,31,32,34,35]
# Source-derived record layout, never derived from observed values/counts.
LAYOUT = {
    8: [(1,3)], 13: [(1,2)], 16: [(1,16),(2,16)], 17: [(1,16)],
    18: [(1,16)], 20: [(1,32),(2,16)], 22: [(1,2),(2,2)],
    23: [(1,6),(2,6)] + [(c,3) for c in range(10,22)],
    24: [(1,1),(2,1),(3,1)], 25: [(1,16),(2,16)],
    28: [(1,64)], 29: [(1,64)], 30: [(c,64) for c in range(1,5)],
    31: [(1,64),(10,64)], 32: [(c,32) for c in range(1,9)]+[(10,64),(11,64)],
    34: [(1,16),(2,16)], 35: [(1,16)]
}

def require(condition, message):
    if not condition:
        raise ValueError(message)

def read(path):
    return Path(path).read_text(encoding="utf-8-sig")

def data(path):
    return json.loads(read(path))

def normalized(value):
    return json.loads(json.dumps(value))

def section(text, heading):
    marker = "## " + heading + "\n"
    require(text.count(marker) == 1, "missing/duplicate section: " + heading)
    return text.split(marker,1)[1].split("\n## ",1)[0]

def source_array(source, name):
    found = re.search(r"\b"+re.escape(name)+r"(?:\[[^\]]*\])+\s*=\s*(\{.*?\});", source, re.S)
    require(found is not None, "source array missing: "+name)
    return [int(x) for x in re.findall(r"-?\d+",found.group(1))]

LOG_EXPORT = TASK / "qa/expected-io/cfx-pure-logs.json"
REPORT_SOURCE = "src/2__cm3/Cortex-M3-src/test/tdc_ut_report.c"
LOG_SOURCE = "src/test/tdc_ut_log.c"


def build_logs_export(manifest: dict, runner: str, reference: dict,
                      report_hashes: dict) -> dict:
    """사전 expected/독립 수식만 사용한다. actual 파일 접근이나 장치 실행은 없다."""
    indexed = {int(u["id"].split("-")[1]): u for u in manifest["units"]}
    inter = source_array(runner, "inter")
    boundary_gains = source_array(runner, "expected_gain")
    volumes = source_array(runner, "volumes")
    values = {
        (8,1): [0,1,0], (13,1): [1,2],
        (16,1): [10]*16, (16,2): [8]*16,
        (17,1): list(range(1,16))+[32], (18,1): [20]*16,
        (20,1): [i*64 for i in range(16)]+list(range(16)),
        (20,2): [-8388608,-8388608,0,8388544,8388607]+[0]*11,
        (22,1): [0,1], (22,2): [100,100],
        (23,1): boundary_gains[:6], (23,2): boundary_gains[6:],
        (24,1): [655], (24,2): [3276], (24,3): [163],
        (25,1): [-10,0,10,8388607,-8388608]+[0]*11,
        (25,2): [-20,0,20,8388607,-8388608]+[0]*11,
        (28,1): [80,48]+[0]*30+[5,3]+[0]*30,
        (29,1): list(range(32))+list(range(200,232)),
        (31,1): [25600]*32+[-12800]*32, (31,10): [1]*64,
        (32,10): [1]*64, (32,11): [1]*64,
        (34,1): list(range(16)), (34,2): list(range(16,32)), (35,1): inter,
    }
    for case in range(10,22):
        values[23,case] = [1]*3
    for case,value in enumerate(volumes,1):
        values[30,case] = [value]*32+[50]*32
    for case,value in enumerate([0,50,100,150,50,50,100,150],1):
        values[32,case] = [value]*32

    result = {
        "role": "cfx-pure",
        "schema_version": 1,
        "array_contract": {
            "expected": "헤더 제외 레코드 데이터 전체. 타깃 성공 시 A[0:N-1]에 그대로 사용하며 생략/반복 기호 없음.",
            "comparison": "records의 target-comparison은 expected callback이 존재하여 모두 일치할 때 PASS N=len(expected) F=0.",
            "variable": "variable_records는 별도 numeric_actual의 actual/lower/upper 삼중항. expected callback 없음/OBSERVED. 비교용 1 배열과 같은 case라도 별도 스트림.",
            "transport": "tdc_ut_report_cfx_begin 및 observed_begin은 중지된 생산자의 유효 XRAM 워드 주소/현재 빌드 심볼/완료/overflow=0을 요구한다. 주소는 결과 A배열 데이터가 아니며 고정 주소를 만들지 않는다.",
            "formatting_owner": "줄분할/CRLF/전체 RTT 문자열은 main이 실제 tdc_ut_log.c로 생성. 이 JSON은 로그 캡처가 아님.",
            "optimization": "기존 PC 기본 O0/동일 유닛 2회만 확인됨. O2/타깃 실행 결과 없음.",
            "noncopy": "정수 기대배열은 소스 expected 리터럴/사전 QA 기대값/범위 전개에서 작성. 관찰 actual 및 저장 로그를 생성 입력으로 사용하지 않음."
        },
        "source_sha256": {**manifest["source_sha256"], **report_hashes},
        "units": []
    }
    def variable(case: int, specs: list, notes: str) -> dict:
        fields, template = [], []
        for i,(name,bounds,unit) in enumerate(specs):
            lo,hi = bounds
            fields.append({
                "name": name, "index": 3*i, "range": [lo,hi], "unit": unit,
                "lower_index": 3*i+1, "upper_index": 3*i+2,
                "reason": "실제 SDK Log2/Exp2 근사와 고정소수점 절삭에 따라 허용 구간 내 변동. PC 대체값으로 고정하지 않음." if lo!=hi else "사전 구간이 단일값이다. 정상 비교가 성립할 때 이 값이어야 하며 실제 관찰은 별도 보존."
            })
            template.extend([None,lo,hi])
        return {
            "case": case, "mode": "target", "log_type": "target-observed",
            "summary_status": "OBSERVED", "count": len(template),
            "summary": f"OBSERVED N={len(template)} F=0",
            "fields": fields, "payload_template": template,
            "notes": notes+" template의 null은 실제 측정값이며 하한/중앙값/PC actual로 대체 금지. F=0은 비교기가 없기 때문이며 합격이 아니다."
        }
    for n in OWNED:
        original = indexed[n]
        exported = {
            "id": original["id"], "file": original["file"],
            "records": [], "variable_records": [], "pc_only_records": [],
            "not_emitted": []
        }
        if n not in PURE:
            exported["notes"] = "분석 전용 유닛. 현재 지정 분석 시나리오의 수치 결과 레코드/RTT 출력은 구현되지 않아 records가 비어 있다."
        else:
            cases = {int(c["key"][1:]):c for c in original["cases"] if "N" in c}
            for case,count in LAYOUT[n]:
                c = cases[case]
                payload = values[n,case]
                require(len(payload)==count==c["N"], f"export count: {n}/{case}")
                notes = c["title"]+". "+c["expected_fields"]["input_procedure_and_expected"].split("\n\n")[0]
                if (n==23 and case>=10) or (n==31 and case==10) or (n==32 and case>=10):
                    notes += " 이 배열의 1은 사전 구간 내 여부의 판정값이며 SDK 원시 수치가 아니다. 원시 수치는 variable_records의 동일 case로 별도 배출한다."
                if n in (24,25,34,35):
                    notes += " run_current의 동일 case도 같은 기대배열이며 중복 export하지 않는다. current의 별도 초기화/복원/구성 실패 조건은 기존 MD 참조."
                exported["records"].append({
                    "case": case, "expected": payload, "mode": "target",
                    "log_type": "target-comparison", "summary_status": "PASS",
                    "count": count, "source": c["source"], "notes": notes
                })
            word_count = sum(count+4 for _,count in LAYOUT[n])
            exported["pc_only_records"].append({
                "case": None, "key": "HOST-PASS", "mode": "pc",
                "log_type": "stdout-literal", "expected_text": f"CFX-{n:03d} HOST_PASS values={word_count}\n",
                "source": "tests/unit/check_cfx_vectors.py",
                "notes": "전체 유닛의 두 반복/복원/거부 assert 뒤 printf되는 줄. 개별 C번호나 A배열 출력은 없으므로 가상 PC RTT로 만들지 않는다. values는 4워드 헤더 포함."
            })
            exported["not_emitted"].append({
                "key": "BUSY-RESTORE-UNSUPPORTED",
                "notes": "미배출 결과/HEAR pending/done/IO done 거부, 상태 복원, 미지원99 및 AGC override 입력은 assert/반환값 검사다. 전용 성공 로그나 새 C레코드를 생산하지 않음."
            })
        if n==23:
            for i,row in enumerate(reference["agc"]):
                specs = [(name,bounds,unit) for name,bounds,unit in zip(
                    ("목표 이득","평활 이득","반환 선형 이득"),
                    row["bounds"],("Q8.16 dB","Q8.16 dB","Q12.12"))]
                exported["variable_records"].append(variable(
                    i+10,specs,f"입력={row['input']}, 볼륨={row['volume']}, 직전 이득=0/대체=0, 고정 계수."))
            for name in ("log_plus_1dB","exp_times_0p8"):
                exported["pc_only_records"].append({
                    "case": None, "key": name, "mode": "pc", "log_type": "stdout-literal",
                    "expected_text": f"CFX NUMERIC MUTATION_REJECTED {name}\n",
                    "source": "tests/unit/check_cfx_numeric_reference.py",
                    "notes": "빌드0, 변조 실행 nonzero 및 numeric 실패 assert 검출 후의 실제 출력 형식. 주소/행번호/OS 종료코드는 정적 expected 문자열로 만들지 않음."
                })
            exported["pc_only_records"].append({
                "case": None, "key": "NUMERIC-HARNESS", "mode": "pc", "log_type": "stdout-literal",
                "expected_text": "CFX NUMERIC_HARNESS_OK; SDK compile=False; target execution=0\n",
                "source": "tests/unit/check_cfx_numeric_reference.py",
                "notes": "--sdk 미지정 경로. SDK/장치 실행 합격 표시가 아님."
            })
            exported["pc_only_records"].append({
                "case": None, "key": "RANGE-OVERFLOW", "mode": "pc", "log_type": "no-output/assert-only",
                "expected": [1,1,1,1,1],
                "injected_predicates": [1,1,0,0,0],
                "raw_expected": [100,100,110,110,100,110,99,100,110,111,100,110,105,110,100],
                "failed": 3, "overflow": 1,
                "source": "tests/unit/check_cfx_vectors.py",
                "notes": "구간 5호출의 비교 expected는 모두1, 의도적 actual 판정은 1,1,0,0,0이다. 오류3/raw보존/용량528/400 overflow assert만 있으며 전용 출력 없음. 이 배열로 PASS RTT를 생성 금지."
            })
        if n==31:
            specs = []
            for channel,row in enumerate(reference["coeff"]):
                specs.extend([(f"채널{channel} A",row["bounds"][0],"Q16.8"),
                              (f"채널{channel} B",row["bounds"][1],"Q16.8")])
            exported["variable_records"].append(variable(10,specs,"채널 i=0..31, T=50+i%4/C=150+i%4/xMin=16/xMax=4096, power 대체=0."))
        if n==32:
            for mode in (1,2):
                specs = []
                for channel in range(32):
                    bounds = reference["mapping"][channel%8]["bounds"][mode-1]
                    specs.extend([(f"채널{channel} PCM",bounds,"자극 코드"),
                                  (f"채널{channel} 공유 출력",bounds,"자극 코드")])
                exported["variable_records"].append(variable(mode+9,specs,f"mute={mode}, offset=0/alert=0, T50/C150, 채널 입력은 독립 mapping 8개를 4회 반복."))
        for c in original["cases"]:
            if c["key"].startswith("LEGACY"):
                exported["not_emitted"].append({
                    "key": c["key"], "source": c["source"],
                    "notes": "기존 run_all 결과는 [-999,legacy_case,values] 2워드 헤더이고 expected 배열이 없어 현행 4워드 report_begin에 직접 투입 불가. 일반 values_begin으로 부분 배열을 수동 지정하면 OBSERVED는 가능하지만 현재 source가 정한 CORE-ID/C 자동 매핑은 없다. 이 export에서는 새 매핑/합격 로그를 발명하지 않음. "+c["expected_fields"]["input_procedure_and_expected"].split("\n\n")[0]
                })
        result["units"].append(exported)
    result["checks"] = {
        "target_comparison_records": 50, "target_expected_integers": 1148,
        "target_numeric_observed_records": 15, "numeric_triples": 228,
        "variable_payload_words": 684, "pc_literal_records": 20,
        "expected_from_actual": False, "device_execution": False,
        "all_file_reads_denied_during_build": True
    }
    return result


def check_logs_export(manifest: dict, emit: bool = False) -> None:
    """완전 배열과 관찰 필드를 검증한다. emit도 stdout만 사용하며 파일을 쓰지 않는다."""
    for relative,digest in manifest["source_sha256"].items():
        require(hashlib.sha256((ROOT/relative).read_bytes()).hexdigest()==digest,
                "source changed before logs export: "+relative)
    runner = read(ROOT/"src/1__cfx/test/tdc_ut_vectors_cfx.c")
    report = read(ROOT/REPORT_SOURCE)
    log = read(ROOT/LOG_SOURCE)
    require("int tdc_ut_report_cfx_begin(" in report and "int tdc_ut_report_cfx_observed_begin(" in report,
            "target report paths missing")
    require('s->expected == 0 ? "OBSERVED"' in log, "OBSERVED contract changed")
    sys.path.insert(0,str(ROOT/"tests/unit"))
    import cfx_numeric_reference as ref
    hashes = {p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in (REPORT_SOURCE,LOG_SOURCE)}
    def deny(*args, **kwargs):
        raise AssertionError("no file access during log expected generation")
    with patch.object(builtins,"open",deny), patch.object(io,"open",deny):
        reference = normalized(ref.references())
        generated = build_logs_export(manifest,runner,reference,hashes)
    if emit:
        print(json.dumps(generated,ensure_ascii=True,indent=2))
        return
    exported = data(LOG_EXPORT)
    require(exported==generated, "log export differs from source expectations")
    records = [r for u in exported["units"] for r in u["records"]]
    variable_records = [r for u in exported["units"] for r in u["variable_records"]]
    require(len(records)==50 and sum(len(r["expected"]) for r in records)==1148, "incomplete expected arrays")
    require(all(type(x) is int and -8388608<=x<=8388607 for r in records for x in r["expected"]),
            "expected values must be signed CFX24 integers")
    require(len(variable_records)==15 and sum(len(r["fields"]) for r in variable_records)==228,
            "numeric observation coverage")
    require(sum(len(r["payload_template"]) for r in variable_records)==684, "numeric raw word count")
    require(len([r for u in exported["units"] for r in u["pc_only_records"] if r["log_type"]=="stdout-literal"])==20,
            "PC literal coverage")
    require("\u00b7" not in read(LOG_EXPORT), "forbidden middot")
    print(json.dumps({"status":"PASS_LOG_EXPORT_ONLY", **exported["checks"]},ensure_ascii=False,indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    actions = parser.add_mutually_exclusive_group(required=True)
    actions.add_argument("--check", action="store_true")
    actions.add_argument("--check-logs", action="store_true")
    actions.add_argument("--emit-logs", action="store_true")
    args = parser.parse_args()
    m = data(MANIFEST)
    if args.check_logs or args.emit_logs:
        check_logs_export(m, emit=args.emit_logs)
        return
    require(m["role"] == "cfx-pure", "wrong role")
    require(m["expected_origin"]["actual_used_as_expected"] is False, "actual cannot be expected")
    units = {int(u["id"].split("-")[1]):u for u in m["units"]}
    require(sorted(units) == OWNED, "ownership/30-unit mismatch")
    # Source snapshots only detect stale document derivation; not a repository
    # preservation baseline and not an assertion about others' edits.
    for relative, digest in m["source_sha256"].items():
        require(hashlib.sha256((ROOT/relative).read_bytes()).hexdigest() == digest,
                "source changed since derivation; review required: "+relative)
    checked_fields = 0
    links = 0
    for n,u in units.items():
        file = ROOT/u["file"]
        require(file.name == u["id"]+".md" and file.parent.name == "CFX", "wrong output name")
        doc = read(file)
        require(doc.startswith("# "+u["id"]+" — "+u["title"]+"\n"), "title mismatch: "+u["id"])
        require("\u00b7" not in doc, "forbidden middot: "+u["id"])
        require("## 실제 PC 관찰과 한계\n" in doc, "observation separation missing")
        require("## 근거 소스와 기존 기록\n" in doc, "sources missing")
        for c in u["cases"]:
            body = section(doc, c["key"]+" "+c["title"])
            require(c["expected_fields"], "empty expected fields")
            for key,value in c["expected_fields"].items():
                require(isinstance(value,str) and bool(value), "field must be nonempty prose")
                require(value in body, f"{u['id']}/{c['key']}/{key} not reflected in MD")
                checked_fields += 1
            if "N" in c:
                exact = f"[UT-{u['id']}] {c['key']} PASS N={c['N']} F=0"
                require(exact in body and "실제 수집 로그 아님" in body, "expected RTT label/count mismatch")
            if c["key"].startswith("LEGACY"):
                require("OBSERVED" in body and "보류" in body, "legacy cannot invent PASS")
        for extra in u["supplemental_sections"]:
            body = section(doc, extra["title"])
            for value in extra["expected_fields"].values():
                require(value in body, "supplemental expectation missing")
                checked_fields += 1
        for value in u["common_expected_fields"].values():
            require(value in doc, "common preconditions/restoration missing: "+u["id"])
            checked_fields += 1
        for target in re.findall(r"\]\(([^)]+)\)",doc):
            path = unquote(target.split("#")[0])
            require((file.parent/path).resolve().exists(), f"broken link {u['id']}: {target}")
            links += 1
        actual_layout = [(int(c["key"][1:]),c["N"]) for c in u["cases"] if "N" in c]
        require(actual_layout == LAYOUT.get(n,[]), "source C/N mismatch: "+u["id"])

    runner = read(ROOT/"src/1__cfx/test/tdc_ut_vectors_cfx.c")
    checker = read(ROOT/"tests/unit/check_cfx_vectors.py")
    numeric_checker = read(ROOT/"tests/unit/check_cfx_numeric_reference.py")
    pure_list = source_array(checker, "units")
    require(pure_list == PURE, "checker supported unit list changed")
    require("repeat<2" in checker and '"-O2"' not in checker, "O0/two-repeat statement changed")
    total_cases = sum(len(v) for v in LAYOUT.values())
    payload = sum(count for v in LAYOUT.values() for _,count in v)
    words = payload+4*total_cases
    require((total_cases,payload,words)==(50,1148,1348), "vector count mismatch")
    require(m["counts"]["current_c_cases"]==total_cases and m["counts"]["current_payload_words"]==payload
            and m["counts"]["current_record_words"]==words, "manifest counts stale")
    cv = {(n,c["key"]):c["numeric_values"] for n,u in units.items() for c in u["cases"]}
    require(cv[23,"C1"]["input_db"] == source_array(runner,"boundary"), "AGC boundary not source")
    require(cv[23,"C2"]["input_db"] == source_array(runner,"boundary"), "AGC boundary not source")
    require(cv[23,"C1"]["goal"]+cv[23,"C2"]["goal"] == source_array(runner,"expected_gain"), "AGC goal not source")
    require(cv[35,"C1"]["I"] == source_array(runner,"inter"), "interleave expected not source")
    volumes = source_array(runner,"volumes")
    for i,v in enumerate(volumes,1):
        require(cv[30,f"C{i}"]["I"] == f"I0..31={v};I32..63=50", "volume expected not source")
    require([cv[24,f"C{i}"]["I"][0] for i in (1,2,3)]==[655,3276,163],"smoothing expectation mismatch")
    require([cv[8,"C1"]["I"],cv[13,"C1"]["I"]]==[[0,1,0],[1,2]],"flag expected mismatch")
    # Document non-copy proof: the independent reference calculation succeeds
    # with every file-open API denied, so saved actual/log JSON cannot feed it.
    sys.path.insert(0,str(ROOT/"tests/unit"))
    import cfx_numeric_reference as ref
    def deny(*args, **kwargs):
        raise AssertionError("file read forbidden during independent expectation generation")
    with patch.object(builtins,"open",deny), patch.object(io,"open",deny):
        expected = normalized(ref.references())
        expected_header = ref.render_header(expected)
        power_x = (1,8,16,64,128,512,4096,16384,262143)
        power = [list(ref.power_bounds(x)) for x in power_x]
        analytic = [math.floor((x*32/2**47)**(ref.P_Q23/2**23)*ref.NORMALIZE_Q16) for x in power_x]
    require(read(ref.HEADER)==expected_header, "independent header mismatch")
    for i,row in enumerate(expected["agc"]):
        case = cv[23,f"C{i+10}"]
        require({k:case[k] for k in ("input","volume","bounds")} == row, "AGC independently recomputed bounds mismatch")
        require(case["I"]==[1,1,1] and case["numeric_count"]==9, "AGC layout mismatch")
    coeff = cv[31,"C10"]
    require(all(r["bounds"][0]==coeff["A"] for r in expected["coeff"]), "coefficient A bounds")
    require([r["bounds"][1] for r in expected["coeff"][:4]]==coeff["B_by_mod4"], "coefficient B bounds")
    for mute in (1,2):
        case = cv[32,f"C{mute+9}"]
        require(case["x_by_mod8"]==[r["input"] for r in expected["mapping"]], "mapping inputs")
        require(case["bounds_by_mod8"]==[r["bounds"][mute-1] for r in expected["mapping"]], "mapping bounds")
    reference_case = cv[23,"CHECK-REFERENCE"]
    require(reference_case["x"]==list(power_x) and reference_case["analytic"]==analytic
            and reference_case["bounds"]==power, "power analytic check mismatch")
    require(all(lo<=v<=hi and hi-lo<20 for v,(lo,hi) in zip(analytic,power)), "power width/containment")
    range_source = checker.split("g_tdc_ut_cfx_vector_count=g_tdc_ut_cfx_vector_failed=g_tdc_ut_cfx_numeric_count=0;",1)[1].split("assert(g_tdc_ut_cfx_vector_failed==3);",1)[0]
    calls = [[int(x) for x in row] for row in re.findall(r"put_numeric\((-?\d+),(-?\d+),(-?\d+)\)",range_source)]
    predicates = [int(lo<=hi and lo<=v<=hi) for v,lo,hi in calls]
    range_case = cv[23,"CHECK-RANGE"]
    require(calls==range_case["calls"] and predicates==range_case["predicates"], "range injection input/result")
    require(sum(v!=1 for v in predicates)==range_case["failed"]==3, "expected three injected failures")
    require([v for row in calls for v in row]==range_case["raw"], "raw preservation expected")
    require("g_tdc_ut_cfx_numeric_actual[6]==99 && g_tdc_ut_cfx_numeric_actual[9]==111" in checker, "raw preservation assert")
    require("put_numeric(105,100,110);assert(g_tdc_ut_cfx_vector_overflow);" in checker, "overflow case missing")
    hdr = read(ROOT/"src/1__cfx/test/tdc_ut_vectors_cfx.h")
    for symbol,value in (("TDC_UT_CFX_VECTOR_CAPACITY",528),("TDC_UT_CFX_NUMERIC_CAPACITY",400)):
        require(re.search(r"#define\s+"+symbol+r"\s+"+str(value)+r"\b",hdr), "capacity changed")
    require(cv[23,"CHECK-OVERFLOW"]["after_counts"]==[528,400], "overflow expectation")

    # Only now read observations. They can confirm records/counts, never change
    # any expected variables or the manifest.
    observation = data(ROOT/"tmp/unit-tests/cfx-vectors/checks.json")
    require(observation["exit_code"]==0, "saved vector run nonzero")
    observed_counts = {int(n):int(v) for n,v in re.findall(r"CFX-(\d+) HOST_PASS values=(\d+)",observation["stdout"])}
    source_counts = {n:sum(count+4 for _,count in layout) for n,layout in LAYOUT.items()}
    require(observed_counts==source_counts, "saved host/source vector counts differ")
    numeric_observation = data(ROOT/"tmp/unit-tests/cfx-numeric-reference/checks.json")
    log = read(TASK/"qa/source-preserving-tests/suite/check_cfx_numeric_reference.log")
    require(numeric_observation["numeric_checks"]=={"23":36,"31":64,"32":128}, "numeric counts")
    require(numeric_observation["sdk_compile"] is None and numeric_observation["target_executed"] is False, "SDK/target scope")
    for mutation,key in zip(numeric_observation["mutations"],("MUTATION-LOG","MUTATION-EXP")):
        expected_mutation = cv[23,key]
        require(mutation["name"]==expected_mutation["name"], "mutation name")
        require(expected_mutation["define"] in numeric_checker, "mutation define source")
        require(mutation["build"]["returncode"]==0 and mutation["execution"]["returncode"]!=0, "mutation not rejected")
        require(expected_mutation["stderr_contains"] in mutation["execution"]["stderr"], "wrong mutation rejection cause")
        require(expected_mutation["expected_log"] in log, "saved MUTATION_REJECTED missing")
    analysis = data(ROOT/"tmp/unit-tests/cfx-analysis/checks.json")
    require(sorted(r["unit"] for r in analysis["records"])==[1,2,3,4,5,6,9,21,38,39,41,42], "analysis coverage mismatch")
    require(analysis["device_execution"] is False and analysis["whole_unit_pass"] is False, "analysis not runtime")
    require(m["counts"]["document_case_sections"]==sum(len(u["cases"]) for u in units.values()), "all case count")
    print(json.dumps({
        "status":"PASS_DOCUMENT_CHECK_ONLY", "role":"cfx-pure",
        "documents":len(units), "document_case_sections":sum(len(u["cases"]) for u in units.values()),
        "expected_fields_checked":checked_fields, "source_fingerprints":len(m["source_sha256"]),
        "links_checked":links, "current_C_cases":total_cases, "current_payload_words":payload,
        "current_record_words":words, "independent_numeric_comparisons":228,
        "expected_generation_with_all_file_reads_denied":True,
        "range_injection_calls":len(calls), "expected_injected_failures":3,
        "stored_mutation_rejections":2, "new_product_tests_run":False,
        "baseline_preservation_check":"main-owned/not-run", "device_execution":False
    },ensure_ascii=False,indent=2))

if __name__ == "__main__":
    main()
