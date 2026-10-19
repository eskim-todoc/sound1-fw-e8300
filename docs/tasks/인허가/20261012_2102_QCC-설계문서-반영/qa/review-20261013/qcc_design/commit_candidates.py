from pathlib import Path
import json,hashlib
from editor import ROOT,sha

EXPECTED={'SAD':'0385038b27c8ac0ede82653cd99b6ce80b985b93ddf99ba07183c5ce7b5c1eb2','SDDD':'2b47b6fd3b74173a68651b814af3e443bc3383ce5c25c1f8aa8a80d1525a2c2e','SIP':'5b0d29f1b59ac109c9089fda84c06f7cdba40924719920bce0e666424b38bc7c'}
def main():
    qa=json.loads((ROOT/'candidate-qa.json').read_text(encoding='utf-8'));assert qa['status']=='통과'
    reports={}
    # Validate the complete set before the first write.
    for code,expected in EXPECTED.items():
        a=json.loads((ROOT/(code+'-authored.json')).read_text(encoding='utf-8'))
        assert sha(Path(a['candidate']))==expected==a['after_sha256']
        assert sha(Path(a['path']))==a['before_sha256']
        reports[code]=a
    image_dir=Path('E:/workspace/projects/sound1-fw-e8300-backup/docs/참고/인허가/사운드1/DHF/03_설계 출력/SW/펌웨어/이미지 참조')
    for name in ['시스템 블록도.png','소프트웨어 구조도.png']:
        assert sha(image_dir/name)==sha(ROOT/'before'/name)
    for code,a in reports.items():
        Path(a['path']).write_bytes(Path(a['candidate']).read_bytes());assert sha(Path(a['path']))==a['after_sha256']
        a['committed']=True;a['status']='통과';a['scope']='Authorship/static verification only; native final field/render verification and product tests pending separately.'
        (ROOT/(code+'-authored.json')).write_text(json.dumps(a,ensure_ascii=False,indent=2),encoding='utf-8')
    assets=[]
    for name in ['시스템 블록도.png','소프트웨어 구조도.png']:
        p=image_dir/name;before=sha(p);p.write_bytes((ROOT/'figures'/name).read_bytes())
        assets.append({'path':str(p),'before_sha256':before,'after_sha256':sha(p),'snapshot':str(ROOT/'before'/name),'generator':str(ROOT/'diagrams.py')})
    result={'documents':{code:{'path':a['path'],'sha256':a['after_sha256'],'before_sha256':a['before_sha256'],'reject_new_exact_before':a['reject_new_c14n_equals_before']}for code,a in reports.items()},'architecture_assets':assets,'status':'통과','native_final':'미실행','product_verdict':'판정대기'}
    (ROOT/'authored.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(result,ensure_ascii=True))
if __name__=='__main__':main()
