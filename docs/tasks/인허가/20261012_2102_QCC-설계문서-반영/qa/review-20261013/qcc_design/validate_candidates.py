from pathlib import Path
from copy import deepcopy
import json,re,hashlib,zipfile
from lxml import etree as E
from editor import ROOT,BASE,NS,q,visible,sha
from content import UNITS,MODULES

DOTX=Path('E:/workspace/rules-for-codex/지침/양식/Word/TDC 공통 스타일.dotx')
def styles(path):
    with zipfile.ZipFile(path)as z:r=E.fromstring(z.read('word/styles.xml'))
    ids={e.get(q('styleId')):e for e in r.findall('w:style',NS)}
    names={e.find('w:name',NS).get(q('val')):k for k,e in ids.items()if e.find('w:name',NS)is not None}
    default={}
    for path0 in ['w:docDefaults/w:rPrDefault/w:rPr','w:docDefaults/w:pPrDefault/w:pPr']:
        el=r.find(path0,NS)
        if el is not None:default.update(flatten(el))
    return ids,names,default
def flatten(el,prefix=''):
    d={};name=E.QName(el).localname;here=prefix+'/'+name
    # Schema flags without a value have the same meaning as val=1.
    attrs={E.QName(k).localname:v for k,v in el.attrib.items()}
    if not len(el)and not attrs:attrs={'val':'1'}
    for k,v in attrs.items():d[here+'/@'+k]=v
    for child in el:
        if E.QName(child).localname.endswith('Change'):continue
        key=here
        if child.tag==q('tblStylePr'):key+='['+child.get(q('type'),'')+']'
        d.update(flatten(child,key))
    return d
def effective(ids,key,default,seen=None):
    seen=set()if seen is None else seen
    if key in seen:raise ValueError('style inheritance cycle')
    seen.add(key);el=ids[key];base=el.find('w:basedOn',NS)
    d=effective(ids,base.get(q('val')),default,seen)if base is not None and base.get(q('val'))in ids else dict(default)
    for prop in ['pPr','rPr','tblPr','trPr','tcPr','tblStylePr']:
        for v in el.findall('w:'+prop,NS):d.update(flatten(v))
    return d
def boolnorm(k,v):
    if k.endswith('/@val')and v in ['true','on']:return '1'
    if k.endswith('/@val')and v in ['false','off']:return '0'
    return v
def accept(path,ownids):
    with zipfile.ZipFile(path)as z:r=E.fromstring(z.read('word/document.xml'))
    for p in list(r.xpath('.//w:p',namespaces=NS)):
        if any(v.get(q('id'))in ownids for v in p.xpath('w:pPr/w:rPr/w:del',namespaces=NS)):p.getparent().remove(p)
    for row in list(r.xpath('.//w:tr[w:trPr/w:del]',namespaces=NS)):row.getparent().remove(row)
    for d in list(r.xpath('.//w:del',namespaces=NS)):
        # Keep historical deleted paragraph marks as metadata. Native Word merges
        # their paragraphs; treating their old blank/font as current is wrong.
        if d.getparent()is not None and d.getparent().tag!=q('rPr'):d.getparent().remove(d)
    for table in list(r.xpath('.//w:tbl',namespaces=NS)):
        if not table.findall('w:tr',NS):table.getparent().remove(table)
    return r
def run():
    ti,tn,td=styles(DOTX);report={'dotx':str(DOTX),'dotx_sha256':sha(DOTX),'documents':{},'status':'미실행'}
    audit=json.loads((ROOT/'input-audit.json').read_text(encoding='utf-8'))
    protected=[{'path':d['path'],'before':d['sha256'],'actual':sha(Path(d['path']))}for d in audit['documents']if d['role']in ['E','Q']]
    assert all(d['before']==d['actual']for d in protected)
    report['protected_sources']=protected
    for code in ['SAD','SDDD','SIP']:
        a=json.loads((ROOT/(code+'-authored.json')).read_text(encoding='utf-8'));path=Path(a['candidate']);own=set(a['new_revision_ids'])
        r=accept(path,own);body=r.find('w:body',NS);text=visible(body)
        ids,names,default=styles(path);style_results=[]
        for name,key in tn.items():
            if not name.startswith('TDC'):continue
            assert name in names
            expected=effective(ti,key,td);actual=effective(ids,names[name],default)
            dif=[]
            for k in sorted(set(expected)|set(actual)):
                av=boolnorm(k,actual.get(k));ev=boolnorm(k,expected.get(k))
                if av!=ev:dif.append({'property':k,'template':ev,'document':av})
            style_results.append({'name':name,'saved_id':names[name],'template_id':key,'effective_differences':dif})
        report['documents'][code]={'path':str(path),'sha256':sha(path),'baseline_current_sha256':sha(Path(a['path'])),'new_revision_count':a['new_revision_count'],'reject_new_exact_before':a['reject_new_c14n_equals_before'],'new_nested_insertions':a['new_nested_insertions'],'tdc_style_count':len(style_results),'style_results':style_results}
        doc=report['documents'][code]
        assert len(style_results)==59
        banned=['제공된 QCC 자료에 따르면','입력받은 자료를 기반으로','원문 추적','병합 중','[QCC 단독]','[E8300/QCC 각각 확인]','소속 유닛','SW-FW-RID-82']
        findings=[x for x in banned if x in text];assert not findings,(code,findings)
        doc['active_prose_forbidden_terms']=findings
        doc['known_paths']={x:x in text for x in ['DMIC2','통화 상향','I2S']};assert all(doc['known_paths'].values())
        notices=[]
        for bi,b in enumerate(body,1):
            for p in b.xpath('.//w:p',namespaces=NS)if b.tag==q('tbl')else[b]if b.tag==q('p')else[]:
                t=visible(p)
                for run0 in p.xpath('.//w:r[w:t and not(ancestor::w:del)]',namespaces=NS):
                    rt=visible(run0)
                    if not rt.startswith(('[QCC 자료 입력]','[QCC/E8300 자료 입력]','[E8300 보완]')):continue
                    color='7030A0'if rt.startswith('[QCC/E8300')else'0070C0'if rt.startswith('[E8300')else'C00000'
                    rp=run0.find('w:rPr',NS)
                    values={k:rp.find('w:'+k,NS).get(q('val'))if rp is not None and rp.find('w:'+k,NS)is not None else None for k in ['color','sz','u','b']}
                    ok=values['color']==color and values['sz']=='24'and values['u']=='single'and values['b']in['0',None]
                    notices.append({'body_block':bi,'text':rt,'direct_format':values,'pass':ok})
        assert all(v['pass']for v in notices),(code,[v for v in notices if not v['pass']])
        doc['missing_notices']=notices
        grids=[]
        for bi,b in enumerate(body,1):
            if b.tag!=q('tbl'):continue
            trs=b.findall('w:tr',NS)
            newtable=bool(trs)and all(any(v.get(q('id'))in own for v in tr.xpath('w:trPr/w:ins',namespaces=NS))for tr in trs)
            if not newtable:continue
            grid=[int(c.get(q('w')))for c in b.find('w:tblGrid',NS)]
            nxt=body[bi]if bi<len(body)else None
            blank=nxt is not None and nxt.tag==q('p')and not visible(nxt).strip()
            assert blank,(code,bi,'new table missing normal trailing blank')
            st=nxt.find('w:pPr/w:pStyle',NS).get(q('val'))
            assert st in [names['TDC 본문 '+str(n)]for n in range(1,7)]
            assert b.find('w:tr/w:trPr/w:tblHeader',NS)is not None
            grids.append({'body_block':bi,'grid':grid,'sum':sum(grid),'rows':len(trs),'trailing_blank_style':st,'repeating_header':True,'fixed_row_heights':b.xpath('.//w:trHeight[@w:hRule="exact"]',namespaces=NS)!=[],'first_row_text':[visible(c)for c in trs[0].findall('w:tc',NS)]})
        doc['new_tables']=grids
        assert not any(v['fixed_row_heights']for v in grids)
        if code=='SAD':
            tracing=[]
            for b in body.findall('w:tbl',NS):
                for tr in b.findall('w:tr',NS):
                    cells=tr.findall('w:tc',NS)
                    if len(cells)==3 and re.fullmatch(r'SW-FW-RID-\d+',visible(cells[0]).strip()):tracing.append({'id':visible(cells[0]).strip(),'module':visible(cells[1]),'unit':visible(cells[2])})
            assert set(v['id']for v in tracing)=={'SW-FW-RID-'+str(n)for n in list(range(1,82))+[83]}
            assert len(tracing)==82
            doc['requirement_trace']=tracing
        if code=='SDDD':
            qt=[v for v in grids if v['grid']==[1304,7512]];assert len(qt)==28
            unitchecks=[]
            for u in UNITS:
                heads=[(bi,b)for bi,b in enumerate(body,1)if b.tag==q('p')and visible(b)==u['id']+' '+u['title']]
                assert len(heads)==1,(u['id'],len(heads))
                bi,b=heads[0];purpose=body[bi];assert visible(purpose)==u['purpose']
                assert purpose.find('w:pPr/w:keepNext',NS)is not None
                unitchecks.append({'id':u['id'],'heading_block':bi,'purpose_block':bi+1,'purpose':visible(purpose),'test_ids':u['tests'],'rids':u['rids'],'module':u['module_id']})
            doc['unitchecks']=unitchecks
            small=[];historical_small=[]
            for bi,b in enumerate(body,1):
                if b.tag==q('p')and not visible(b).strip():
                    for sz in b.xpath('w:pPr/w:rPr/w:sz/@w:val',namespaces=NS):
                        if int(sz)<20:
                            value={'body_block':bi,'font_halfpoint':sz,'paraId':b.get('{http://schemas.microsoft.com/office/word/2010/wordml}paraId')}
                            if b.find('w:pPr/w:rPr/w:del',NS)is not None:historical_small.append(value)
                            else:small.append(value)
            assert not small,small
            doc['small_active_blank_paragraphs']=small
            doc['historical_deleted_paragraph_marks_with_small_format']=historical_small
        if code=='SIP':
            doc['integration_ids']={str(n):bool(re.search(r'(?<!\d)'+str(n).zfill(3)+r'(?!\d)',text))for n in range(18,25)}
            assert all(doc['integration_ids'].values())
        doc['structural_status']='통과';doc['native_render_status']='미실행'
    report['status']='통과';report['limit']='Static package/content checks only; final native fields and all-page QA pending at root. Product tests remain pending.'
    (ROOT/'candidate-qa.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({k:{'sha256':d['sha256'],'tables':len(d['new_tables']),'notices':len(d['missing_notices']),'style_diff_counts':sum(bool(s['effective_differences'])for s in d['style_results'])}for k,d in report['documents'].items()},ensure_ascii=True))

if __name__=='__main__':run()
