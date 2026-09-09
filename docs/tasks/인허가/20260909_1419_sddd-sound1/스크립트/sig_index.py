# -*- coding: utf-8 -*-
"""모든 헤더의 함수 서명 색인 - 유닛 파라미터 검증용 (라인 버퍼 방식)"""
import io, os, re, json, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

SRC = u'E:/workspace/projects/sound1-fw-e8300-backup/src'
DECL = re.compile(
    r'^(?P<ret>(?:extern\s+)?(?:static\s+)?(?:const\s+)?'
    r'[A-Za-z_][A-Za-z0-9_ ]*?(?:\s*\*+|\s))\s*'
    r'(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*\((?P<args>[^;{]*)\)\s*;')
SKIP = {'if', 'for', 'while', 'switch', 'return', 'sizeof', 'defined', 'typedef'}
EXCL_DIRS = {'Debug', 'Release', '.settings', '.metadata', 'Gen1_5',
             'OTE_1_5_gen', 'RemoteSystemsTempFiles', 'segger_rtt', 'tiny-AES-c'}


def argc(args):
    a = args.strip()
    if a == '' or a == 'void':
        return 0
    depth = 0
    n = 1
    for ch in a:
        if ch in '([':
            depth += 1
        elif ch in ')]':
            depth -= 1
        elif ch == ',' and depth == 0:
            n += 1
    return n


idx = {}
nfile = 0
for dirpath, dirs, files in os.walk(SRC):
    dirs[:] = [d for d in dirs if d not in EXCL_DIRS]
    for f in sorted(files):
        if not f.endswith('.h'):
            continue
        nfile += 1
        path = os.path.join(dirpath, f)
        try:
            txt = io.open(path, encoding='utf-8', errors='replace').read()
        except Exception:
            continue
        txt = re.sub(r'/\*.*?\*/', '\n', txt, flags=re.S)
        buf = ''
        for raw in txt.split('\n'):
            code = raw.split('//')[0].rstrip()
            if not code.strip():
                buf = ''
                continue
            buf = (buf + ' ' + code.strip()).strip() if buf else code.strip()
            if ';' not in buf:
                if len(buf) > 400:
                    buf = ''
                continue
            m = DECL.match(buf)
            if m and m.group('name') not in SKIP:
                name = m.group('name')
                ret = ' '.join(m.group('ret').replace('extern', '').split())
                args = ' '.join(m.group('args').split())
                if name not in idx:
                    idx[name] = {'ret': ret, 'args': args, 'argc': argc(args),
                                 'file': os.path.relpath(path, SRC).replace('\\', '/')}
            buf = ''

json.dump(idx, io.open('sig_index.json', 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
print(u'헤더 %d개 / 함수 서명 %d건 색인' % (nfile, len(idx)))
for n in ['tdc_audio_mix_1_buffer', 'lib_init_I2S', 'pcmDataOut', 'audio_agc',
          'tdc_audio_mix_2_buffers_for_beamforming', 'app_configure_interrupts',
          'LB_Normal_PowerMode', 'stimulationStrategy_NofM_Peak_Pick']:
    v = idx.get(n)
    print('  %-42s %s' % (n, ('%s (%s) argc=%d' % (v['ret'], v['args'][:46], v['argc'])) if v else 'X 없음'))
