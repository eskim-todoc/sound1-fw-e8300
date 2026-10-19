"""Run existing audit tests and capture a bounded read-back fixture for document QA.

Only task-local copies are instrumented. Product sources and registered tests remain
unchanged. The output is PC/RAM evidence, not a board RTT measurement.
"""
import hashlib
import json
import re
import shutil
import subprocess
from pathlib import Path
from inspect_audit_units import QA, ROOT

def run(command, cwd):
    result = subprocess.run(command,cwd=cwd,capture_output=True,text=True,encoding='utf8',timeout=60)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    return result.stdout

def main():
    evidence=ROOT/'tmp/unit-tests/event-log'
    for name in ['checks.json','O0.log','O2.log','event_log_types.inc','event_log_product.inc']:
        shutil.copyfile(evidence/name,QA/name)
    source=ROOT/'tests/unit/tdc_ut_event_log_host.c'
    text=source.read_text(encoding='utf8')
    text=text.replace('#define ci_printi(...) ((void)0)', '#define ci_printi(...) printf(__VA_ARGS__)')
    old='begin(21, true); value("return", ci_event_log_read(), 0); value("no_write", s_tdc_writes, 0);'
    new='''begin(21, true);
    log = stored(); log.entity_count = 1; log.write_index = 1;
    memcpy(log.entities[0].time, time, 6);
    const CI_EVENT_LOG_BT_ADDR_T query_addr = {{0x12,0x34,0x56,0x78,0x9a,0xbc}};
    log.entities[0].bt_addr = query_addr; log.entities[0].event = 0x40;
    memcpy(s_tdc_disk, &log, sizeof(log));
    puts("QUERY_BEGIN");
    value("return", ci_event_log_read(), 0); value("no_write", s_tdc_writes, 0);
    puts("QUERY_END");
    value("query_preserved", memcmp(s_tdc_disk, &log, sizeof(log)) == 0, 1);'''
    assert text.count(old)==1
    probe=QA/'audit_readback_probe.c'
    probe.write_text(text.replace(old,new),encoding='utf8')
    exe=QA/'audit_readback_probe.exe'
    run(['gcc','-std=c11','-O2',probe.name,'-o',exe.name],QA)
    output=run([str(exe)],QA)
    (QA/'readback.log').write_text(output,encoding='utf8')
    query=output.split('QUERY_BEGIN\n',1)[1].split('QUERY_END',1)[0]
    expected='      <  0> : TIME {26-10-02-13-14-15} ADDR {12:34:56:78:9A:BC} EVENT {00000040}'
    assert expected in query
    assert '"field":"query_preserved","actual":1,"expected":1' in output
    timer_path=ROOT/'src/2__cm3/Gen1_5/common/ci_timer.c'
    header=(ROOT/'src/2__cm3/Gen1_5/common/ci_timer.h').read_text(encoding='utf-8-sig')
    timer=timer_path.read_text(encoding='utf-8-sig')
    timer=timer[timer.index('static bool    _ci_is_leap'):timer.index('int ci_timer_get_tick(void)')]
    a=timer.index('void TIMER_3_IRQHandler(void)'); b=timer.index('static bool _ci_is_leap(',a)
    timer=timer[:a]+timer[b:]
    types=re.search(r'typedef\s+struct\s*\{[^}]*\}\s*CI_TIMER_TIME_T\s*;',header)[0]
    base=re.search(r'#define\s+CI_TIMER_BASE_YEAR\s+(\d+)',header)[1]
    wrapper='#include <stdint.h>\n#include <stdbool.h>\n#include <stdio.h>\n#define ci_printv(...) ((void)0)\n#define CI_TIMER_BASE_YEAR '+base+'\n'+types+'\nvoid ci_timer_update_reference_time(CI_TIMER_TIME_T *, uint32_t);\n'+timer
    wrapper+='''
int main(void) {
    CI_TIMER_TIME_T value={26,10,2,13,14,15};
    ci_timer_update_reference_time(&value,0);
    ci_timer_increase_tick();
    for(int i=0;i<1000;i++) ci_timer_increase_tick();
    value=ci_timer_get_reference_time_after_self_update();
    printf("TIME %u-%u-%u-%u-%u-%u\\n",value.year,value.month,value.day,value.hour,value.min,value.sec);
    return !(value.sec==16 && value.min==14 && value.hour==13);
}
'''
    tp=QA/'audit_timer_probe.c'; tp.write_text(wrapper,encoding='utf8')
    te=QA/'audit_timer_probe.exe'
    run(['gcc','-std=c11','-O2',tp.name,'-o',te.name],QA)
    tout=run([str(te)],QA)
    assert tout.strip()=='TIME 26-10-2-13-14-16'
    (QA/'timer.log').write_text(tout,encoding='utf8')
    report={'device_execution':False,'scope':'Product function bodies; RAM file and captured printf; actual timer arithmetic with simulated ticks',
            'readback_visible_text':query.replace('\x00',''), 'timer':tout,
            'sources':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in [source,timer_path,ROOT/'src/2__cm3/Gen1_5/FS/ci_event_log.c']}}
    (QA/'extra-evidence.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps(report,ensure_ascii=False,indent=2))

if __name__=='__main__': main()
