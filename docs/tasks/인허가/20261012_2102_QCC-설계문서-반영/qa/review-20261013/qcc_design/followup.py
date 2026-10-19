from pathlib import Path
from copy import deepcopy
import re,json,zipfile,hashlib,shutil
from lxml import etree as E
import editor
from editor import NS,q,visible,sha,BASE
from content import UNITS,MODULES,test_description

OWN=Path(__file__).parent
OUT=OWN/'followup';OUT.mkdir(exist_ok=True)
SUPPORT={1:[21,22,29,63],2:[35,38,69],4:[48,49],6:[23,28,34],9:[29,35,38,63]}
WHY={1:'전원/상태 처리를 시작하기 위한 초기 설정을 지원한다.',2:'위치/잔량 및 무선 연결 상태의 저장/조회를 지원한다.',4:'페어링/보호 데이터 경로를 사용하기 위한 Bluetooth 활성화를 지원한다. 활성화 상태만으로 페어링/암호화 완료를 판정하지 않는다.',6:'배터리 잔량/전원 상태를 제공하여 LED/자극 알림의 입력을 지원한다. 알림/자극의 최종 실행은 E8300 시스템 경로에서 별도로 확인한다.',9:'현재/peer의 위치/잔량 및 연결 상태를 광고/조회 데이터에 반영하는 입력을 지원한다.'}
for n,rids in SUPPORT.items():UNITS[n-1]['rids']=sorted(set(UNITS[n-1]['rids']+rids))
DOTX=Path('E:/workspace/rules-for-codex/지침/양식/Word/TDC 공통 스타일.dotx')

def style_repair(e):
    original=e.parts['word/styles.xml'];sr=E.fromstring(original)
    with zipfile.ZipFile(DOTX)as z:tr=E.fromstring(z.read('word/styles.xml'))
    tids={s.get(q('styleId')):s for s in tr.findall('w:style',NS)}
    tdc={k:s for k,s in tids.items()if s.find('w:name',NS)is not None and s.find('w:name',NS).get(q('val')).startswith('TDC')}
    current={s.find('w:name',NS).get(q('val')):s for s in sr.findall('w:style',NS)if s.find('w:name',NS)is not None}
    mapping={k:current[s.find('w:name',NS).get(q('val'))].get(q('styleId'))for k,s in tdc.items()}
    needed=set();pending=list(tdc)
    while pending:
        key=pending.pop()
        for ref in tids[key].xpath('w:basedOn/@w:val|w:link/@w:val|w:next/@w:val',namespaces=NS):
            if ref not in tdc and ref not in needed:
                assert ref in tids;needed.add(ref);pending.append(ref)
    for key in sorted(needed):mapping[key]='TDCSupport20261013'+key
    copied=[]
    for key in sorted(needed):
        s=deepcopy(tids[key]);s.set(q('styleId'),mapping[key]);s.attrib.pop(q('default'),None)
        old=s.find('w:name',NS).get(q('val'));s.find('w:name',NS).set(q('val'),'TDC 지원 '+old)
        for v in s.xpath('w:basedOn|w:next|w:link',namespaces=NS):v.set(q('val'),mapping.get(v.get(q('val')),v.get(q('val'))))
        assert not sr.xpath('w:style[@w:styleId="'+mapping[key]+'"]',namespaces=NS)
        sr.append(s);copied.append({'template_id':key,'new_id':mapping[key],'name':old})
    bindings=[]
    for key,tstyle in tdc.items():
        name=tstyle.find('w:name',NS).get(q('val'));old=current[name];new=deepcopy(tstyle);new.set(q('styleId'),old.get(q('styleId')))
        for v in new.xpath('w:basedOn|w:next|w:link',namespaces=NS):v.set(q('val'),mapping.get(v.get(q('val')),v.get(q('val'))))
        # Numbering IDs are document-local. Preserve its existing heading list binding.
        on=old.find('w:pPr/w:numPr/w:numId',NS);nn=new.find('w:pPr/w:numPr/w:numId',NS)
        if nn is not None:
            assert on is not None;bindings.append({'name':name,'template_numId':nn.get(q('val')),'saved_numId':on.get(q('val'))});nn.set(q('val'),on.get(q('val')))
        at=sr.index(old);sr.remove(old);sr.insert(at,new)
    names={s.get(q('styleId')):s.find('w:name',NS).get(q('val'))for s in sr.findall('w:style',NS)if s.find('w:name',NS)is not None}
    for s in sr.findall('w:style',NS):
        if not names.get(s.get(q('styleId')),'').startswith('TDC'):continue
        for node in s.xpath('w:basedOn|w:next|w:link',namespaces=NS):assert node.get(q('val'))in names
        b=s.find('w:basedOn',NS)
        if b is not None:
            parent=sr.xpath('w:style[@w:styleId="'+b.get(q('val'))+'"]',namespaces=NS)[0]
            assert parent.get(q('type'))==s.get(q('type')),(names[s.get(q('styleId'))],names[parent.get(q('styleId'))])
    # Non-TDC styles, document defaults and numbering are byte/semantic preserved.
    before=E.fromstring(original)
    for s in before.findall('w:style',NS):
        name=s.find('w:name',NS).get(q('val'))
        if name.startswith('TDC'):continue
        match=sr.xpath('w:style[@w:styleId="'+s.get(q('styleId'))+'"]',namespaces=NS)[0]
        assert E.tostring(s,method='c14n')==E.tostring(match,method='c14n')
    e.parts['word/styles.xml']=E.tostring(sr,encoding='UTF-8',xml_declaration=True,standalone=True)
    # finish() currently asserts all other parts unchanged; update its local baseline
    # only for the intentionally repaired styles, while recording both exact bytes.
    (OUT/(e.code+'-styles-before.xml')).write_bytes(original)
    (OUT/(e.code+'-styles-after.xml')).write_bytes(e.parts['word/styles.xml'])
    e.entries=[(i,e.parts['word/styles.xml']if i.filename=='word/styles.xml'else b)for i,b in e.entries]
    return {'template_sha256':sha(DOTX),'saved_tdc_styles':len(tdc),'support_styles':copied,'numbering_bindings_preserved':bindings,'before_styles_sha256':hashlib.sha256(original).hexdigest(),'after_styles_sha256':hashlib.sha256(e.parts['word/styles.xml']).hexdigest(),'non_tdc_styles_and_document_defaults_preserved':True,'limit':'Styles definitions are saved Word format; prose/drawing revisions remain separately accept/reject capable. Current baseline style XML is snapshotted.'}

def patch(code):
    p=next(BASE.glob('[[]E8300-QCC[]] '+code+'*.docx'));before=OUT/'before'/p.name;before.parent.mkdir(exist_ok=True)
    if not before.exists():shutil.copy2(p,before)
    else:assert sha(p)==sha(before),'Concurrent post-baseline change'
    editor.ROOT=OUT;e=editor.Editor(code);changes=[]
    if code=='SAD':
        for tbl in e.body.findall('w:tbl',NS):
            for row in tbl.findall('w:tr',NS):
                if row.find('w:trPr/w:del',NS)is not None:continue
                cells=row.findall('w:tc',NS)
                if len(cells)!=3:continue
                first=visible(cells[0]).strip();m=re.fullmatch(r'SW-FW-RID-(\d+)',first)
                if m:
                    rid=int(m.group(1));applicable=[UNITS[n-1]for n,extra in SUPPORT.items()if rid in extra]
                    for u in applicable:
                        mod=next(m for m in MODULES if m['id']==u['module_id'])
                        for ci,entry in [(1,mod['id']+' '+mod['title']),(2,u['id']+' '+u['title'])]:
                            current=visible(cells[ci]);id0=entry.split(' ',1)[0]
                            if id0 not in current:
                                head,sep,tail=current.partition('\n[')
                                value=head+'\n'+entry+(sep+tail if sep else'')
                                e.cell(cells[ci],value);changes.append({'requirement':first,'column':ci,'added':entry})
                um=re.match(r'SU-ID-QCC-(\d+)',first)
                if um and int(um.group(1))in SUPPORT:
                    u=UNITS[int(um.group(1))-1];e.cell(cells[2],', '.join('SW-FW-RID-'+str(v)for v in u['rids']))
                    changes.append({'reverse_unit':u['id'],'requirements':u['rids']})
    if code=='SDDD':
        unit=None
        for b in e.body:
            if b.tag==q('p'):
                m=re.fullmatch(r'SU-ID-QCC-(\d+) .*',visible(b))
                if m:unit=int(m.group(1))
            if b.tag!=q('tbl')or unit not in SUPPORT:continue
            for tr in b.findall('w:tr',NS):
                if tr.find('w:trPr/w:del',NS)is not None:continue
                cells=tr.findall('w:tc',NS)
                if len(cells)==2 and visible(cells[0])=='요구사항/검증 연결':
                    u=UNITS[unit-1];value='상위 요구사항: '+', '.join('SW-FW-RID-'+str(n)for n in u['rids'])+'. '+test_description(u)+' '+WHY[unit]
                    e.cell(cells[1],value.replace('·','/'));changes.append({'unit':u['id'],'requirements':u['rids'],'support_scope':WHY[unit]})
    # The user uses / or 또는 in newly written explanations. Normalize only
    # this authorship's visible paragraphs, with real deletion/insertion history.
    prior=json.loads((OWN/(code+'-authored.json')).read_text(encoding='utf-8'));ourprior=set(prior['new_revision_ids'])
    normalized=[]
    for p0 in e.body.xpath('.//w:p',namespaces=NS):
        if p0.xpath('ancestor::w:tr[w:trPr/w:del]',namespaces=NS):continue
        text=visible(p0)
        relevant=any(t.text and '·'in t.text and any(v.tag==q('ins')and v.get(q('id'))in ourprior for v in t.iterancestors())for t in p0.xpath('.//w:t[not(ancestor::w:del)]',namespaces=NS))
        if not relevant or p0.xpath('.//w:fldChar|.//w:drawing',namespaces=NS):continue
        e.delete_runs(p0);ins=e.rev('ins',p0)
        for piece in re.split(r'(?=\[(?:QCC|E8300))',text.replace('·','/')):
            if piece:e.run(ins,piece,missing=piece.startswith(('[QCC','[E8300')))
        normalized.append(text)
    style=style_repair(e);e.changes=changes
    r=e.finish(commit=True);r['style_repair']=style;r['separator_normalization_count']=len(normalized)
    (OUT/(code+'-authored.json')).write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf-8')
    return {'code':code,'sha256':r['after_sha256'],'new_revision_count':r['new_revision_count'],'trace_change_count':len(changes),'style_support_count':len(style['support_styles']),'reject_new_text_exact_before':r['reject_new_c14n_equals_before'],'separator_normalization_count':len(normalized)}

if __name__=='__main__':
    print(json.dumps([patch(c)for c in ['SAD','SDDD','SIP']],ensure_ascii=True))
