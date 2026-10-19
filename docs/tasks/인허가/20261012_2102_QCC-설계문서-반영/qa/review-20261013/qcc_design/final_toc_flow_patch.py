from pathlib import Path
from copy import deepcopy
import json, uuid, zipfile
import final_layout_patch as F
from final_layout_patch import Q,N,P,E,visible,sha,dead

F.ROOT=Path(__file__).parent/'final-toc-flow'
F.ROOT.mkdir(exist_ok=True)
F.EXPECTED['SAD']='ff7726ea2270a6fd34cb10322770a9c8a003b85edc527589578b355d25376576'

class Patch(F.Patch):
    def __init__(self):
        super().__init__('SAD');self.created_mark_rPr=set()
    def replace(self,p,tabs=False,keep_next=False):
        # Whole balanced TOC entry or plain question is replaced to retain older, unaccepted pPrChange history.
        oldtext=visible(p);newp=E.Element(Q('p'));newp.set(P,uuid.uuid4().hex[:8].upper())
        pp=deepcopy(p.find('w:pPr',N))
        for x in list(pp.findall('w:pPrChange',N)):pp.remove(x)
        rp=pp.find('w:rPr',N)
        if rp is None:rp=E.Element(Q('rPr'));pp.append(rp)
        for x in list(rp):
            if x.tag in [Q('ins'),Q('del'),Q('rPrChange')]:rp.remove(x)
        rp.append(self.rev('ins'))
        if tabs:
            assert pp.find('w:tabs',N)is None
            ts=E.Element(Q('tabs'))
            for val,pos,leader in [('clear',1200,None),('right',9628,'dot')]:
                x=E.SubElement(ts,Q('tab'));x.set(Q('val'),val);x.set(Q('pos'),str(pos))
                if leader:x.set(Q('leader'),leader)
            pp.insert(1,ts)
            assert pp.find('w:spacing',N).get(Q('line'))=='240'
        if keep_next:
            x=pp.find('w:keepNext',N)
            if x is None:x=E.Element(Q('keepNext'));pp.insert(1,x)
            x.set(Q('val'),'1')
        # Actual Word pPr order; inherited font/indent/spacing/numbering are retained.
        rank={'pStyle':0,'keepNext':1,'keepLines':2,'pageBreakBefore':3,'widowControl':4,'numPr':5,'tabs':6,'spacing':7,'ind':8,'jc':9,'rPr':10}
        children=list(pp)
        for c in children:pp.remove(c)
        for c in sorted(children,key=lambda c:rank.get(E.QName(c).localname,8)):pp.append(c)
        newp.append(pp);ins=self.rev('ins');newp.append(ins)
        runs=p.xpath('.//w:r[not(ancestor::w:del)]',namespaces=N)
        # All fields are balanced within each selected TOC entry; the outer TOC begin/end is in other paragraphs.
        if tabs:
            kinds=[r.get(Q('fldCharType'))for r in p.xpath('.//w:fldChar[not(ancestor::w:del)]',namespaces=N)]
            assert kinds.count('begin')==kinds.count('end')==2
        for r in runs:
            nr=deepcopy(r)
            for x in list(nr.xpath('./w:rPr/w:rPrChange',namespaces=N)):x.getparent().remove(x)
            ins.append(nr)
        for r in runs:
            parent=r.getparent();at=parent.index(r);d=self.rev('del');parent.remove(r);parent.insert(at,d);d.append(r)
            for t in r.findall('w:t',N):t.tag=Q('delText')
            for t in r.findall('w:instrText',N):t.tag=Q('delInstrText')
        oldpp=p.find('w:pPr',N);oldrp=oldpp.find('w:rPr',N)
        if oldrp is None:
            oldrp=E.Element(Q('rPr'));at=len(oldpp)-int(oldpp.find('w:pPrChange',N)is not None);oldpp.insert(at,oldrp)
            mark=self.rev('del');oldrp.append(mark);self.created_mark_rPr.add(mark.get(Q('id')))
        else:
            assert oldrp.find('w:del',N)is None
            oldrp.append(self.rev('del'))
        parent=p.getparent();parent.insert(parent.index(p)+1,newp)
        assert visible(newp)==oldtext
        self.changes.append({'before_para_id':p.get(P),'after_para_id':newp.get(P),'text_unchanged':oldtext,'operation':'tracked paragraph replacement preserving earlier format history','flow':'clear inherited left tab1200 and retain right page tab9628 dotted leader/line240'if tabs else'keepNext1 to join question with answer','font_size_and_spacing_preserved':True,'field_instructions_and_bookmark_targets_preserved':True})
    def reject(self):
        r=deepcopy(self.r)
        for p in list(r.iter(Q('p'))):
            if any(x.get(Q('id'))in self.ids for x in p.xpath('./w:pPr/w:rPr/w:ins',namespaces=N)):p.getparent().remove(p)
        for e in list(r.xpath('.//*[@w:id]',namespaces=N))[::-1]:
            rid=e.get(Q('id'))
            if rid not in self.ids or e.getparent()is None:continue
            parent=e.getparent();at=parent.index(e)
            if e.tag==Q('del')and parent.tag!=Q('rPr'):
                for c in list(e):
                    for t in c.iter(Q('delText')):t.tag=Q('t')
                    for t in c.iter(Q('delInstrText')):t.tag=Q('instrText')
                    e.remove(c);parent.insert(at,c);at+=1
            parent.remove(e)
            if rid in self.created_mark_rPr:
                assert len(parent)==0;parent.getparent().remove(parent)
        return r

def main():
    p=Patch()
    for pid in ['34949DA7','4470D281','3748799A']:p.replace(p.para(pid),tabs=True)
    question=p.para('08E166C3');assert '연계성이 확인'in visible(question);p.replace(question,keep_next=True)
    out=p.finish();out['style_tab_evidence']={'toc2_inherited_left':1200,'toc2_inherited_right':9628,'leader':'dot','original_current':True,'problem':'short label reaches inherited left tab instead of right page tab','affected_entries':['1.1. 개요','1.2. 목적','1.3. 대상']};out['accepted_body_text_and_actual_field_codes_unchanged']=True
    (F.ROOT/'proof.json').write_text(json.dumps({'SAD':out},ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps({'candidate':out['candidate'],'before':out['before_sha256'],'after':out['candidate_sha256'],'paragraphs':len(out['changes'])},ensure_ascii=True))
if __name__=='__main__':main()
