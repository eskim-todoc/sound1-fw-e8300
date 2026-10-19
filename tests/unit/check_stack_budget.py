"""Measure ARM per-function frames using the exact isolated SDK compile inputs.

No product/source edits and no device execution. Frames are not whole-call-chain
or IRQ worst-case usage. Existing full links must also reserve sufficient RAM.
"""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'tmp/unit-tests/stack-budget'


def main():
    OUT.mkdir(parents=True,exist_ok=True)
    records=[]
    for core in ('CM3','BOOT'):
        build=json.loads((ROOT/f'tmp/unit-tests/target/{core}/test/checks.json').read_text('utf-8'))
        for item in build['compile']:
            cmd=list(item['command']);source=Path(cmd[cmd.index('-c')+1])
            if '/test/' not in source.as_posix() and source.name not in ('SEGGER_RTT_Wrapper.c','rtt_printf.c','stimulationParaCal.c'):
                continue
            relative=source.relative_to(ROOT).as_posix()
            digest=hashlib.sha256(source.read_bytes()).hexdigest()
            assert digest==build['sources'][str(source.relative_to(ROOT))],(relative,'Changed since link')
            obj=OUT/(core+'_'+Path(cmd[cmd.index('-o')+1]).name)
            cmd[cmd.index('-o')+1]=str(obj);cmd.insert(1,'-fstack-usage')
            result=subprocess.run(cmd,cwd=OUT,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=60)
            assert result.returncode==0,result.stderr
            usage=obj.with_suffix('.su');assert usage.exists(),str(usage)
            frames=[]
            for line in usage.read_text('utf-8').splitlines():
                name,bytes_,kind=line.split('\t')
                frame={'function':name.rsplit(':',1)[-1],'frame_bytes':int(bytes_),'kind':kind}
                assert kind=='static' and frame['frame_bytes']<4096,(relative,frame)
                frames.append(frame)
            records.append({'core':core,'source':relative,'sha256':digest,'command':cmd,
                'frames':frames,'maximum_frame_bytes':max((f['frame_bytes'] for f in frames),default=0)})
    assert records
    report={'device_execution':False,'complete_stack_safety_verified':False,
        'scope':'SDK -O0 static per-function frames only; no call-chain/IRQ/printf library worst-depth proof',
        'records':records,'maximum_frame_bytes':max(v['maximum_frame_bytes'] for v in records)}
    (OUT/'checks.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print('STACK_FRAMES_OK',len(records),'files; max',report['maximum_frame_bytes'],'bytes; total depth unverified')


if __name__=='__main__':main()
