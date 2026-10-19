"""Close owned design QA by binding actual final native and package proofs.

Read-only for deliverables and other owners' records. Writes only qcc_design QA.
Previous failed records are retained as history rather than rewritten as passes.
"""
from pathlib import Path
from datetime import datetime, timezone
import copy
import hashlib
import json
from collections import Counter

HERE = Path(__file__).resolve().parent
BASE = HERE.parent


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def read(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def write(path, value):
    Path(path).write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def reference(path):
    return {"path": str(Path(path).resolve()), "sha256": sha(path)}


def main():
    checked = datetime.now(timezone.utc).isoformat()
    coverage_path = BASE / "qcc_requirements/word-visual-coverage.json"
    coverage = read(coverage_path)
    coverage_sha = sha(coverage_path)
    assert coverage["status"] == "PASS" and not coverage["errors"] and not coverage["pending"]
    expected_pages = {"SAD": 55, "SDDD": 171, "SIP": 17}
    owned = {}
    for code, count in expected_pages.items():
        doc = coverage["documents"][code]
        assert doc["status"] == "PASS"
        assert doc["covered_page_count"] == doc["actual_page_count"] == count
        assert not doc["missing_pages"] and not doc["errors"]
        assert sha(doc["source"]) == doc["sourceSHA256_after"].lower()
        assert sha(doc["PDF"]) == doc["PDF_SHA256"].lower()
        assert len(doc["current_pngs"]) == count
        for page, item in doc["current_pngs"].items():
            assert sha(item["png"]) == item["sha256"].lower(), (code, page)
            assert doc["proofs"][page], (code, page)
        owned[code] = {
            "path": doc["source"], "sourceSHA256": doc["sourceSHA256_after"].lower(),
            "pages": count, "PDF": doc["PDF"], "PDF_SHA256": doc["PDF_SHA256"].lower(),
            "covered_page_count": count, "status": "통과",
            "individual_page_evidence": doc["proofs"], "current_pngs": doc["current_pngs"],
        }

    metadata = read(BASE / "final-word/design-native.json")
    for code, doc in owned.items():
        record = next(x for x in metadata if x["code"] == code)
        assert record["sourceSHA256"].lower() == doc["sourceSHA256"]
        assert record["sourceUnchanged"] and record["pages"] == doc["pages"]

    cells_path = BASE / "final-word/SAD-native-accepted-cells.json"
    cells = read(cells_path)
    assert cells["pass"] and not cells["differences"]
    assert cells["row_count"] == 83 and cells["column_count"] == 3
    assert len(cells["rows"]) == 83 and all(len(row["cells"]) == 3 for row in cells["rows"])
    native_xml = BASE / "final-word/SAD-native-accepted.xml"
    assert sha(native_xml) == cells["native_XML_sha256"].lower()
    sad_record = next(x for x in metadata if x["code"] == "SAD")
    assert Path(sad_record["nativeAcceptedXML"]).resolve() == native_xml.resolve()
    native_final = {
        **reference(cells_path), "sourceSHA256": owned["SAD"]["sourceSHA256"],
        "row_count": 83, "column_count": 3, "segments": 249, "pass": True,
        "native_XML": reference(native_xml),
        "scope": "Root actual native Word AcceptAllRevisions after field refresh and guarded G publication; read-only snapshot. This is actual cell preservation, not only an OOXML projection.",
    }

    selective_path = HERE / "native-trace-probe-v2/reject-allmarkup/native-cell-proof.json"
    selective = read(selective_path)
    assert selective["status"] == "PASS"
    assert selective["native_new_revisions_rejected"] == 235
    assert selective["saved_QA_revision_nodes"] == 360
    assert selective["normalized_redundant_format_nodes"] == 125
    proof_path = HERE / "native-trace-probe-v2/proof.json"
    proof = read(proof_path)
    assert proof["pass"] and proof["source_sha256"].lower().startswith("8af46e")
    assert proof["actual_native_new_revision_nodes_rejected"] == 235
    assert proof["stored_new_revision_nodes"] == 360
    assert proof["native_normalized_redundant_format_nodes"] == 125

    style_path = HERE / "current-style-qa.json"
    styles = read(style_path)
    for code, doc in owned.items():
        style = styles["documents"][code]
        assert style["sha256"].lower() == doc["sourceSHA256"]
        assert style["TDC_count"] == 59 and style["style_registration"] == "통과"
        assert all(x["status"] == "통과" for x in style["style_reference_graph"])

    cross_path = HERE / "cross-actual-saved.json"
    cross = read(cross_path)
    assert cross["status"] == "통과"
    for code, doc in owned.items():
        assert cross["input_hashes"][code].lower() == doc["sourceSHA256"]
    assert cross["input_hashes"]["test_current_values"] == sha(BASE / "qcc_tests/current-values.json")
    assert cross["input_hashes"]["requirements_contract"] == sha(BASE / "qcc_requirements/requirements-contract.json")

    principle_path = BASE / "qcc_requirements/principle3-independent-qa.json"
    principle = read(principle_path)
    summary = principle["summary"]
    assert summary["active_requirements"] == 82 and summary["modules"] == 14
    assert summary["units"] == 129 and summary["test_total"] == 237
    assert summary["undefined_current_IDs"] == 0 and not principle["undefined_references"]
    assert principle["native_SAD_followup"]["sourceSHA256"].lower() == owned["SAD"]["sourceSHA256"]

    position_path = BASE / "current-positions.json"
    positions = read(position_path)
    assert len(positions) == 12 and sum(len(x["entries"]) for x in positions) == 397

    checks_path = HERE / "checks-29.json"
    checks = read(checks_path)
    assert sorted(x["number"] for x in checks["checks"]) == list(range(1, 30))
    old_checks = copy.deepcopy(checks)
    prior_native = read(HERE / "native-review.json")
    old_failure_history = {
        "prior_checks": old_checks,
        "prior_native_review": prior_native,
        "later_failed_blank_cleanup": {
            "status": "실패",
            "candidate": "014C80CF / 35-page direct paragraph-mark deletion candidate",
            "failure": "Actual native accepted SAD trace contents were lost although the custom XML model passed. Candidate was rejected as invalid; exact 8AF baseline was restored by root.",
            "rollback": reference(BASE / "SAD-native-rollback.json"),
            "replacement": "Native Word tracked Range.Delete preserving the cell-end mark; actual accept/reject all83x3 cell verification. The failed XML approach is not reused.",
        },
        "native_60_page_baseline": {
            "sourceSHA256": proof["source_sha256"], "defect_pages": [22, 51],
            "failure": "Empty continued trace-row areas; 183 active blank paragraphs removed through actual native tracked deletion.",
        },
    }
    updates = {
        3: "실제 저장된82요구/14모듈/129유닛/237시험 선언과 미정의 현재ID0을 독립 대조했다. QCC4모듈/14유닛/15유닛시험과7통합시험의 기능/목적/입력/조건을 대조했다. 통합22의 실제 수신경계 CFX005/008/015 연결과 별도SIT013 범위를 확인했다. QCC007 일련번호 PS 시험조건의 직접 요구 추적 미비는 보존한다.",
        6: "요구/시험 저장본과 설계의 양방향 반복에서 보조 요구 추적/유닛 목적/통합22 수신경계 누락을 보완한 후 동일ID/조건/예상값/미실행 결과 범위를 다시 대조했다. 고정 반복 횟수 없이 실제 불일치 해소로 수렴을 판정했다.",
        23: "root가 현재 저장12파일의397개 위치를 수집하고 중앙목록을 같은 작업에서 갱신했다. current-positions.json의 실제12파일/397개 위치를 확인했다. 삭제추적/목차/비교원문을 현재 위치로 세지 않는다.",
        26: "실제 변경추적 저자는 김은수이고 그림의 원본bytes/이력을 보존했다. SAD 빈문단 정리는 실제 native Word 생성 트랙이며 저장360개 중 재열기 시 동일 실효서식125개가 정규화되고, 실제 남은235개를 새QA저자만 선택 거부하여0개가 됐다. 과거 revision text와 원래83행×3셀/본문 복원을 확인했다. 최종 필드/G게시 후 actual WordAcceptAll83×3cells249개도 재확인했다. 정적 C14N 모델만으로 native 복원을 단정하지 않는다.",
        28: "현재G SAD55/SDDD171/SIP17 총243쪽의 개별 원본크기 PNG 검사 또는 이미 직접 검토한 페이지의 정확한 본문 픽셀 일치 근거가 전부 연결됐다. 전체 목차/본문 흐름/그림/캡션/미비 안내/셀잘림/표 연결과 수정 페이지를 확인했다. SAD 최종 actual native 수락83×3cells도 통과했다. 문서 검증과 실제 제품시험/정식 승인은 분리한다.",
        29: "Word README와 실제DOTX를 기준으로59TDC 스타일 등록/기반 참조와 격리 지원2스타일 상속을 대조했다. 기존E번호체계를 보존하고 실제 Q상세표28개가 동종E본문 수준/폭8816/grid1304·7512/기준선에 맞는지 확인했다. 목적 본문/입출력처리표/정상 표뒤 문단/수식·그림/표행 흐름/변경추적/목차·필드를 전체 native243쪽에서 확인했다. raw 기본값/번호ID 차이를 정의 전체 동일이라고 주장하지 않는다.",
    }
    check_refs = {
        1: [cross_path], 2: [cross_path], 3: [principle_path, cross_path],
        4: [HERE / "input-audit.json"], 5: [cross_path], 6: [cross_path, principle_path],
        7: [style_path, HERE / "final-mic-patch/proof.json"],
        8: [cross_path, HERE / "SDDD-authored.json"],
        9: [cross_path], 10: [cross_path], 11: [HERE / "SIP-authored.json"],
        12: [BASE / "qcc_tests/qa.json"], 13: [cross_path],
        14: [HERE / "excel-targeted-native.json"], 15: [BASE / "qcc_tests/qa.json"],
        16: [cross_path], 17: [cross_path], 18: [cross_path],
        19: [cross_path], 20: [HERE / "authored.json"], 21: [HERE / "input-audit.json"],
        22: [position_path, coverage_path], 23: [position_path],
        24: [BASE / "qcc_tests/qa.json"], 25: [BASE / "qcc_tests/qa.json"],
        26: [proof_path, selective_path, cells_path], 27: [HERE / "input-audit.json"],
        28: [coverage_path, cells_path], 29: [style_path, coverage_path],
    }
    for item in checks["checks"]:
        number = item["number"]
        if number in updates:
            item["inspection_and_reason"] = updates[number]
            item["status"] = "통과"
        item["evidence"] = [reference(p) for p in check_refs[number]]
    checks["scope"] = "Owned3designDOCX document verification; unrelated Excel execution/format items are reasoned 해당없음 and linked to owner QA. QCC product tests remain 미실행/판정대기. All29 applicability decisions are recorded."
    checks["status"] = "통과"
    checks["remaining"] = []
    checks["checkedAt"] = checked
    checks["current_documents"] = {k: {x:v[x] for x in ["path", "sourceSHA256", "pages"]} for k,v in owned.items()}
    checks["native_final_accepted_cells"] = native_final
    checks["current_visual_coverage"] = reference(coverage_path)
    checks["current_principle3"] = reference(principle_path)
    checks["current_position_count"] = {"files": 12, "locations": 397, **reference(position_path)}
    checks["prior_failure_history"] = old_failure_history
    checks["product_state"] = "미실행 / 판정 대기; 문서 점검 통과는 제품시험 통과 또는 정식 승인이 아니다."
    checks["renderer_attempt"]["native_final_status"] = "통과"
    checks["renderer_attempt"]["native_final_page_count"] = 243
    checks["counts"] = dict(Counter(x["status"] for x in checks["checks"]))
    assert checks["counts"].get("실패",0) == checks["counts"].get("미실행",0) == 0

    native_review = {
        "status": "통과", "checkedAt": checked,
        "scope": "Current G owned SAD55/SDDD171/SIP17 full-page native document verification. Reviewer distribution is explicit in the coverage proof; contact sheets alone do not count as full-page verification.",
        "total_pages": 243, "documents": owned,
        "coverage": reference(coverage_path), "native_metadata": reference(BASE / "final-word/design-native.json"),
        "native_final_accepted_cells": native_final,
        "table_geometry_pt": prior_native["table_geometry_pt"],
        "native_review_sources": [reference(p) for p in [BASE / "root-SAD-final-full-qa.json", BASE / "qcc_requirements/SAD-final-full-qa.json", BASE / "qcc_tests/SAD-final-full-qa.json", HERE / "SAD-final-full-qa.json", HERE / "full-native-final.json"]],
        "observations": read(HERE / "SAD-final-full-qa.json")["observations"],
        "prior_failure_history": old_failure_history,
        "product_state": checks["product_state"],
        "unchecked_product_evidence": principle["unresolved_data"],
    }
    proof["G_published"] = True
    proof["current_G_published_sha256"] = owned["SAD"]["sourceSHA256"]
    proof["root_publication"] = reference(BASE / "SAD-native-safe-published.json")
    proof["post_fields_actual_native_final_accept"] = native_final
    proof["scope"] = "Native-generated empty paragraph deletion, actual new-only rejection/history and allbody restoration proof, then author/bookmark-only finalization. Root field/G publication preserves strict acceptance/rejection content. Final actual WordAcceptAll83x3 cells after publication passes. Historical before8AF guard remains unchanged."

    assert sha(coverage_path) == coverage_sha, "Coverage changed during close; rerun with stable proof"
    for code, doc in owned.items():
        assert sha(doc["path"]) == doc["sourceSHA256"], code
    write(proof_path, proof)
    # Refresh references to the proof we just augmented; historical snapshots stay unchanged.
    for item in checks["checks"]:
        if item["number"] == 26:
            item["evidence"] = [reference(p) for p in check_refs[26]]
    write(checks_path, checks)
    write(HERE / "native-review.json", native_review)
    result = {"status":"통과", "checkedAt":checked, "checks":29, "counts":checks["counts"],
              "owned_page_count":243, "current_documents":checks["current_documents"],
              "checks_file":reference(checks_path), "native_review_file":reference(HERE / "native-review.json"),
              "coverage":reference(coverage_path), "native_final_accepted_cells":native_final,
              "writes":"Only owned qcc_design QA files; no COM or deliverable writes."}
    write(HERE / "final-closure.json", result)
    print(json.dumps(result, ensure_ascii=True))


if __name__ == "__main__":
    main()
