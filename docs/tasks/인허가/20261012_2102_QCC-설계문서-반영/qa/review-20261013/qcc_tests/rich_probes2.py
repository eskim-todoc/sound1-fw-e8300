from pathlib import Path
from lxml import etree as E
import json,zipfile,copy
from integrate import N,NS,texts,sha
H=Path(__file__).resolve().parent
with zipfile.ZipFile(H/'SUVC-probe-canonical-rich.xlsx') as z:
    infos=z.infolist();base={i.filename:z.read(i.filename) for i in infos}
member='xl/worksheets/sheet2.xml';results=[]
for mode in ['bare-rich','color-only-rich','font-size-rich','no-empty-rich','shared-rich']:
    d=base.copy();root=E.fromstring(d[member])
    for isn in root.findall('.//x:c/x:is',N):
        for rp in isn.findall('x:r/x:rPr',N):
            if mode=='bare-rich':rp.getparent().remove(rp)
            elif mode in ['color-only-rich','font-size-rich']:
                allowed={'color'} if mode=='color-only-rich' else {'rFont','sz'}
                for c in list(rp):
                    if E.QName(c).localname not in allowed:rp.remove(c)
        if mode=='no-empty-rich':
            for r in list(isn):
                if not texts(r):isn.remove(r)
    changed=[member]
    if mode=='shared-rich':
        ss=E.fromstring(d['xl/sharedStrings.xml'])
        for c in root.findall('.//x:c[@t="inlineStr"]',N):
            isn=c.find('x:is',N)
            si=E.SubElement(ss,'{'+NS+'}si')
            for child in isn:si.append(copy.deepcopy(child))
            idx=len(ss)-1
            for child in list(c):c.remove(child)
            c.set('t','s');E.SubElement(c,'{'+NS+'}v').text=str(idx)
        ss.set('uniqueCount',str(len(ss)))
        ss.set('count',str(int(ss.get('count','0'))+len(root.findall('.//x:c[@t="s"]',N))))
        d['xl/sharedStrings.xml']=E.tostring(ss,xml_declaration=True,encoding='utf-8',standalone=True)
        changed.append('xl/sharedStrings.xml')
    d[member]=E.tostring(root,xml_declaration=True,encoding='utf-8',standalone=True)
    p=H/('SUVC-probe-'+mode+'.xlsx')
    with zipfile.ZipFile(p,'w') as z:
        for i in infos:z.writestr(i,d[i.filename])
    results.append({'mode':mode,'path':str(p),'sha256':sha(p),'changed_parts':changed})
(H/'open-candidates2.json').write_text(json.dumps(results,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(results,ensure_ascii=True))
