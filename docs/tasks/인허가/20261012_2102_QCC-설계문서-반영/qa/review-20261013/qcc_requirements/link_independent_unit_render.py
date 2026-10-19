from pathlib import Path
from PIL import Image
import json,hashlib,datetime
H=Path(__file__).resolve().parent;Q=H.parent
file=H/'independent-unit-native-qa.json'
a=json.loads(file.read_text(encoding='utf-8'))
native=json.loads((Q/'native-excel/tests-native.json').read_text(encoding='utf-8'))
specs={s['code']:s for s in native}
proof=[]
for page in a['pages']:
    p=Path(page['png'])
    with Image.open(p) as im: current=hashlib.sha256(im.convert('RGB').tobytes()).hexdigest().upper()
    proof.append({'code':page['code'],'page':page['page'],'reviewed_pixelSHA256':page['pixelSHA256'],'current_pixelSHA256':current,'equal':current==page['pixelSHA256'],'current_pngSHA256':hashlib.sha256(p.read_bytes()).hexdigest().upper(),'mtime':datetime.datetime.fromtimestamp(p.stat().st_mtime).astimezone().isoformat()})
sources=[]
for old in a['sources']:
    spec=specs[old['code']]
    sha=hashlib.sha256(Path(spec['path']).read_bytes()).hexdigest().upper()
    pages=json.loads((Q/'native-excel/tests'/old['code']/'pages.json').read_text(encoding='utf-8'))
    sources.append({'code':old['code'],'currentSHA256':sha,'nativeSHA256':spec['sourceSHA256'],'same_current_native':sha==spec['sourceSHA256'],'same_inspected_XML':sha==old['read_only_after'],'pages_count':len(pages),'nativeCheckedAt':spec['checkedAt']})
link={'checkedAt':datetime.datetime.now().astimezone().isoformat(),'all44pixels_equal':all(p['equal'] for p in proof),'sources':sources,'pages':proof}
assert link['all44pixels_equal'] and all(s['same_current_native'] and s['same_inspected_XML'] for s in sources)
a['final_render_link']=link
a['native_current_link']='root 09:36 재렌더의 QCC 꼬리 구간34쪽과 대표10쪽이 직접 검토한44 PNG와 픽셀 동일. actual current G/source/native hash 동등 확인.'
a['status']='통과: 지정한 QCC38조건 표시/현재 동일조건/미실행 판정 및 RTT/정량10페이지 표본. 전체 유닛 문서461쪽 검사로 확대하지 않는다. 제품 QCC시험은 미실행/판정대기.'
file.write_text(json.dumps(a,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'all44pixels_equal':link['all44pixels_equal'],'sources':sources},ensure_ascii=False))
