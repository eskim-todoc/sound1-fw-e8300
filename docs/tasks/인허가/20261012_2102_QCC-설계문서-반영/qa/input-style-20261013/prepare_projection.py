from build_inputs import *
def strip_past(p,keep):
 p=deepcopy(p)
 for e in list(p.xpath('.//w:pPrChange|.//w:rPrChange',namespaces=N)):e.getparent().remove(e)
 for e in list(p.xpath('.//w:del|.//w:moveFrom',namespaces=N)):
  if e.get(qn('id')) not in keep:e.getparent().remove(e)
 for e in list(p.xpath('.//w:ins',namespaces=N))[::-1]:
  if e.get(qn('id')) not in keep:
   par=e.getparent();at=par.index(e)
   for ch in list(e):e.remove(ch);par.insert(at,ch);at+=1
   par.remove(e)
 return p
plans=json.loads((Q/'authored.json').read_text(encoding='utf-8'))['docs'];proof=[]
for plan in plans:
 parts=readzip(plan['candidate']);tree=E.fromstring(parts['word/document.xml']);pars=tree.xpath('//w:body//w:p',namespaces=N)
 selected=[strip_past(pars[c['index']],set(plan['new_text_revision_ids'])) for c in plan['note_changes']]
 body=tree.find(qn('body'));section=deepcopy(body.find(qn('sectPr')))
 for ch in list(body):body.remove(ch)
 for p in selected:body.append(p)
 if section is not None:body.append(section)
 parts['word/document.xml']=xml(tree);out=Q/(plan['code']+'-new-text-only.docx');writezip(out,parts)
 proof.append({'code':plan['code'],'path':str(out),'sha256':sha(out),'expected_accept':'\n'.join(c['after'] for c in plan['note_changes']),'expected_reject':'\n'.join(c['before'] for c in plan['note_changes']),'revision_count':len(plan['new_text_revision_ids']),'scope':'Only new text revisions of the six notice paragraphs; older history accepted in QA projection only. Source and final histories retained.'})
(Q/'projection-plan.json').write_text(json.dumps(proof,ensure_ascii=False,indent=2),encoding='utf-8')
print('New text revision QA projections prepared')
