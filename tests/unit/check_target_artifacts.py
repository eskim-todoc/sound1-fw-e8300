"""Check final SDK images, retention, source identity and normal isolation.

Run after build_target_tests for all normal/test cores. No board access.
"""
from __future__ import annotations
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

ROOT=Path(__file__).resolve().parents[2]
SDK=Path(os.environ.get("TDC_E8300_SDK","C:/Program Files (x86)/ON Semiconductor/Ezairo 8300 SDK"))
REQUIRED={
    "CFX":["tdc_ut_cfx_run_unit","tdc_ut_cfx_io_run_unit","tdc_ut_hear_start","tdc_ut_hear_poll","tdc_ut_hear_release"],
    "CM3":["tdc_ut_cm3_vectors_run_unit","tdc_ut_cm3_vectors_expected","tdc_ut_cm3_vectors_observe_calculation_failure","tdc_ut_io_cm3_run_unit","tdc_ut_io_cm3_observe_unit","tdc_ut_report_begin","tdc_ut_report_poll","tdc_ut_report_cancel","tdc_ut_report_cfx_core_begin","tdc_ut_report_cfx_observed_begin","tdc_ut_report_cfx_values_begin"],
    "BOOT":["tdc_ut_boot_run_unit","tdc_ut_boot_release","tdc_ut_boot_report_begin","tdc_ut_boot_report_poll","tdc_ut_boot_report_cancel"],
}
REQUIRED["CFX"] += ["tdc_ut_cfx_batch_start", "tdc_ut_cfx_batch_poll", "tdc_ut_cfx_batch_service",
                    "g_tdc_ut_cfx_batch_mailbox"]
REQUIRED["CM3"] += ["tdc_ut_batch_cm3_begin", "tdc_ut_batch_cm3_begin_capture",
                    "tdc_ut_batch_cm3_poll", "tdc_ut_batch_cm3_cancel",
                    "tdc_ut_cfx_bridge_configure", "tdc_ut_cfx_bridge_begin",
                    "tdc_ut_cfx_bridge_poll", "tdc_ut_cm3_console_serve", "tdc_ut_console_poll"]
REQUIRED["BOOT"] += ["tdc_ut_boot_batch_begin", "tdc_ut_boot_batch_poll",
                     "tdc_ut_boot_console_serve", "tdc_ut_console_poll"]


def tool(command):
    return subprocess.run(list(map(str,command)),capture_output=True,text=True,encoding="utf-8",errors="replace",check=True,timeout=30).stdout


def main():
    records=[]
    for core in REQUIRED:
        shared=[]
        for normal in (False,True):
            folder=ROOT/"tmp/unit-tests/target"/core/("normal" if normal else "test")
            data=json.loads((folder/"checks.json").read_text(encoding="utf-8"))
            assert all(v["returncode"]==0 for v in data["compile"]),core
            assert data["link"] and data["link"]["returncode"]==0,core
            assert data.get("build_complete") and data.get("artifacts"),(core,"missing current artifact identity")
            for path,sha in data["artifacts"].items():
                assert hashlib.sha256((ROOT/path).read_bytes()).hexdigest()==sha,(core,path,"artifact changed since link")
            assert data.get("inputs"),"Rebuild with local header/linker identity checks"
            for path,sha in data["inputs"].items():
                assert hashlib.sha256((ROOT/path).read_bytes()).hexdigest()==sha,(core,path,"source changed since build")
            if core=="CFX":
                text=(folder/"test_image.map").read_text(encoding="utf-8",errors="replace")
                symbols=re.findall(r"::(\S+)\s+\((?:Function|Data),",text)
                size=dict(re.findall(r"Memory map for memory '(\w+)':\s+Size\s*=\s*\d+\s+Width\s*=\s*\d+ bits\s+Offset\s*=\s*\d+\s+Used\s*=\s*(\d+)",text))
            else:
                symbols=tool([SDK/"arm_tools/bin/arm-none-eabi-nm.exe","-g","--defined-only",folder/"test_image.elf"]).splitlines()
                size=tool([SDK/"arm_tools/bin/arm-none-eabi-size.exe",folder/"test_image.elf"]).strip().splitlines()[-1]
                if core=="CM3":
                    shared.append(next(v.split()[0] for v in symbols if v.split()[-1]=="cfx_cm3_sharedMemoryAll"))
            test_symbols=[v for v in symbols if re.search(r"(?:g_|s_)?tdc_ut_",v)]
            if normal:
                assert not test_symbols,(core,"test symbols leaked",test_symbols)
            else:
                for name in REQUIRED[core]:
                    assert any(name in v for v in symbols),(core,"entry discarded",name)
            records.append(dict(core=core,normal=normal,source_files=len(data["sources"]),size=size,
                                global_test_symbols=len(test_symbols),link_verified=True))
            if core in ("CM3","BOOT"):
                addresses={v.split()[-1]:int(v.split()[0],16) for v in symbols if len(v.split())==3}
                begin=addresses["__Heap_Begin__"];limit=addresses["__Heap_Limit__"]
                stack=addresses["__Main_Stack_Size"]
                assert begin<=limit and limit+stack==addresses["__stack"],(core,"static RAM overlaps stack")
                records[-1]["ram_budget"]={"static_end":hex(begin),"stack_bottom":hex(limit),
                    "reserved_stack_bytes":stack,"heap_gap_bytes":limit-begin,
                    "worst_call_depth_and_interrupt_stack_verified":False}
            if core=="BOOT":
                sections=tool([SDK/"arm_tools/bin/arm-none-eabi-objdump.exe","-h",folder/"test_image.elf"])
                workspace=re.search(r'^\s*\d+\s+\.tdc_ut_boot_workspace\s+([0-9a-f]+)\s+([0-9a-f]+).*\n([^\n]*)',sections,re.M)
                if normal:
                    assert workspace is None,'Workspace leaked into normal image'
                else:
                    assert workspace and 'CONTENTS' not in workspace[3],'Test workspace must be NOLOAD'
                    bytes_,address=int(workspace[1],16),int(workspace[2],16)
                    rtt=re.search(r'^\s*\d+\s+\.segger_rtt\s+([0-9a-f]+)\s+([0-9a-f]+)',sections,re.M)
                    assert rtt and int(rtt[2],16)==0x20000000,'RTT control address moved'
                    assert address>=0x20000000+int(rtt[1],16) and address+bytes_<=0x20008000,'Workspace overlaps RTT or BRAM boundary'
                    records[-1]['file_workspace']={'address':hex(address),'bytes':bytes_,'no_load':True,'normal_image_absent':True}
        if core=="CM3":
            assert len(set(shared))==1,("shared memory base moved",shared)
    paths=set()
    for core in REQUIRED:
        data=json.loads((ROOT/"tmp/unit-tests/target"/core/"test/checks.json").read_text(encoding="utf-8"))
        paths.update(data["sources"])
    paths.update(str(p.relative_to(ROOT)) for p in (ROOT/"src/test").glob("*.[ch]"))
    for path in sorted(paths):
        if Path(path).suffix not in (".c",".h"): continue
        text=(ROOT/path).read_text(encoding="utf-8-sig")
        # Existing unrelated bytes are not silently rewritten on a test task.
        # The check reports exact offending source instead of repairing it.
        text.encode("cp949")
        assert not any(c in text for c in ("\u2014","\u2013","\u2248","\u00b5","\u25ba")),path
    out=ROOT/"tmp/unit-tests/target"/"final-checks.json"
    out.write_text(json.dumps(dict(device_execution=False,records=records,source_encoding_checked=len(paths),
                                   shared_memory_base_unchanged=True),ensure_ascii=False,indent=2),encoding="utf-8")
    print("FINAL_ARTIFACTS_OK: six links/current sources, retained entries, normal symbols absent, shared base, UTF8/CP949")


if __name__=="__main__":
    main()
