"""Build the actual two printf bodies and check stored length/reentrant buffer."""
import json
import subprocess
from pathlib import Path
from check_cfx_vectors import function

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "tmp/unit-tests/rtt-wrappers"
PREFIX = r'''
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define SEGGER_RTT_PRINTF_BUFFER_SIZE 256
static char capture[512];
static unsigned calls, length;
static int nested, in_nested;
void SEGGER_RTT_Wrapper_printf(const char *,...);
static int host_vsnprintf(char *buf,size_t size,const char *fmt,va_list ap) {
 if (!strcmp(fmt,"FORMAT_ERROR")) return -1;
 return vsnprintf(buf,size,fmt,ap);
}
static unsigned SEGGER_RTT_Write(unsigned ch,const void *data,unsigned n) {
 assert(ch==0 && n<sizeof(capture));
 if (nested && !in_nested) {in_nested=1; SEGGER_RTT_Wrapper_printf("nested"); in_nested=0;}
 calls++; length=n; memcpy(capture,data,n); capture[n]=0; return n;
}
'''
MAIN = r'''
int main(void) {
 char long_text[400]; memset(long_text,'X',399); long_text[399]=0;
 rtt_printf("%s",long_text); assert(length==63 && strlen(capture)==63);
 unsigned saved=calls; rtt_printf("FORMAT_ERROR"); assert(calls==saved);
 rtt_printf(""); assert(calls==saved);
 SEGGER_RTT_Wrapper_printf("%s",long_text); assert(length==255 && strlen(capture)==255);
 saved=calls; SEGGER_RTT_Wrapper_printf("FORMAT_ERROR"); assert(calls==saved);
 nested=1; SEGGER_RTT_Wrapper_printf("foreground");
 assert(!strcmp(capture,"foreground"));
 puts("RTT_WRAPPERS HOST_PASS: truncation, negative, empty, nested buffer");
 return 0;
}
'''


def main():
    paths = [("src/0__bootloader/source/segger_rtt/rtt_printf.c", "rtt_printf"),
             ("src/2__cm3/Gen1_5/SEGGER_RTT/SEGGER_RTT_Wrapper.c", "SEGGER_RTT_Wrapper_printf")]
    bodies = [function((ROOT / p).read_text(encoding="utf-8-sig"), n).replace("vsnprintf(", "host_vsnprintf(") for p, n in paths]
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "wrappers.c"
    source.write_text(PREFIX + "\n".join(bodies) + MAIN, encoding="utf-8")
    exe = OUT / "wrappers.exe"
    subprocess.run(["gcc", "-std=c99", "-Wall", "-Wextra", "-Werror", str(source), "-o", str(exe)], check=True)
    run = subprocess.run([str(exe)], capture_output=True, text=True)
    (OUT / "checks.json").write_text(json.dumps({"returncode": run.returncode, "stdout": run.stdout}, indent=2), encoding="utf-8")
    print(run.stdout)
    if run.returncode:
        raise RuntimeError(run.stderr or "wrapper assertions failed")


if __name__ == "__main__":
    main()
