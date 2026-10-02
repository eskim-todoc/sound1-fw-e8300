"""Explicitly record the files examined for the architecture analysis."""
from pathlib import Path
import hashlib
import json
import subprocess

ROOT = Path(__file__).resolve().parents[4]
FILES = '''src/0__bootloader/.cproject
src/0__bootloader/main.c
src/0__bootloader/source/bootloader.c
src/0__bootloader/source/bootmgr/ci_boot.c
src/1__cfx/.cproject
src/1__cfx/systemControl/main.c
src/1__cfx/systemControl/interrupt_service_routine.c
src/1__cfx/systemControl/system_control.c
src/1__cfx/systemControl/audioMixer.c
src/1__cfx/systemControl/audioMixer.h
src/1__cfx/signalProcessing/agc.c
src/1__cfx/signalProcessing/stimulationStrategy.c
src/1__cfx/signalProcessing/definitionsForAlgorithm.h
src/1__cfx/internalDevice/driver_PCM.c
src/1__cfx/lib_cfx/lib_audio_in.h
src/1__cfx/hear/microcode.h
src/2__cm3/.cproject
src/2__cm3/Cortex-M3-src/main.c
src/2__cm3/Cortex-M3-src/systemControl/initialize.c
src/2__cm3/Cortex-M3-src/systemControl/driver_timmer.c
src/2__cm3/Cortex-M3-src/systemControl/driver_SPI.h
src/2__cm3/Cortex-M3-src/systemControl/cfx_cm3_sharedMemory.c
src/2__cm3/Cortex-M3-src/signalProcessing/stimulationParaCal.c
src/2__cm3/Cortex-M3-src/internalDevice/isd_interface.c
src/2__cm3/Cortex-M3-src/internalDevice/isd_interface_stimulationStandAlone.c
src/2__cm3/Cortex-M3-src/internalDevice/isd_interface_stimulationParaSetting.c
src/2__cm3/Cortex-M3-src/BleCommunication/ble_communication.c
src/2__cm3/Cortex-M3-src/BleCommunication/ci_ble_control_ota.c
src/2__cm3/Cortex-M3-src/QCC/snd_qcc.c
src/2__cm3/Gen1_5/common/ci_timer.c
src/3__hear/microcode.hct'''.splitlines()

if __name__ == '__main__':
    data = {'review_date':'2026-09-29',
            'scope':'Selected entry points, cross-core paths and active branches; not full-file correctness review.',
            'git_head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
            'basis':'Working tree bytes including existing local modifications',
            'files':[{'path':p,'sha256':hashlib.sha256((ROOT/p).read_bytes()).hexdigest()} for p in FILES]}
    dest = ROOT/'docs/참고/시스템/분석 기준 파일.json'
    dest.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(len(FILES), 'source files recorded')
