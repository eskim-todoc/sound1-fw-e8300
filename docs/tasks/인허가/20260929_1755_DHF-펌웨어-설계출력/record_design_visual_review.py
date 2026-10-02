"""Record page-image review evidence, including exactly identical prior pages."""
from datetime import datetime,timezone
from hashlib import sha256
from pathlib import Path
import json
import argparse

T=Path(__file__).resolve().parent;Q=T/'qa/design-readability/design-intent-and-placeholders'
def read(n):return json.loads((Q/n).read_text(encoding='utf8'))
def hashes(count):return {str(n):sha256((Q/'pages'/f'page-{n:03}.png').read_bytes()).hexdigest() for n in range(1,count+1)}
def main():
    p=argparse.ArgumentParser();p.add_argument('action',choices=['snapshot','finish']);p.add_argument('--reviewed-pages',type=int,nargs='*',default=[]);args=p.parse_args()
    c=read('checks.json');pagehash=hashes(c['pages'])
    if args.action=='snapshot':
        comp=read('page-comparison.json')
        assert sorted(comp['pixel_identical_pages']+comp['pages_requiring_review'])==list(range(1,c['pages']+1))
        result={'candidate_sha256':c['candidate_sha256'],'page_hashes':pagehash,
                'inherited_identical_pages':comp['pixel_identical_pages'],
                'directly_reviewed_pages':comp['pages_requiring_review'],
                'directly_reviewed_boards':list(range(1,15)),'unresolved_layout_defects':[]}
        out='visual-review-pass-1.json'
    else:
        base=read('visual-review-pass-1.json')
        same=[int(n) for n,h in pagehash.items() if base['page_hashes'].get(n)==h]
        changed=[int(n) for n,h in pagehash.items() if base['page_hashes'].get(n)!=h]
        assert sorted(args.reviewed_pages)==sorted(changed),(changed,args.reviewed_pages)
        result={'checked_at':datetime.now(timezone.utc).isoformat(),'candidate_sha256':c['candidate_sha256'],
                'pages_reviewed':list(range(1,c['pages']+1)),
                'method':'Word final view, original-size 96dpi page images; unchanged pages matched to recorded prior visual review',
                'identical_to_first_review_pages':same,'renewed_visual_review_pages':changed,
                'prior_review':str(Q/'visual-review-pass-1.json'),'unresolved_layout_defects':[]}
        out='visual-check.json'
    (Q/out).write_text(json.dumps(result,indent=2),encoding='utf8')
    print(json.dumps({'record':out,'pages':c['pages'],'renewed':result.get('renewed_visual_review_pages',[]) }))
if __name__=='__main__':main()
