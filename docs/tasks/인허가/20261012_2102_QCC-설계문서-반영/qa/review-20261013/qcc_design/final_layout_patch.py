from pathlib import Path
from copy import deepcopy
from datetime import datetime, timezone
import zipfile, json, hashlib, uuid
from lxml import etree as E

ROOT = Path(__file__).parent / 'final-layout'
ROOT.mkdir(exist_ok=True)
BASE = Path('G:/공유 드라이브/20_프로젝트별/01_project sound1/08_DHF/03_설계 출력/SW/외부기')
W = 'http://schemas.openxmlformats.org/wordprocessingml/2006/main'
P = '{http://schemas.microsoft.com/office/word/2010/wordml}paraId'
N = {'w': W}
Q = lambda t: '{' + W + '}' + t
DATE = datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace('+00:00', 'Z')
GRAMMAR = {'장치명를': '장치명을', '활성를': '활성화를', '하향를': '하향 경로를'}
EXPECTED = {'SAD': 'da3ad4861b3915b8eb64eaf89a09ce4749ef6dbd220283488d4576713da60e6e', 'SDDD': 'c6b56a6464c17b2b7b393f5d0d3f822e8fd55dc03701d3f0306dddd15da83e97', 'SIP': 'd2051a712ca509466c6d0fbeb0841a250af328d6dbe6570125b496f5a0400d40'}

def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def dead(x):
    return any(a.tag == Q('del') or (a.tag == Q('tr') and a.find('w:trPr/w:del', N) is not None) for a in [x, *x.iterancestors()])
def visible(x):
    if dead(x): return ''
    if x.tag in [Q('pPr'), Q('rPr'), Q('tblPr'), Q('trPr'), Q('tcPr')]: return ''
    if x.tag == Q('t'): return x.text or ''
    if x.tag == Q('br'): return '\n'
    if x.tag == Q('tab'): return '\t'
    return ''.join(visible(c) for c in x)
def transform(t):
    for a, b in GRAMMAR.items(): t = t.replace(a, b)
    return t.replace('·', '/')

class Patch:
    def __init__(self, code):
        self.code = code
        self.src = next(BASE.glob('[[]E8300-QCC[]] ' + code + '*.docx'))
        self.before_hash = sha(self.src)
        assert self.before_hash == EXPECTED[code], 'G concurrent baseline changed: ' + code
        (ROOT / (code + '-before.docx')).write_bytes(self.src.read_bytes())
        with zipfile.ZipFile(self.src) as z: self.entries = [(deepcopy(i), z.read(i.filename)) for i in z.infolist()]
        self.parts = {i.filename: b for i, b in self.entries}
        self.r = E.fromstring(self.parts['word/document.xml'])
        self.before = deepcopy(self.r)
        self.expected_text = transform(visible(self.before))
        self.ids = set(); self.created = {}; self.changes = []
        self.next = max(int(v) for v in self.r.xpath('.//@w:id', namespaces=N) if v.isdigit()) + 1
    def rev(self, tag):
        e = E.Element(Q(tag)); e.set(Q('id'), str(self.next)); e.set(Q('author'), '김은수'); e.set(Q('date'), DATE)
        self.ids.add(str(self.next)); self.next += 1
        return e
    def para(self, pid):
        ps = [p for p in self.r.iter(Q('p')) if p.get(P) == pid]
        assert len(ps) == 1, (pid, len(ps))
        return ps[0]
    def delete_runs(self, p):
        for r in list(p.xpath('.//w:r[not(ancestor::w:del) and (w:t or w:br or w:tab)]', namespaces=N)):
            par = r.getparent(); at = par.index(r); d = self.rev('del'); par.remove(r); par.insert(at, d); d.append(r)
            for t in r.findall('w:t', N): t.tag = Q('delText')
    def mark_delete(self, p):
        pp = p.find('w:pPr', N); assert pp is not None
        rp = pp.find('w:rPr', N)
        if rp is None:
            rp = E.Element(Q('rPr')); pp.insert(len(pp) - int(pp.find('w:pPrChange', N) is not None), rp)
            self.created[id(rp)] = True
        assert rp.find('w:del', N) is None, 'Do not revise already deleted paragraph'
        rp.append(self.rev('del'))
    def replace_insertion(self, old):
        p = old.getparent(); assert p.tag == Q('p')
        assert all(c.tag == Q('r') for c in old), 'Source insertion includes non-run history'
        oldtext = visible(old); new = self.rev('ins')
        for c in old:
            nc = deepcopy(c)
            for t in nc.iter(Q('t')): t.text = transform(t.text or '')
            new.append(nc)
        d = self.rev('del')
        for c in list(old):
            old.remove(c); d.append(c)
            for t in c.iter(Q('t')): t.tag = Q('delText')
        old.append(d); p.insert(p.index(old) + 1, new)
        self.changes.append({'type': 'grammar or new QCC separator', 'para_id': p.get(P), 'before': oldtext, 'after': visible(new)})
    def text_corrections(self):
        containers = []
        for t in self.r.xpath('.//w:t[not(ancestor::w:del)]', namespaces=N):
            if dead(t) or transform(t.text or '') == (t.text or ''): continue
            ins = next((a for a in t.iterancestors() if a.tag == Q('ins')), None)
            assert ins is not None and ins.get(Q('date'), '') >= '2026-10-12', 'Original E wording must stay untouched'
            if ins not in containers: containers.append(ins)
        for ins in containers: self.replace_insertion(ins)
    def format(self, p, properties, label):
        pp = p.find('w:pPr', N); assert pp is not None
        assert pp.find('w:pPrChange', N) is None, 'Existing format history needs paragraph replacement'
        original = deepcopy(pp)
        order = ['pStyle','keepNext','keepLines','pageBreakBefore','framePr','widowControl','numPr','suppressLineNumbers','pBdr','shd','tabs','suppressAutoHyphens','kinsoku','wordWrap','overflowPunct','topLinePunct','autoSpaceDE','autoSpaceDN','bidi','adjustRightInd','snapToGrid','spacing','ind','contextualSpacing','mirrorIndents','suppressOverlap','jc','textDirection','textAlignment','textboxTightWrap','outlineLvl','divId','cnfStyle','rPr','sectPr','pPrChange']
        for tag, attrs in properties.items():
            el = pp.find('w:' + tag, N)
            if el is None:
                el = E.Element(Q(tag)); rank = order.index(tag)
                at = next((i for i,c in enumerate(pp) if E.QName(c).localname in order and order.index(E.QName(c).localname) > rank), len(pp)); pp.insert(at, el)
            for key,value in attrs.items(): el.set(Q(key), str(value))
        change = self.rev('pPrChange'); change.append(original); pp.append(change)
        self.changes.append({'type': label, 'para_id': p.get(P), 'properties': properties})
    def replacement_paragraph(self, p, properties, label):
        oldtext = visible(p)
        newp = E.Element(Q('p')); newp.set(P, uuid.uuid4().hex[:8].upper())
        pp = deepcopy(p.find('w:pPr', N))
        for x in list(pp.findall('w:pPrChange', N)): pp.remove(x)
        rp = pp.find('w:rPr', N)
        if rp is None: rp = E.SubElement(pp, Q('rPr'))
        for x in list(rp):
            if x.tag in [Q('ins'), Q('del'), Q('rPrChange')]: rp.remove(x)
        rp.append(self.rev('ins')); newp.append(pp)
        # These four SIP paragraphs contain plain text/line breaks, with their original bookmark retained in the old paragraph.
        assert not p.xpath('.//w:fldChar|.//w:drawing', namespaces=N)
        ins = self.rev('ins'); newp.append(ins)
        for r in p.xpath('.//w:r[not(ancestor::w:del) and (w:t or w:br or w:tab)]', namespaces=N):
            nr = deepcopy(r)
            for t in nr.iter(Q('t')): t.text = transform(t.text or '')
            for x in list(nr.xpath('./w:rPr/w:rPrChange', namespaces=N)): x.getparent().remove(x)
            ins.append(nr)
        # Set normal paragraph flow before inserting a new actual paragraph.
        for tag, val in properties.items():
            x = pp.find('w:' + tag, N)
            if x is None:
                x = E.Element(Q(tag)); pp.insert(1 if pp.find('w:pStyle', N) is not None else 0, x)
            x.set(Q('val'), str(val))
        # Canonical pPr ordering for the inserted body paragraph.
        rank = {'pStyle':0,'keepNext':1,'keepLines':2,'pageBreakBefore':3,'widowControl':4,'spacing':5,'ind':6,'jc':7,'rPr':8}
        children = list(pp)
        for c in children: pp.remove(c)
        for c in sorted(children, key=lambda c: rank.get(E.QName(c).localname, 5)): pp.append(c)
        self.delete_runs(p); self.mark_delete(p)
        par = p.getparent(); par.insert(par.index(p)+1, newp)
        self.changes.append({'type': label, 'before_para_id': p.get(P), 'after_para_id': newp.get(P), 'before': oldtext, 'after': visible(newp), 'flow': properties})
    def sad(self):
        sdt = self.r.find('w:body/w:sdt', N)
        entries = [p for p in sdt.xpath('.//w:p', namespaces=N) if (p.find('w:pPr/w:pStyle', N) is not None and p.find('w:pPr/w:pStyle', N).get(Q('val')) in ['12','20','30'])]
        assert len(entries) == 32
        for p in entries: self.format(p, {'spacing': {'after':0,'line':240,'lineRule':'auto'}}, 'normal TOC line spacing; font unchanged')
        # Put the actual upper tracing in the existing requirement-centered row, beside the preserved notice.
        rows = [tr for tr in self.r.iter(Q('tr')) if not dead(tr) and tr.findall('w:tc', N) and visible(tr.findall('w:tc', N)[0]).strip() == 'SW-FW-RID-83']
        assert len(rows) == 1
        cell = rows[0].findall('w:tc', N)[2]; p = cell.find('w:p', N)
        old_cell_text = visible(p)
        plain = next(r for r in p.xpath('.//w:r[not(ancestor::w:del)]', namespaces=N) if r.find('w:t', N) is not None and not (r.find('w:rPr/w:color',N) is not None and r.find('w:rPr/w:color',N).get(Q('val')) == '0070C0'))
        ins = self.rev('ins'); r = E.SubElement(ins, Q('r'))
        rp = plain.find('w:rPr',N)
        if rp is not None: r.append(deepcopy(rp))
        t = E.SubElement(r,Q('t')); t.text = '상위 요구사항: SYS-RID-ACDOC-17 / SYS-RID-ACDOC-18 / SYS-RID-ACDOC-20.'
        E.SubElement(r,Q('br')); p.insert(1 if p.find('w:pPr',N) is not None else 0,ins)
        duplicate = self.para('7EB2C45B'); text = visible(duplicate)
        assert self.expected_text.count(old_cell_text) == 1
        self.expected_text = self.expected_text.replace(old_cell_text, t.text + '\n' + old_cell_text)
        assert self.expected_text.count(text) == 1
        self.expected_text = self.expected_text.replace(text, '')
        self.delete_runs(duplicate); self.mark_delete(duplicate)
        self.changes.append({'type':'consolidate duplicate RID83 notice into existing requirement row','removed_para_id':'7EB2C45B','before':text,'retained_requirement':'SW-FW-RID-83','retained_upper_ids':['SYS-RID-ACDOC-17','SYS-RID-ACDOC-18','SYS-RID-ACDOC-20'],'notice_preserved':True})
    def sip(self):
        for pid, props in [
            ('1C0ED646',{'pageBreakBefore':1,'keepNext':1,'keepLines':1}),
            ('003EEAA4',{'keepNext':1,'keepLines':1}),
            ('08BC3354',{'keepNext':1,'keepLines':1}),
            ('3709D544',{'keepNext':0,'keepLines':1})]:
            self.replacement_paragraph(self.para(pid),props,'keep boot integration explanation and pending OTA notice together in a normal body section')
    def key_rows(self):
        tables = [t for t in self.r.iter(Q('tbl')) if not dead(t) and 'shared_service_key' in visible(t) and '데이터사용 역할저장/처리 경로' in visible(t)]
        assert len(tables) == 1
        rows = tables[0].findall('w:tr',N)[1:]; assert len(rows) == 4
        for row in rows:
            pr = row.find('w:trPr',N)
            assert len(pr) == 1 and pr[0].tag == Q('ins'), 'Unexpected preexisting row formatting'
            pr.insert(0,E.Element(Q('cantSplit')))
            change = self.rev('trPrChange'); change.append(E.Element(Q('trPr'))); pr.append(change)
            self.changes.append({'type':'keep short key-role data row together','key':visible(row.findall('w:tc',N)[0]),'cantSplit':True,'original_row_insertion_preserved':True})
    def reject(self):
        r = deepcopy(self.r)
        for p in list(r.iter(Q('p'))):
            if any(x.get(Q('id')) in self.ids for x in p.xpath('./w:pPr/w:rPr/w:ins', namespaces=N)): p.getparent().remove(p)
        for el in list(r.xpath('.//*[@w:id]', namespaces=N))[::-1]:
            if el.get(Q('id')) not in self.ids or el.getparent() is None: continue
            par = el.getparent(); at = par.index(el)
            if el.tag == Q('pPrChange'):
                assert par.tag == Q('pPr'); old = deepcopy(el.find('w:pPr', N)); p=par.getparent(); p.replace(par,old)
            elif el.tag == Q('trPrChange'):
                assert par.tag == Q('trPr') and len(el.find('w:trPr',N)) == 0
                par.remove(par.find('w:cantSplit',N)); par.remove(el)
            elif el.tag == Q('del') and par.tag != Q('rPr'):
                for c in list(el):
                    for t in c.iter(Q('delText')): t.tag = Q('t')
                    el.remove(c); par.insert(at,c); at += 1
                par.remove(el)
            else: par.remove(el)
        return r
    def finish(self):
        assert E.tostring(self.reject(),method='c14n') == E.tostring(self.before,method='c14n'), 'Reject differs from immediate baseline: '+self.code
        assert visible(self.r) == self.expected_text, 'Accepted text includes undeclared difference: '+self.code
        for ins in self.r.xpath('.//w:ins',namespaces=N):
            if ins.get(Q('id')) in self.ids: assert not ins.xpath('ancestor::w:ins|ancestor::w:del',namespaces=N)
        assert all(x.get(Q('author'))=='김은수' for x in self.r.xpath('.//*[@w:id]',namespaces=N)if x.get(Q('id'))in self.ids)
        assert not any(a in visible(self.r)for a in GRAMMAR)
        assert '·' not in visible(self.r), 'Remaining active dot requires intentional original review'
        self.parts['word/document.xml']=E.tostring(self.r,encoding='UTF-8',xml_declaration=True,standalone=True)
        candidate=ROOT/self.src.name
        with zipfile.ZipFile(candidate,'w')as z:
            for info,data in self.entries: z.writestr(info,self.parts.get(info.filename,data))
        with zipfile.ZipFile(candidate)as z:
            for info,data in self.entries:
                if info.filename!='word/document.xml': assert z.read(info.filename)==data
        assert sha(self.src)==self.before_hash,'G concurrent change'
        return {'code':self.code,'source':str(self.src),'before_sha256':self.before_hash,'candidate':str(candidate),'candidate_sha256':sha(candidate),'new_revision_ids':sorted(self.ids,key=int),'author':'김은수','reject_new_exact_baseline_c14n':True,'accepted_text_only_declared_changes':True,'new_nested_insertions':0,'all_other_package_parts_exact':True,'styles_fonts_media_field_instructions_preserved':True,'changes':self.changes,'G_published':False,'static_status':'통과','native_status':'미실행','toc_cache':'root final page-number update required; unchanged heading text'}

def main():
    reports={}
    for code in ['SAD','SDDD','SIP']:
        p=Patch(code);p.text_corrections()
        if code=='SAD':p.sad()
        if code=='SIP':p.sip()
        reports[code]=p.finish()
    (ROOT/'proof.json').write_text(json.dumps(reports,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps({c:{'candidate':v['candidate'],'sha256':v['candidate_sha256'],'changes':len(v['changes'])}for c,v in reports.items()},ensure_ascii=True))
if __name__=='__main__':main()
