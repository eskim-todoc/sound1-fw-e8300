"""Guarded publication under the existing workbook names, no forced unlock."""
import json
import os
import shutil
from pathlib import Path
from inline_unit_acceptance import QA, sha, save_json
from verify_inline_unit_acceptance import verify

verify()
plan=json.loads((QA/'plan.json').read_text('utf-8'))
layout=json.loads((QA/'layout-checks.json').read_text('utf-8'))
assert all(not x['missingCellTailsOrRtt'] and not x['nativeAutoFitOverflow'] for x in layout)
assert (QA/'visual-check.json').exists(),'Representative preview review must be recorded before publication.'
for b in plan['baseline']['books']:
    assert sha(Path(b['path']))==b['hash'],f"Externally edited: {b['path']}"
results=[]
for b in plan['baseline']['books']:
    candidate=QA/f'{b["key"]}-candidate.xlsx'
    path=Path(b['path']);temporary=path.with_suffix(path.suffix+'.tmp')
    assert not temporary.exists(),f'An unrelated staged file exists: {temporary}'
    shutil.copy2(candidate,temporary)
    try:
        os.replace(temporary,path)
    except OSError:
        # Keep candidate and before file in QA, not duplicate deliverables.
        temporary.unlink()
        save_json('publication.json',{'published':results,'blocked':str(path)})
        raise
    assert sha(path)==sha(candidate)
    results.append({'key':b['key'],'path':str(path),'hash':sha(path)})
save_json('publication.json',{'published':results,'complete':True})
print(json.dumps(results,ensure_ascii=False,indent=2))
