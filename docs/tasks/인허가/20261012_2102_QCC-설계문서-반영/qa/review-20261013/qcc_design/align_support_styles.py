from pathlib import Path
from copy import deepcopy
import json,zipfile
from lxml import etree as E
from editor import ROOT,NS,q,sha

MAPPING={'TDCSupport20261013a1':('TDCSupport20261013Paragraph','공통 양식 본문 기반'),'TDCSupport20261013a3':('TDCSupport20261013Table','공통 양식 표 기반')}
def main():
    rows=[]
    for code in ['SAD','SDDD','SIP']:
        proof=ROOT/'followup'/(code+'-authored.json');a=json.loads(proof.read_text(encoding='utf-8'));p=Path(a['path']);assert sha(p)==a['after_sha256']
        with zipfile.ZipFile(p)as z:items=[(deepcopy(i),z.read(i.filename))for i in z.infolist()]
        parts={i.filename:b for i,b in items};r=E.fromstring(parts['word/styles.xml'])
        for old,(new,name)in MAPPING.items():
            s=r.xpath('w:style[@w:styleId="'+old+'"]',namespaces=NS)[0];s.set(q('styleId'),new);s.find('w:name',NS).set(q('val'),name)
        for n in r.xpath('./w:style/w:basedOn|./w:style/w:next|./w:style/w:link',namespaces=NS):
            if n.get(q('val'))in MAPPING:n.set(q('val'),MAPPING[n.get(q('val'))][0])
        parts['word/styles.xml']=E.tostring(r,encoding='UTF-8',xml_declaration=True,standalone=True)
        candidate=ROOT/'followup'/'candidate'/p.name
        with zipfile.ZipFile(candidate,'w')as z:
            for i,b in items:z.writestr(i,parts[i.filename])
        with zipfile.ZipFile(candidate)as z:
            assert z.read('word/document.xml')==parts['word/document.xml']
            for i,b in items:
                if i.filename!='word/styles.xml':assert z.read(i.filename)==b
        p.write_bytes(candidate.read_bytes());a['after_sha256']=sha(p);a['styles_saved']=59
        a['style_repair']['support_styles']=[{'template_id':key,'new_id':value[0],'name':value[1]}for key,value in [('a1',MAPPING['TDCSupport20261013a1']),('a3',MAPPING['TDCSupport20261013a3'])]]
        (ROOT/'followup'/(code+'-styles-after.xml')).write_bytes(parts['word/styles.xml'])
        proof.write_text(json.dumps(a,ensure_ascii=False,indent=2),encoding='utf-8')
        rows.append({'code':code,'path':str(p),'sha256':sha(p),'saved_canonical_TDC_styles':59})
    (ROOT/'followup'/'stable.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(rows,ensure_ascii=True))
if __name__=='__main__':main()
