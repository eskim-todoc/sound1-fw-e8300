"""Test HEAR comparison/lifecycle code with explicitly SYNTHETIC RAM fixtures.

No HEAR accelerator is executed or emulated. Synthetic completion events test
the runner's memory safety and its independent-reference comparisons only.
This is never hardware output or a device PASS. The ordinary CFX host runner
continues to require a real target for HEAR start/poll.
"""
from __future__ import annotations

import hashlib
import json
import re
import shutil
import subprocess
from pathlib import Path

from check_cfx_vectors import function
from tdc_ut_hear_reference import build

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "tmp/unit-tests/hear-reference"
SOURCE = ROOT / "src/1__cfx/test/tdc_ut_io_cfx.c"

PREFIX = r"""
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define TDC_UNIT_TEST 1
#include "tdc_ut_io_cfx.h"
#include "tdc_ut_hear_reference.h"
int g_tdc_ut_cfx_io_actual[TDC_UT_CFX_IO_CAPACITY];
int g_tdc_ut_cfx_io_expected[TDC_UT_CFX_IO_CAPACITY];
int g_tdc_ut_cfx_io_count, g_tdc_ut_cfx_io_failed, g_tdc_ut_cfx_io_overflow;
int g_tdc_ut_io_override_enabled;
volatile int g_tdc_ut_cfx_vector_done, g_tdc_ut_cfx_io_done;
static int mix[16], absolute[16], fft_input[512], fft_output[512], vmag[256], window[512];
#define HEAR_ADDR_AUDIO_MIX mix
#define HEAR_ADDR_AUDIO_MIX_ABS absolute
#define HEAR_ADDR_FFT_INPUT fft_input
#define HEAR_ADDR_FFT_OUTPUT fft_output
#define HEAR_ADDR_VMAG_OUTPUT vmag
#define HEAR_ADDR_FFT_WINDOW window
static struct { int BLOCK_EXP; } bfp;
#define D_HEAR_BFP (&bfp)
static struct { int function_chain0, function_chain1; } g_interrupt_flags;
static int requested_chain=-1;
#define HEAR_FC_agc_preprocessing 0
#define HEAR_FC_fft_vmag 1
/* Observe a request only. It does not produce any calculated hardware data. */
#define CALL_FUNCTION_CHAIN(chain) (requested_chain=(chain))
"""

MAIN = r"""
static void seed(void)
{
    assert(!g_tdc_ut_hear_pending);
    tdc_ut_hear_release();
    g_interrupt_flags.function_chain0=g_interrupt_flags.function_chain1=0;
    for (int i=0; i<512; i++)
    { fft_input[i]=1000+i; fft_output[i]=2000+i; window[i]=s_tdc_ut_hear_window_reference[0][i]; }
    for (int i=0; i<256; i++) vmag[i]=3000+i;
    for (int i=0; i<16; i++) { mix[i]=4000+i; absolute[i]=5000+i; }
    requested_chain=-1;
}
static void restored(void)
{
    for (int i=0; i<512; i++) { assert(fft_input[i]==1000+i); assert(fft_output[i]==2000+i); }
    for (int i=0; i<256; i++) assert(vmag[i]==3000+i);
    for (int i=0; i<16; i++) { assert(mix[i]==4000+i); assert(absolute[i]==5000+i); }
    assert(!g_interrupt_flags.function_chain0 && !g_interrupt_flags.function_chain1);
}
static int rounded_divide(int value, int scale)
{ return value < 0 ? -((-value+scale/2)/scale) : (value+scale/2)/scale; }
static void synthetic_capture(int vector, int exponent)
{
    /* All values below are TEST INPUT for the comparator, not device output. */
    memset(fft_output,0,sizeof(fft_output));
    bfp.BLOCK_EXP=exponent;
    for (int j=0; j<TDC_UT_HEAR_FFT_REFERENCE_BINS; j++)
    { int k=s_tdc_ut_hear_reference_bins[j];
      for (int c=0; c<2; c++) fft_output[2*k+c]=rounded_divide(s_tdc_ut_hear_fft_reference[vector-1][2*j+c],1<<exponent); }
    for (int i=0; i<256; i++) vmag[i]=rounded_divide(s_tdc_ut_hear_vmag_reference[vector-1][i],1<<(exponent+1));
    g_interrupt_flags.function_chain1=1;
}
static void record_format(void)
{
    int i=0;
    while (i<g_tdc_ut_hear_count)
    { assert(g_tdc_ut_hear_actual[i]==-999); int length=g_tdc_ut_hear_actual[i+3];
      assert(i+4+length<=g_tdc_ut_hear_count); i+=4+length; }
    assert(i==g_tdc_ut_hear_count);
}
int main(void)
{
    /* Explicit boundary and representation rejection; no product output used. */
    assert(hear_value_in_range(1064,1000,0,0));
    assert(hear_value_in_range(1065,1000,0,0));
    assert(!hear_value_in_range(1066,1000,0,0));
    assert(hear_value_in_range(537,1000,0,1));
    assert(!hear_value_in_range(538,1000,0,1));
    assert(!hear_value_in_range(-1,0,0,1));
    assert(!hear_value_in_range(8388608,0,0,0));
    assert(!hear_value_in_range(0,0,-1,0));
    assert(!hear_value_in_range(0,0,14,0));
    seed();
    window[123]=1;
    assert(tdc_ut_hear_start(2,2,1)==TDC_UT_IO_WINDOW_NOT_READY);
    assert(!g_tdc_ut_hear_pending && requested_chain==-1); restored();
    seed(); g_interrupt_flags.function_chain1=1;
    assert(tdc_ut_hear_start(2,2,1)==TDC_UT_IO_BUSY);
    assert(g_interrupt_flags.function_chain1==1 && requested_chain==-1);
    g_interrupt_flags.function_chain1=0;
    for (int vector=1; vector<=5; vector++)
    {
        seed(); assert(tdc_ut_hear_start(2,vector,1)==0);
        assert(requested_chain==1 && g_tdc_ut_hear_pending);
        for (int i=0; i<512; i++)
        { int expected=vector==1 ? 0 : vector==4 ? 32768 : vector==5 ? i%2 ? -32768 : 32768 : s_tdc_ut_sine16[(i*(vector==2 ? 1 : 2))%16];
          assert(fft_input[i]==expected); }
        synthetic_capture(vector,4); assert(tdc_ut_hear_poll()==0);
        record_format(); assert(g_tdc_ut_hear_failed==0 && !g_tdc_ut_hear_overflow);
        assert(g_tdc_ut_hear_fft_count==512 && g_tdc_ut_hear_vmag_count==256 && g_tdc_ut_hear_block_exp==4);
        assert(g_tdc_ut_hear_count==(vector==1 ? 8 : 282)); restored();
        assert(tdc_ut_hear_release()==0);
    }
    /* Halved FFT scale, double vMag scale, side-bin corruption and negatives fail. */
    for (int fault=0; fault<5; fault++)
    {
        seed(); assert(tdc_ut_hear_start(2,2,1)==0); synthetic_capture(2,4);
        if (fault==0) fft_output[65]/=2;
        if (fault==1) vmag[32]*=2;
        if (fault==2) vmag[17]=10000;
        if (fault==3) vmag[17]=-1;
        if (fault==4) bfp.BLOCK_EXP=14;
        assert(tdc_ut_hear_poll()==0 && g_tdc_ut_hear_failed>0); restored();
    }
    seed(); assert(tdc_ut_hear_start(2,1,1)==0); synthetic_capture(1,4); fft_output[223]=1;
    assert(tdc_ut_hear_poll()==0 && g_tdc_ut_hear_failed==1); restored();
    seed(); assert(tdc_ut_hear_start(1,1,1)==0); assert(requested_chain==0);
    for (int i=0; i<16; i++) absolute[i]=i<5 ? (int[]){100,32,17,100,99}[i] : 0;
    g_interrupt_flags.function_chain0=1;
    assert(tdc_ut_hear_poll()==0 && g_tdc_ut_hear_failed==0 && g_tdc_ut_hear_count==22); restored();
    seed(); assert(tdc_ut_hear_start(1,1,1)==0);
    for (int i=0; i<16; i++) absolute[i]=i<5 ? (int[]){0,32,17,100,99}[i] : 0;
    g_interrupt_flags.function_chain0=1;
    assert(tdc_ut_hear_poll()==0 && g_tdc_ut_hear_failed==1); restored();
    /* Without a completion, memory stays owned even after the bounded timeout. */
    seed(); assert(tdc_ut_hear_start(2,2,1)==0);
    for (int i=1; i<TDC_UT_HEAR_POLL_LIMIT; i++) assert(tdc_ut_hear_poll()==1);
    assert(tdc_ut_hear_poll()==TDC_UT_IO_TIMEOUT);
    assert(g_tdc_ut_hear_pending && !g_tdc_ut_hear_done && g_tdc_ut_hear_timed_out);
    assert(fft_input[0]==0 && fft_input[1]==25080);
    assert(tdc_ut_hear_release()==TDC_UT_IO_BUSY);
    assert(tdc_ut_hear_start(1,1,1)==TDC_UT_IO_BUSY);
    synthetic_capture(2,4); assert(tdc_ut_hear_poll()==0);
    assert(g_tdc_ut_hear_failed==1 && g_tdc_ut_hear_timed_out); record_format(); restored();
    puts("HEAR_COMPARATOR_LIFECYCLE_OK: synthetic comparison inputs only; accelerator NOT executed");
    return 0;
}
"""


def main() -> None:
    build()  # Regenerates independent Python mathematics in memory as a check.
    source = SOURCE.read_text(encoding="utf-8-sig")
    hear = source[source.index("/* HEAR는 CMD"):].rsplit("#endif", 1)[0]
    helpers = function(source, "put") + "\n" + function(source, "mark")
    generated = PREFIX + re.sub(r"\blong\b", "long long", helpers + hear) + MAIN
    OUT.mkdir(parents=True, exist_ok=True)
    translation = OUT / "hear_comparator.c"
    translation.write_text(generated, encoding="utf-8")
    gcc = shutil.which("gcc")
    if not gcc:
        raise RuntimeError("gcc unavailable")
    command = [gcc, "-std=c99", "-Wall", "-Wextra", "-Werror",
               "-I" + str(ROOT / "tests/unit/tdc_ut_cfx_io_host"),
               "-I" + str(ROOT / "src/1__cfx/test"), str(translation), "-o", str(OUT / "hear_comparator.exe")]
    compiled = subprocess.run(command, capture_output=True, text=True, timeout=60)
    if compiled.returncode:
        raise RuntimeError(compiled.stdout + compiled.stderr)
    ran = subprocess.run([str(OUT / "hear_comparator.exe")], capture_output=True, text=True, timeout=10)
    evidence = {"scope": "comparison and lifecycle code; SYNTHETIC RAM data/events; NOT HEAR execution",
                "hardware_executed": False, "device_pass_claimed": False,
                "source": str(SOURCE.relative_to(ROOT)), "source_sha256": hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
                "build": command, "build_exit_code": compiled.returncode, "exit_code": ran.returncode,
                "stdout": ran.stdout, "stderr": ran.stderr,
                "checks": ["5 fixed vector inputs", "FFT packed DC/Nyquist and selected 9 bins", "all 256 vMag comparisons",
                           "FFT 0.1% and magnitude 1% tolerance boundaries", "wrong scale and side-bin faults rejected",
                           "negative magnitude and invalid exponent rejected", "zero vector exactness", "wrong Hann rejected before request",
                           "HEAR1 intermediate versus final", "pending timeout retains memory", "late event cannot erase timeout",
                           "buffers and old completion flags restored after completion only"]}
    (OUT / "checks.json").write_text(json.dumps(evidence, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(ran.stdout)
    if ran.returncode:
        raise RuntimeError(ran.stderr or "synthetic comparator fixture failed")


if __name__ == "__main__":
    main()
