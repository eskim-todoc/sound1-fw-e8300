import json,subprocess,sys
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
HERE=Path(__file__).resolve().parent;TASK=HERE.parent.parent
plans=json.loads((HERE/'native-final-v2-plans.json').read_text(encoding='utf-8'));pdfs=[Path(p)for plan in plans for p in plan['pdfs']]
def one(pdf):
 out=pdf.with_suffix('')
 r=subprocess.run([sys.executable,str(TASK/'render_pdf.py'),str(pdf),'--out',str(out),'--scale','2','--contact'],capture_output=True,text=True,encoding='utf-8')
 if r.returncode:raise RuntimeError(r.stderr)
 summary=json.loads(r.stdout);summary['pdf']=str(pdf);return summary
with ThreadPoolExecutor(max_workers=3)as pool:results=list(pool.map(one,pdfs))
(TASK/'evidence/qcc_tests_native_v2.json').write_text(json.dumps({'results':results},ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps([{'pdf':Path(r['pdf']).name,'pages':r['pages']}for r in results],ensure_ascii=True))
