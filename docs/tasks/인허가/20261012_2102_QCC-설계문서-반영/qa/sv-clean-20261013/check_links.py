from pathlib import Path
from urllib.parse import unquote
import hashlib,json,re,sys
import prepare_clean as p

sys.stdout.reconfigure(encoding='utf-8')
h=Path(__file__).resolve().parent;task=h.parents[1];root=task.parents[3]
paths=[root/'AGENTS.md',root/'docs/참고/인허가/사운드1/SW 밸리데이션 문서 작성·검증 지침.md',root/'docs/참고/인허가/사운드1/QCC 자료 추가 위치 관리.md',task/'작업.md',Path('E:/workspace/rules-for-codex/지침/운영/문서 기본 서식.md')]
records=[]
for path in paths:
 fence=False
 for number,line in enumerate(path.read_text(encoding='utf-8-sig').splitlines(),1):
  if re.match(r'^\s*(`{3,}|~{3,})',line):fence=not fence;continue
  if fence:continue
  for match in re.finditer(r'!?\[[^\]]*\]\(([^)]+)\)',line):
   link=match[1].strip().strip('<>')
   if link.startswith('#') or re.match(r'^(?:https?|mailto|codex|app):',link):continue
   target=unquote(link.split('#',1)[0].split('?',1)[0])
   real=Path(target) if re.match(r'^[A-Za-z]:[\\/]',target) else path.parent/target
   records.append({'file':str(path),'line':number,'target':str(real.resolve()),'exists':real.exists()})
bad=[r for r in records if not r['exists']]
out={'files':[{'path':str(f),'sha256':p.sha(f)} for f in paths],'links':records,'failures':bad,'pass':not bad}
p.save(h/'changed-links.json',out)
print(json.dumps({'files':len(paths),'links':len(records),'failures':bad,'pass':not bad},ensure_ascii=False))
sys.exit(bool(bad))
