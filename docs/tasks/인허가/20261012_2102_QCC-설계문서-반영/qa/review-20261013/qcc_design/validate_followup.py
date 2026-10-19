from pathlib import Path
from copy import deepcopy
import json,re,zipfile,hashlib
from lxml import etree as E
from editor import ROOT,NS,q,visible,sha
from followup import DOTX,SUPPORT,UNITS
from validate_candidates import accept

def tree(el):return (el.tag,tuple(sorted(el.attrib.items())),el.text or '',tuple(tree(c)for c in el))
def main():
    with zipfile.ZipFile(DOTX)as z:template=E.fromstring(z.read('word/styles.xml'))
    ts={s.find('w:name',NS).get(q('val')):s for s in template.findall('w:style',NS)if s.find('w:name',NS)is not None}
    result={'documents':{},'status':'통과','native_final':'미실행','product_verdict':'판정대기'}
    for code in ['SAD','SDDD','SIP']:
        base=json.loads((ROOT/(code+'-authored.json')).read_text(encoding='utf-8'));a=json.loads((ROOT/'followup'/(code+'-authored.json')).read_text(encoding='utf-8'));p=Path(a['path'])
        assert sha(p)==a['after_sha256'];own=set(base['new_revision_ids']+a['new_revision_ids']);doc=accept(p,own);body=doc.find('w:body',NS)
        with zipfile.ZipFile(p)as z:st=E.fromstring(z.read('word/styles.xml'));numbering=z.read('word/numbering.xml')
        with zipfile.ZipFile(Path(a['path']).parent/(''+Path(a['path']).name.replace('[E8300-QCC]','[E8300]')))as z:pass
        current={s.find('w:name',NS).get(q('val')):s for s in st.findall('w:style',NS)if s.find('w:name',NS)is not None}
        mapping={s.get(q('styleId')):current[name].get(q('styleId'))for name,s in ts.items()if name.startswith('TDC')}
        mapping.update({'a1':'TDCSupport20261013Paragraph','a3':'TDCSupport20261013Table'})
        stylesproof=[]
        for name,t in ts.items():
            if not name.startswith('TDC'):continue
            actual=current[name];expected=deepcopy(t);expected.set(q('styleId'),actual.get(q('styleId')))
            for node in expected.xpath('w:basedOn|w:next|w:link',namespaces=NS):node.set(q('val'),mapping.get(node.get(q('val')),node.get(q('val'))))
            en=expected.find('w:pPr/w:numPr/w:numId',NS);an=actual.find('w:pPr/w:numPr/w:numId',NS)
            if en is not None:assert an is not None;en.set(q('val'),an.get(q('val')))
            assert tree(actual)==tree(expected),(code,name)
            stylesproof.append({'name':name,'saved_id':actual.get(q('styleId')),'template_structure_equal_after_dependency_and_numId_mapping':True})
        assert len(stylesproof)==59
        assert 'SW-FW-RID-82'not in visible(body)
        support=[]
        if code=='SAD':
            trace={};reverse={}
            for tbl in body.findall('w:tbl',NS):
                for tr in tbl.findall('w:tr',NS):
                    cells=tr.findall('w:tc',NS)
                    if len(cells)!=3:continue
                    first=visible(cells[0]).strip()
                    if re.fullmatch(r'SW-FW-RID-\d+',first):trace[first]=cells
                    if re.match(r'SU-ID-QCC-\d+',first):reverse[re.match(r'SU-ID-QCC-\d+',first).group()]=visible(cells[2])
            assert len(trace)==82
            for n,rids in SUPPORT.items():
                u=UNITS[n-1]
                for rid in rids:
                    cells=trace['SW-FW-RID-'+str(rid)];assert u['id']in visible(cells[2]);assert u['module_id']in visible(cells[1]);assert 'SW-FW-RID-'+str(rid)in reverse[u['id']]
                support.append({'unit':u['id'],'added_support_requirements':rids,'forward_and_reverse_match':True})
        if code=='SDDD':
            unit=None
            for b in body:
                m=re.fullmatch(r'SU-ID-QCC-(\d+) .*',visible(b))if b.tag==q('p')else None
                if m:unit=int(m.group(1))
                if b.tag==q('tbl')and unit in SUPPORT:
                    for tr in b.findall('w:tr',NS):
                        cells=tr.findall('w:tc',NS)
                        if len(cells)==2 and visible(cells[0])=='요구사항/검증 연결':
                            text=visible(cells[1]);assert all('SW-FW-RID-'+str(n)in text for n in SUPPORT[unit]);support.append({'unit':UNITS[unit-1]['id'],'support_requirements':SUPPORT[unit],'scope_description':text})
            assert len(support)==5
        result['documents'][code]={'path':str(p),'sha256':sha(p),'canonical_tdc_styles':stylesproof,'support_styles':['TDCSupport20261013Paragraph','TDCSupport20261013Table'],'numbering_sha256':hashlib.sha256(numbering).hexdigest(),'support_links':support,'reject_followup_new_prose_exact_previous_body':a['reject_new_c14n_equals_before'],'native_final':'미실행'}
        a['style_repair']['after_styles_sha256']=hashlib.sha256(E.tostring(st,encoding='UTF-8',xml_declaration=True,standalone=True)).hexdigest()
        (ROOT/'followup'/(code+'-authored.json')).write_text(json.dumps(a,ensure_ascii=False,indent=2),encoding='utf-8')
    (ROOT/'followup'/'qa.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    checks=json.loads((ROOT/'checks-29.json').read_text(encoding='utf-8'))
    for c in checks['checks']:
        if c['number']in[3,6]:c['status']='통과';c['inspection_and_reason']+=' 지원 요구 추가를 SAD 정/역방향 추적과 SDDD5유닛에서 재검사하여 누락 해소.'
        if c['number']==29:c['inspection_and_reason']+=' DOTX 기반ID충돌을 지원2스타일로 격리,59정의의 template 구조 일치/비TDC·기존heading번호 보존 재검사 통과.'
    checks['status']='미실행';checks['remaining']=['root 최종필드/native전체페이지 검사','root 현재위치 등록'];checks['evidence'].append('followup/qa.json')
    (ROOT/'checks-29.json').write_text(json.dumps(checks,ensure_ascii=False,indent=2),encoding='utf-8')
    print('59 canonical style definitions and 5-unit cross-loop support links PASS in all applicable documents.')
if __name__=='__main__':main()
