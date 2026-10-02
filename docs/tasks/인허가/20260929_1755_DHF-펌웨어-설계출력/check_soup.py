"""Identify reviewed source files and run host KATs without firmware changes."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess

TASK = Path(__file__).resolve().parent
ROOT = TASK.parents[3]
SDK = Path('C:/Program Files (x86)/ON Semiconductor/Ezairo 8300 SDK')
QA = TASK/'qa/soup'
FILES = [
    'src/0__bootloader/tiny-AES-c/aes.c','src/0__bootloader/tiny-AES-c/aes.h',
    'src/2__cm3/Gen1_5/tiny-AES-c/aes.c','src/2__cm3/Gen1_5/tiny-AES-c/aes.h',
    'src/0__bootloader/source/bootloader.c','src/0__bootloader/source/bootloader_crc.c',
    'src/0__bootloader/source/bootmgr/ci_boot.c','src/2__cm3/Gen1_5/crc/ci_crc.c',
    'src/2__cm3/Gen1_5/FS/ci_filesystem.c','src/2__cm3/Gen1_5/FS/ci_map.c',
    'src/2__cm3/Cortex-M3-src/BleCommunication/ci_ble_control_ota.c',
    'src/2__cm3/Gen1_5/ui/tdc_ui_command.c','src/2__cm3/Cortex-M3-src/main.c',
    'src/0__bootloader/source/segger_rtt/SEGGER_RTT.c',
    'src/0__bootloader/include/segger_rtt/SEGGER_RTT_Conf.h',
    'src/2__cm3/Gen1_5/SEGGER_RTT/SEGGER_RTT.c',
    'src/2__cm3/Gen1_5/SEGGER_RTT/SEGGER_RTT_Conf.h',
    'src/2__cm3/Gen1_5/SEGGER_RTT/SEGGER_RTT_Wrapper.c',
    'src/0__bootloader/.cproject','src/2__cm3/.cproject',
    'src/0__bootloader/Debug/0__bootloader.map','src/2__cm3/Debug/2__cm3.map']
SDK_FILES = ['REVISION','source/Cortex-M3/fatfs/code/ff.c',
    'source/Cortex-M3/fatfs/include/ffconf.h','include/cm3/ff.h','include/cm3/ffconf.h',
    'include/shared/crc.h','include/shared/version.h','lib/cm3/libfatfs.a']

def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    QA.mkdir(parents=True, exist_ok=True)
    paths=[ROOT/f for f in FILES]+[SDK/f for f in SDK_FILES]
    hashes={str(p):digest(p) for p in paths}
    gcc=shutil.which('gcc')
    assert gcc, 'Host GCC required for these KATs'
    result={'compiler':subprocess.check_output([gcc,'--version'],text=True).splitlines()[0],
            'source_sha256':hashes,'tests':[],
            'scope':'Host AES core only. No target, filesystem, CRC peripheral, RTT or product safety test.'}
    for label,rel in [('BOOT','src/0__bootloader/tiny-AES-c'),('CM3','src/2__cm3/Gen1_5/tiny-AES-c')]:
        source=ROOT/rel
        executable=QA/f'aes-{label}.exe'
        cmd=[gcc,'-std=c99','-O2','-Wall','-Wextra','-I',str(source),str(source/'aes.c'),str(TASK/'soup_aes_kat.c'),'-o',executable.name]
        build=subprocess.run(cmd,cwd=QA,capture_output=True,text=True)
        if build.returncode:
            print(build.stdout+build.stderr)
            build.check_returncode()
        run=subprocess.run([str(executable)],capture_output=True,text=True,check=True)
        result['tests'].append({'component':label,'command':cmd,'build_output':build.stdout+build.stderr,'exit':run.returncode,'output':run.stdout.strip()})
    assert all(digest(p)==hashes[str(p)] for p in paths)
    result['inputs_unchanged']=True
    (TASK/'SOUP 확인 근거.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps({'tests':result['tests'],'inputs_unchanged':True},ensure_ascii=False))

if __name__=='__main__': main()
