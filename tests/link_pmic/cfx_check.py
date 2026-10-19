"""Compile actual CFX backtel source against the existing RAM PCM boundary.

Expected 24-word blocks come from the packet grammar, independent of C pointer
arithmetic. Host execution does not validate FPGA/RF timing or physical voltage.
"""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "tmp/unit-tests/link-pmic-cfx"
SOURCE = ROOT / "src/1__cfx/internalDevice/driver_PCM_liveStimulation.c"
STANDBY, BACKTEL, FORWARD, DURATION = 0x25555, 0x35555, 0x507FF, 0x60000


def expected(frame: int, count: int, width: int, read: int) -> list[int]:
    tail = [0x100000 + i for i in range(24)]
    if frame > 9:
        words = [read] + [BACKTEL] * (2 * frame - 1)
    else:
        count = 2 if count < 0 else min(19, count)
        swap = frame >= 4
        capacity = 24 - (frame + 6 if swap else 2 * frame + 3)
        count = min(count, capacity)
        words = [FORWARD] + [STANDBY] * (frame - 1 + count)
        if swap:
            words += [DURATION, read] + [BACKTEL] * 3 + [DURATION | max(0, width - 13)]
        else:
            words += [read] + [BACKTEL] * (frame - 1 + 3)
        boundary = ((len(words) + frame - 1) // frame) * frame
        if 24 - boundary < frame:
            boundary = 24
        words += [BACKTEL] * (boundary - len(words))
    return words + tail[len(words):]


PREFIX = r'''
#define TDC_UNIT_TEST 1
#define TDC_UT_HOST 1
#include <driver_PCM_liveStimulation.h>
#include "tdc_ut_io_cfx.h"
#include <assert.h>
#include <stdio.h>
static ST__CFX_CM3_SharedMemory_ALL shared;
#undef Addr_SharedMem
#define Addr_SharedMem (&shared)
int g_pcmFrameNum_per_channel;
static int fifo[26];
static int writes;
int *tdc_ut_io_pcm_top(void) { return &fifo[24]; }
void tdc_ut_io_pcm_write(int *p, int word)
{
    assert(p >= &fifo[1] && p <= &fifo[24]);
    *p = word;
    writes++;
}
static void reset(void)
{
    for (int i = 0; i < 26; i++) fifo[i] = 0xDEAD;
    for (int i = 0; i < 24; i++) fifo[24-i] = 0x100000+i;
    writes=0;
}
'''

MAIN = r'''
int main(void)
{
    for (unsigned n=0; n<sizeof(cases)/sizeof(cases[0]); ++n)
    {
        const int *v=cases[n];
        g_pcmFrameNum_per_channel=v[0];
        shared.tdc_power_stable_nop_count=v[1];
        shared.currentMapData.stimulationPulsePhaseWidth=v[2];
        reset();
        backtelPcmOut();
        for (int i=0; i<24; ++i) assert(fifo[24-i]==v[3+i]);
        assert(fifo[0]==0xDEAD && fifo[25]==0xDEAD);
    }
    for (int f=1;f<=12;++f)
    {
        g_pcmFrameNum_per_channel=f;
        reset(); shared.backtelControlRegister=0x58;
        Enable_backtelCircuit();
        int onslot=f<=9 ? 19 : 0;
        for (int i=0;i<24;++i)
        {
            int wanted=i==onslot ? (0x60100|0x59) :
                (i>0 && i<f) ? 0x25555 : 0x100000+i;
            assert(fifo[24-i]==wanted);
        }
        assert(shared.backtelControlRegister==0x59);
        reset(); backtelPcmOut();
        reset(); Disable_backtelCircuit();
        int offslot=f<=9 ? 1 : 0;
        for (int i=0;i<24;++i)
        {
            int wanted=i==offslot ? (0x60100|0x58) :
                (i>offslot && i<f) ? 0x25555 : 0x100000+i;
            assert(fifo[24-i]==wanted);
        }
        assert(shared.backtelControlRegister==0x58);
        shareConnectionCheckFired_DiableBacktel();
        assert(shared.cfx_PCM_interface.conneded_ISDCheckPCM_state==2);
    }
    const int invalid[]={-1,0,13,24,100};
    for(unsigned n=0;n<sizeof(invalid)/sizeof(invalid[0]);++n)
    {
        g_pcmFrameNum_per_channel=invalid[n]; reset();
        Enable_backtelCircuit(); backtelPcmOut(); Disable_backtelCircuit();
        shareConnectionCheckFired_DiableBacktel();
        assert(writes==0);
        assert(shared.cfx_PCM_interface.conneded_ISDCheckPCM_state==0);
        /* Invalid read followed by a new valid map cannot fabricate completion. */
        g_pcmFrameNum_per_channel=4;
        shareConnectionCheckFired_DiableBacktel();
        assert(shared.cfx_PCM_interface.conneded_ISDCheckPCM_state==0);
    }
    /* Enable without read and duplicate publication are not read completion. */
    g_pcmFrameNum_per_channel=4;
    Enable_backtelCircuit(); shareConnectionCheckFired_DiableBacktel();
    assert(shared.cfx_PCM_interface.conneded_ISDCheckPCM_state==0);
    backtelPcmOut(); shareConnectionCheckFired_DiableBacktel();
    assert(shared.cfx_PCM_interface.conneded_ISDCheckPCM_state==2);
    shareConnectionCheckFired_DiableBacktel();
    assert(shared.cfx_PCM_interface.conneded_ISDCheckPCM_state==0);
    const int periods[]={-1,0,1,3,255,256};
    const int ms[]={300,300,100,300,25500,300};
    for(unsigned n=0;n<sizeof(periods)/sizeof(periods[0]);++n)
    {
        shared.tdc_backtel_period_100ms=periods[n];
        assert(tdc_link_backtel_period_ms()==ms[n]);
        m_link_connection_check_counter=ms[n]-1; LB_increaseCounter();
        assert(m_link_connection_check_counter==ms[n]);
        LB_increaseCounter(); assert(m_link_connection_check_counter==0);
    }
    puts("CFX backtel vectors, F10..12 fallback, guards, lifecycle and period: PASS");
    return 0;
}
'''


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    text = SOURCE.read_text(encoding="utf-8-sig")
    # Compile both read-register selections; the production switch stays intact.
    for read in (0x50400, 0x50600):
        vectors = [[f, n, w, *expected(f, n, w, read)]
                   for f in range(1, 13) for n in [*range(20), -1, 20, 100]
                   for w in (13, 268)]
        (OUT / f"expected_{read:x}.json").write_text(json.dumps(vectors), encoding="utf-8")
        source = PREFIX
        if read == 0x50600:
            source += "\n#undef conneded_ISDCheck_byISDPower\n#define conneded_ISDCheck_byForwardPath\n"
        source += "\n".join(line for line in text.splitlines() if not line.startswith("#include"))
        source += "\nstatic const int cases[][27] = {\n"
        source += ",\n".join("{" + ",".join(map(str, v)) + "}" for v in vectors) + "};\n" + MAIN
        generated = OUT / "backtel.c"
        generated.write_text(source, encoding="utf-8")
        dirs = [ROOT / "tests/unit/tdc_ut_cfx_io_host"] + [ROOT / "src/1__cfx" / p for p in
                ("internalDevice", "signalProcessing", "environment", "hear", "systemControl", "OTE_1_5_gen", "lib_cfx", "test")]
        command = [shutil.which("gcc") or "gcc", "-std=c99", "-Wall", "-Wextra", "-Werror"]
        command += ["-I" + str(d) for d in dirs] + [str(generated), "-o", str(OUT / "backtel.exe")]
        subprocess.run(command, check=True)
        subprocess.run([str(OUT / "backtel.exe")], check=True)
        print(f"read=0x{read:x}: {len(vectors)} full 24-word vectors")
    (OUT / "source_sha256.txt").write_text(hashlib.sha256(SOURCE.read_bytes()).hexdigest(), encoding="ascii")


if __name__ == "__main__":
    main()
