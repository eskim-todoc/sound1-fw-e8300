from pathlib import Path
from copy import deepcopy
import json,zipfile,uuid,re,hashlib,sys
from lxml import etree as E
import final_layout_patch as F
import final_toc_flow_patch as T
from final_layout_patch import Q,N,P,visible,dead,sha

ROOT=Path(__file__).parent/'final-mic-patch';ROOT.mkdir(exist_ok=True)
F.ROOT=ROOT
LOC=json.loads((ROOT/'input-locators.json').read_text(encoding='utf8'))
ASSET=Path(__file__).parent/'figures/시스템 블록도.png'
OLD='070b60b0a85a3540110a40f9ed3e5e427fe6e973f5186192e99de5d58c11b38b'
R='http://schemas.openxmlformats.org/officeDocument/2006/relationships'
REL='http://schemas.openxmlformats.org/package/2006/relationships'
X={**N,'a':'http://schemas.openxmlformats.org/drawingml/2006/main','wp':'http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing'}
ID=re.compile(r'S[IU]-ID-[A-Z0-9]+-\d{3}')

class Extra:
    def init_extra(self):self.newparts={};self.figures=[];self.created_empty=set();self.trace_expected={}
    def figure(self):
        rels=E.fromstring(self.parts['word/_rels/document.xml.rels']);targets={x.get('Id'):x.get('Target')for x in rels}
        found=[]
        for b in self.r.xpath('.//a:blip',namespaces=X):
            if dead(b):continue
            rid=b.get('{'+R+'}embed');part='word/'+targets[rid]
            if hashlib.sha256(self.parts[part]).hexdigest()==OLD:found.append((b,rid,part))
        assert len(found)==1,(self.code,len(found))
        b,oldrid,oldpart=found[0];run=next(a for a in b.iterancestors()if a.tag==Q('r'));p=next(a for a in run.iterancestors()if a.tag==Q('p'))
        newrun=deepcopy(run);rid='rIdFinalMic20261013'+self.code;part='word/media/final-mic-'+self.code+'.png'
        assert rid not in targets and part not in self.parts
        E.SubElement(rels,'{'+REL+'}Relationship',Id=rid,Type=R+'/image',Target=part[5:])
        self.parts['word/_rels/document.xml.rels']=E.tostring(rels,encoding='UTF-8',xml_declaration=True,standalone=True)
        self.newparts[part]=ASSET.read_bytes()
        for v in newrun.xpath('.//a:blip',namespaces=X):v.set('{'+R+'}embed',rid)
        maxdp=max(int(v)for v in self.r.xpath('.//wp:docPr/@id',namespaces=X))
        for d in newrun.xpath('.//wp:docPr',namespaces=X):d.set('id',str(maxdp+1));d.set('name','DMIC2 양쪽 연결 및 통화 마이크 입력');d.set('descr','DMIC2 양쪽 연결 및 통화 마이크 입력')
        for d in newrun.iter():
            if E.QName(d).localname=='creationId' and d.get('id'):d.set('id','{'+str(uuid.uuid4()).upper()+'}')
            for k in list(d.attrib):
                if E.QName(k).localname in ['anchorId','editId']:d.set(k,uuid.uuid4().hex[:8].upper())
        parent=run.getparent();at=parent.index(run);d=self.rev('del');parent.remove(run);parent.insert(at,d);d.append(run)
        p.append(self.rev('ins'));p[-1].append(newrun)
        self.figures.append({'paragraph_id':p.get(P),'old_rid':oldrid,'old_part':oldpart,'old_sha256':OLD,'new_rid':rid,'new_part':part,'asset_sha256':sha(ASSET),'old_bytes_and_relationship_retained':True,'extent_unchanged':True,'change':'QCC input inferred PDM replaced with confirmed phone microphone input; E8300 PDM unchanged'})
    def text(self,p,newtext,label):
        old=visible(p);runs=[r for r in p.xpath('.//w:r[not(ancestor::w:del)]',namespaces=N)if r.find('w:t',N)is not None]
        assert runs and not p.xpath('.//w:fldChar|.//w:drawing',namespaces=N)
        rp=deepcopy(runs[0].find('w:rPr',N)) if runs[0].find('w:rPr',N)is not None else None
        self.replacement_paragraph(p,{},label);new=p.getparent()[p.getparent().index(p)+1];ins=new.find('w:ins',N)
        for c in list(ins):ins.remove(c)
        r=E.SubElement(ins,Q('r'))
        if rp is not None:
            for c in list(rp.findall('w:rPrChange',N)):rp.remove(c)
            r.append(rp)
        for i,s in enumerate(newtext.split('\n')):
            if i:E.SubElement(r,Q('br'))
            t=E.SubElement(r,Q('t'));t.text=s;t.set('{http://www.w3.org/XML/1998/namespace}space','preserve')
        assert self.expected_text.count(old)==1
        self.expected_text=self.expected_text.replace(old,newtext)
        self.changes[-1]['after']=newtext
        return new
    def trace_cell(self,p,rid,column):
        old=visible(p);runs=p.xpath('.//w:r[not(ancestor::w:del)]',namespaces=N)
        plain=[];notice=[];in_notice=False
        for r in runs:
            if not any(r.find('w:'+t,N)is not None for t in ['t','br','tab']):continue
            if visible(r).startswith(('[QCC','[E8300')):in_notice=True
            (notice if in_notice else plain).append(r)
        head=''.join(visible(r)for r in plain).rstrip('\n');tail=''.join(visible(r)for r in notice)
        notice=[deepcopy(r)for r in notice]
        matches=list(ID.finditer(head));assert matches and head[:matches[0].start()].strip()==''
        lines=[]
        for i,m in enumerate(matches):
            name=head[m.end():matches[i+1].start() if i+1<len(matches) else len(head)].strip()
            assert name,(rid,column,m.group())
            lines.extend([m.group(),name])
        newhead='\n'.join(lines);newtext=newhead+('\n'+tail if tail else'')
        assert re.sub(r'\s','',newtext)==re.sub(r'\s','',old),'ordinary text differs'
        rp=deepcopy(plain[0].find('w:rPr',N))if plain[0].find('w:rPr',N)is not None else None
        self.replacement_paragraph(p,{},'requirement trace ID and name line boundaries restored to E8300 format')
        new=p.getparent()[p.getparent().index(p)+1];ins=new.find('w:ins',N)
        for c in list(ins):ins.remove(c)
        r=E.SubElement(ins,Q('r'))
        if rp is not None:
            for c in list(rp.findall('w:rPrChange',N)):rp.remove(c)
            r.append(rp)
        for i,s in enumerate(lines):
            if i:E.SubElement(r,Q('br'))
            t=E.SubElement(r,Q('t'));t.text=s;t.set('{http://www.w3.org/XML/1998/namespace}space','preserve')
        if notice:
            E.SubElement(r,Q('br'))
            for nr in notice:ins.append(deepcopy(nr))
        assert visible(new)==newtext,repr((rid,column,visible(new),newtext))
        self.trace_expected[p.get(P)]=newtext
        self.changes[-1].update({'requirement':rid,'column':column,'after':newtext,'ordinary_text_order_unchanged':True,'notice_runs_bytes_unchanged':True,'ID_name_pairs':len(matches)})
    def trace(self):
        count=0
        for tr in list(self.r.iter(Q('tr'))):
            cs=tr.findall('w:tc',N)
            if dead(tr)or len(cs)!=3 or not re.fullmatch(r'SW-FW-RID-\d+',visible(cs[0]).strip()):continue
            rid=visible(cs[0]).strip()
            for col,tc in enumerate(cs[1:],1):
                if ('SI-ID-QCC-'if col==1 else'SU-ID-QCC-')not in visible(tc):continue
                ps=[p for p in tc.findall('w:p',N)if visible(p)]
                assert len(ps)==1,(rid,col,len(ps))
                self.trace_cell(ps[0],rid,col);count+=1
        assert count==86,count
        def expected(e):
            if e.tag==Q('p') and e.get(P)in self.trace_expected:return self.trace_expected[e.get(P)]
            if dead(e):return ''
            if e.tag in [Q('pPr'),Q('rPr'),Q('tblPr'),Q('trPr'),Q('tcPr')]:return ''
            if e.tag in [Q('t'),Q('br'),Q('tab')]:return visible(e)
            return ''.join(expected(c)for c in e)
        self.expected_text=expected(self.before)
        return count
    def finish_extra(self):
        # Text paragraph replacements may create an rPr container absent from the immediate baseline.
        rejected=self.reject()
        bp={p.get(P):p for p in self.before.iter(Q('p'))}
        for p in rejected.iter(Q('p')):
            if p.get(P)in bp:
                old=bp[p.get(P)].find('w:pPr/w:rPr',N);rp=p.find('w:pPr/w:rPr',N)
                if old is None and rp is not None and len(rp)==0:rp.getparent().remove(rp)
        assert E.tostring(rejected,method='c14n')==E.tostring(self.before,method='c14n'),self.code+' reject exact failed'
        assert visible(self.r)==self.expected_text,self.code+' accepted undeclared text'
        newrev=[x for x in self.r.xpath('.//*[@w:id]',namespaces=N)if x.get(Q('id'))in self.ids]
        assert all(x.get(Q('author'))=='김은수'for x in newrev)
        assert not any(x.get(Q('id'))in self.ids for x in self.r.xpath('.//w:ins[ancestor::w:ins or ancestor::w:del]',namespaces=N))
        self.parts['word/document.xml']=E.tostring(self.r,encoding='UTF-8',xml_declaration=True,standalone=True)
        candidate=ROOT/self.src.name
        with zipfile.ZipFile(candidate,'w')as z:
            for i,data in self.entries:z.writestr(i,self.parts.get(i.filename,data))
            for name,data in self.newparts.items():z.writestr(name,data,compress_type=zipfile.ZIP_DEFLATED)
        with zipfile.ZipFile(candidate)as z:
            unchanged=[]
            for i,data in self.entries:
                if i.filename not in ['word/document.xml','word/_rels/document.xml.rels']:assert z.read(i.filename)==data;unchanged.append(i.filename)
            for i,data in self.entries:
                if i.filename.startswith('word/media/'):assert z.read(i.filename)==data
            for n in z.namelist():
                if n.endswith('.xml')or n.endswith('.rels'):E.fromstring(z.read(n))
        assert sha(self.src)==self.before_hash,'G baseline changed'
        return {'code':self.code,'source':str(self.src),'before_sha256':self.before_hash,'candidate':str(candidate),'candidate_sha256':sha(candidate),'new_revision_ids':sorted(self.ids,key=int),'author':'김은수','reject_only_this_patch_exact_immediate_G_c14n':True,'accepted_text_only_declared_changes':True,'new_nested_insertions':0,'all_other_parts_exact':unchanged,'styles_numbering_fonts_and_original_media_bytes_preserved':True,'changes':self.changes,'figures':self.figures,'G_written':False,'static_status':'통과','native_status':'미실행'}

class Plain(Extra,F.Patch):
    def __init__(self,code):
        F.BASE=Path(LOC[code]['path']).parent;F.EXPECTED[code]=LOC[code]['sha256'];super().__init__(code);self.init_extra();self.expected_text=visible(self.before)
class Sad(Extra,T.Patch):
    def __init__(self):
        F.BASE=Path(LOC['SAD']['path']).parent;F.EXPECTED['SAD']=LOC['SAD']['sha256'];super().__init__();self.init_extra();self.expected_text=visible(self.before)

def main():
    only_sddd='--only-sddd'in sys.argv
    proof=json.loads((ROOT/'proof.json').read_text(encoding='utf8'))if only_sddd else {}
    if not only_sddd:
        p=Sad()
        for pid in ['34949DA7','4470D281','3748799A']:p.replace(p.para(pid),tabs=True)
        p.replace(p.para('08E166C3'),keep_next=True);p.figure();p.trace();proof['SAD']=p.finish_extra()
    p=Plain('SDDD');p.figure();old=visible(p.para('2E8869E0'));assert '표 145와'in old
    p.text(p.para('2E8869E0'),old.replace('표 145와','표 146와'),'correct DAC parameter table reference 145 to actual table146')
    # Korean particle follows the table number; preserve the original grammatical relation.
    np=p.r.xpath('.//w:t[contains(text(),"표 146와")]',namespaces=N);assert len(np)==1;np[0].text=np[0].text.replace('표 146와','표 146과');p.expected_text=p.expected_text.replace('표 146와','표 146과');p.changes[-1]['after']=p.changes[-1]['after'].replace('표 146와','표 146과')
    second=[x for x in p.r.iter(Q('p'))if '내부기 파라미터가 표 145'in visible(x)];assert len(second)==1
    old=visible(second[0]);p.text(second[0],old.replace('표 145와','표 146과'),'correct the DAC input/output example reference to actual table146')
    proof['SDDD']=p.finish_extra()
    if only_sddd:
        (ROOT/'proof.json').write_text(json.dumps(proof,ensure_ascii=False,indent=2),encoding='utf8')
        print(json.dumps({'SDDD':proof['SDDD']['candidate_sha256']},ensure_ascii=True));return
    p=Plain('SIP')
    old=visible(p.para('793BE780'));p.text(p.para('793BE780'),old+'\nSU-ID-CFX-005/008/015','add actual E8300 I2S receive units to integration22 trace')
    old=visible(p.para('0A2D1AD4'));p.text(p.para('0A2D1AD4'),old.replace('E8300 CFX 처리','E8300 CFX 수신 버퍼'),'state the integration22 actual receive-buffer boundary')
    old=visible(p.para('7F6AFE45'));addition=' SU-ID-CFX-005 I2S 인터페이스 초기화는 수신 초기화와 인터럽트 준비를 담당한다. SU-ID-CFX-008 I2S 입력 상태 판정은 수신 상태/복사와 스트리밍 갱신을 담당한다. SU-ID-CFX-015 I2S 수신 버퍼 관리는 16표본 FIFO 수신/버퍼 완료를 담당한다. 믹싱/자극은 SW-FW-IntegrationTest-ID-013의 별도 범위이다.'
    p.text(p.para('7F6AFE45'),old+addition,'connect three E8300 receive roles and retain separate mixing/stimulation scope')
    proof['SIP']=p.finish_extra()
    p=Plain('SR');p.figure();proof['SR']=p.finish_extra()
    proof['PDM_basis']={'E8300_PDM_FIFO':'system structure row19 and main.c86/87/lib_audio_in.h22-24,72-75','QCC':'user confirmed shared DMIC2 physical connection and phone microphone capture; QCC PDM setting not found in source docs','QCC_protocol_not_inferred_as_fact':True,'asset':str(ASSET),'asset_sha256':sha(ASSET)}
    (ROOT/'proof.json').write_text(json.dumps(proof,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps({k:{'path':v['candidate'],'sha256':v['candidate_sha256'],'before':v['before_sha256']}for k,v in proof.items()if 'candidate'in v},ensure_ascii=True))
if __name__=='__main__':main()
