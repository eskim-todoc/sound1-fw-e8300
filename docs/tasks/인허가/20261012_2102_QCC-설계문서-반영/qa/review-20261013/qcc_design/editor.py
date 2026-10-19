from pathlib import Path
from copy import deepcopy
from datetime import datetime,timezone
import hashlib,json,re,zipfile
from lxml import etree as E
ROOT=Path(__file__).parent
BASE=Path('G:/공유 드라이브/20_프로젝트별/01_project sound1/08_DHF/03_설계 출력/SW/외부기')
W='http://schemas.openxmlformats.org/wordprocessingml/2006/main'
R='http://schemas.openxmlformats.org/officeDocument/2006/relationships'
REL='http://schemas.openxmlformats.org/package/2006/relationships'
NS={'w':W,'a':'http://schemas.openxmlformats.org/drawingml/2006/main','r':R,'wp':'http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing'}
q=lambda t:'{'+W+'}'+t
DATE=datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace('+00:00','Z')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def add(p,t,**a):
    e=E.SubElement(p,q(t))
    for k,v in a.items():e.set(q(k),str(v))
    return e
def visible(x):
    if x.tag==q('del') or(x.tag==q('tr')and x.find('w:trPr/w:del',NS)is not None):return ''
    if x.tag==q('t'):return x.text or ''
    if x.tag==q('br'):return '\n'
    if x.tag==q('tab'):return '\t'
    return ''.join(visible(c)for c in x)
def clean(x):
    y=deepcopy(x)
    for e in y.xpath('.//w:pPrChange|.//w:rPrChange|.//w:tblPrChange|.//w:tcPrChange|.//w:trPrChange|.//w:tblGridChange|.//w:ins|.//w:del',namespaces=NS):
        if e.getparent()is not None:e.getparent().remove(e)
    return y

class Editor:
    def __init__(self,code):
        self.code=code
        self.path=next(BASE.glob('[[]E8300-QCC[]] '+code+'*.docx'))
        self.baseline=ROOT/'before'/self.path.name
        assert sha(self.path)==sha(self.baseline),'Concurrent input differs: '+code
        self.before_hash=sha(self.path)
        with zipfile.ZipFile(self.path)as z:self.entries=[(deepcopy(i),z.read(i.filename))for i in z.infolist()]
        self.parts={i.filename:b for i,b in self.entries}
        self.root=E.fromstring(self.parts['word/document.xml']);self.before=deepcopy(self.root)
        self.body=self.root.find('w:body',NS);self.blocks=list(self.body)
        self.ids=set();self.revisions=[];self.changes=[];self.figures=[];self.new_parts={}
        self.next_id=max(int(v)for v in self.root.xpath('.//@w:id',namespaces=NS)if v.isdigit())+1
        st=E.fromstring(self.parts['word/styles.xml'])
        self.styles={x.find('w:name',NS).get(q('val')):x.get(q('styleId'))for x in st if x.find('w:name',NS)is not None}
        self.created_containers={}
        self.seq_count=sum('SEQ 표 'in (e.text or '')for e in self.root.xpath('.//w:instrText[not(ancestor::w:del)]',namespaces=NS))
    def rev(self,tag,p):
        e=add(p,tag,id=self.next_id,author='김은수',date=DATE)
        self.ids.add(str(self.next_id));self.revisions.append({'id':str(self.next_id),'type':tag})
        self.next_id+=1;return e
    def block(self,n):return self.blocks[n-1]
    def run(self,parent,text,missing=False):
        r=add(parent,'r')
        if missing:
            rp=add(r,'rPr');add(rp,'b',val=0)
            color='7030A0'if text.startswith('[QCC/E8300')else'0070C0'if text.startswith('[E8300')else'C00000'
            add(rp,'color',val=color);add(rp,'sz',val=24);add(rp,'szCs',val=24);add(rp,'u',val='single')
        for n,s in enumerate(text.split('\n')):
            if n:add(r,'br')
            t=add(r,'t');t.set('{http://www.w3.org/XML/1998/namespace}space','preserve');t.text=s
        return r
    def p(self,text='',level=2,heading=False,keep=False,missing=False,style=None):
        p=E.Element(q('p'));pp=add(p,'pPr')
        add(pp,'pStyle',val=style or self.styles['TDC '+('제목 'if heading else'본문 ')+str(level)])
        if keep:add(pp,'keepNext')
        if not heading and not keep:add(pp,'keepNext',val=0)
        pr=add(pp,'rPr');self.rev('ins',pr)
        if text:self.run(self.rev('ins',p),text,missing=missing)
        return p
    def blank(self,level=2):return self.p(level=level)
    def cap(self,text,level=4):
        self.seq_count+=1
        p=self.p(level=level,style=self.styles['TDC 표 캡션 '+str(level)],keep=True)
        ins=self.rev('ins',p);self.run(ins,'표 ')
        r=add(ins,'r');add(r,'fldChar',fldCharType='begin',dirty='true')
        r=add(ins,'r');add(r,'instrText').text='SEQ 표 \\* ARABIC'
        r=add(ins,'r');add(r,'fldChar',fldCharType='separate')
        self.run(ins,str(self.seq_count));r=add(ins,'r');add(r,'fldChar',fldCharType='end')
        self.run(ins,'. '+text);return p
    def mark_paragraph(self,p,kind):
        pp=p.find('w:pPr',NS)
        if pp is None:pp=E.Element(q('pPr'));p.insert(0,pp)
        rp=pp.find('w:rPr',NS);new=rp is None
        if new:rp=add(pp,'rPr')
        if rp.find('w:'+kind,NS)is not None:return
        e=self.rev(kind,rp)
        if new:self.created_containers[e.get(q('id'))]='rPr'
    def delete_runs(self,p):
        runs=p.xpath('.//w:r[not(ancestor::w:del) and (w:t or w:br or w:tab)]',namespaces=NS)
        for r in runs:
            parent=r.getparent();at=parent.index(r);d=self.rev('del',parent);parent.remove(d);parent.insert(at,d);parent.remove(r);d.append(r)
            for t in r.findall('w:t',NS):t.tag=q('delText')
    def delete(self,b):
        if b.tag==q('p'):
            self.delete_runs(b);self.mark_paragraph(b,'del')
        elif b.tag==q('tbl'):
            for tr in b.findall('w:tr',NS):
                pr=tr.find('w:trPr',NS)
                if pr is None:pr=E.Element(q('trPr'));tr.insert(0,pr)
                if pr.find('w:del',NS)is None:self.rev('del',pr)
        else:raise ValueError('Unsupported delete '+b.tag)
    def insert_before(self,b,els):
        at=self.body.index(b)
        for e in els:self.body.insert(at,e);at+=1
    def insert_after(self,b,els):
        at=self.body.index(b)+1
        for e in els:self.body.insert(at,e);at+=1
    def replace(self,n,text,level=None,missing=None):
        b=self.block(n);old=visible(b)
        if b.tag!=q('p'):raise ValueError(n)
        ps=b.find('w:pPr/w:pStyle',NS);style=ps.get(q('val'))if ps is not None else None
        # Existing old revisions and style definitions remain in the deleted paragraph.
        self.delete(b)
        new=self.p(text,level=level or 2,missing=text.startswith(('[QCC','[E8300'))if missing is None else missing,style=None if level else style)
        self.insert_after(b,[new]);self.changes.append({'before_body_block':n,'before':old,'after':text,'operation':'tracked paragraph replacement'})
    def replace_range(self,start,end,els,label):
        old=[visible(b)for b in self.blocks[start-1:end]]
        self.insert_before(self.block(start),els)
        for b in self.blocks[start-1:end]:self.delete(b)
        self.changes.append({'before_body_blocks':[start,end],'before':old,'after':label,'operation':'tracked section replacement'})
    def cell(self,tc,text):
        ps=tc.findall('w:p',NS)
        for p in ps:self.delete_runs(p)
        ins=self.rev('ins',ps[0])
        for piece in re.split(r'(?=\[(?:QCC|E8300))',text):
            if piece:self.run(ins,piece,missing=piece.startswith(('[QCC','[E8300')))
    def table(self,rows,level=4,widths=None,prototype=None):
        widths=widths or[1304,7512]
        table=E.Element(q('tbl'))
        if prototype is not None:tp=clean(prototype.find('w:tblPr',NS));table.append(tp)
        else:
            tp=add(table,'tblPr');add(tp,'tblStyle',val=self.styles['TDC 표 '+str(level)])
            add(tp,'tblW',w=sum(widths),type='dxa');add(tp,'tblLayout',type='fixed')
            add(tp,'tblLook',val='04A0',firstRow=1,firstColumn=int(len(widths)==2),lastRow=0,lastColumn=0,noHBand=0,noVBand=1)
        # Explicit fixed widths use the matching E8300 table, while indent/margins inherit its common style.
        sw=tp.find('w:tblStyle',NS);sw.set(q('val'),self.styles['TDC 표 '+str(level)])
        tw=tp.find('w:tblW',NS);tw.set(q('w'),str(sum(widths)));tw.set(q('type'),'dxa')
        for node in tp.findall('w:tblInd',NS):tp.remove(node)
        grid=add(table,'tblGrid')
        for width in widths:add(grid,'gridCol',w=width)
        for rn,row in enumerate(rows):
            tr=add(table,'tr');pr=add(tr,'trPr')
            if rn==0:add(pr,'tblHeader');add(pr,'cantSplit')
            self.rev('ins',pr)
            for cn,(width,value)in enumerate(zip(widths,row)):
                tc=add(tr,'tc');cp=add(tc,'tcPr');add(cp,'tcW',w=width,type='dxa');add(cp,'vAlign',val='center')
                role=rn==0 or(len(widths)==2 and cn==0)
                if role:add(cp,'shd',val='clear',color='auto',fill='F5F5F5')
                p=add(tc,'p');pp=add(p,'pPr')
                name='TDC 표 머리행'if rn==0 else'TDC 표 항목'if role else'TDC 표 설명'if len(widths)==2 else'TDC 표 본문'
                add(pp,'pStyle',val=self.styles[name]);add(pp,'jc',val='center'if role else'left')
                if rn==0:add(pp,'keepNext')
                else:add(pp,'keepNext',val=0)
                add(pp,'keepLines',val=0)
                self.rev('ins',add(pp,'rPr'));self.run(self.rev('ins',p),str(value))
        return table
    def figure(self,sequence,asset,label):
        active=[]
        for b in self.root.xpath('.//a:blip[@r:embed and not(ancestor::w:del)]',namespaces=NS):active.append(b)
        blip=active[sequence-1];oldrid=blip.get('{'+R+'}embed')
        run=next(a for a in blip.iterancestors()if a.tag==q('r'));p=next(a for a in run.iterancestors()if a.tag==q('p'))
        newrun=deepcopy(run);rid='rIdReview20261013'+self.code+str(sequence)
        rels=E.fromstring(self.parts['word/_rels/document.xml.rels'])
        assert not any(x.get('Id')==rid for x in rels)
        name='review20261013-'+self.code+'-'+str(sequence)+'.png';part='word/media/'+name
        E.SubElement(rels,'{'+REL+'}Relationship',Id=rid,Type=R+'/image',Target='media/'+name)
        self.parts['word/_rels/document.xml.rels']=E.tostring(rels,encoding='UTF-8',xml_declaration=True,standalone=True)
        self.new_parts[part]=asset.read_bytes()
        for bb in newrun.xpath('.//a:blip',namespaces=NS):bb.set('{'+R+'}embed',rid)
        maxdp=max(int(v)for v in self.root.xpath('.//wp:docPr/@id',namespaces=NS))
        for dp in newrun.xpath('.//wp:docPr',namespaces=NS):dp.set('id',str(maxdp+1));dp.set('name',label);dp.set('descr',label)
        parent=run.getparent();at=parent.index(run);d=self.rev('del',parent);parent.remove(d);parent.insert(at,d);parent.remove(run);d.append(run)
        self.rev('ins',p).append(newrun)
        ct=E.fromstring(self.parts['[Content_Types].xml'])
        if not any(n.get('Extension')=='png'for n in ct):
            E.SubElement(ct,'{'+ct.nsmap[None]+'}Default',Extension='png',ContentType='image/png')
            self.parts['[Content_Types].xml']=E.tostring(ct,encoding='UTF-8',xml_declaration=True,standalone=True)
        self.figures.append({'sequence':sequence,'old_rid':oldrid,'new_rid':rid,'new_part':part,'asset':str(asset),'sha256':sha(asset),'extent_preserved':True,'tracking':'old drawing deletion and new sibling insertion; old bytes/relationship retained'})
    def reject_own(self):
        r=deepcopy(self.root)
        # Row and paragraph insertion marks identify complete blocks created by this edit.
        for table in list(r.xpath('.//w:tbl',namespaces=NS)):
            trs=table.findall('w:tr',NS)
            if trs and all(any(v.get(q('id'))in self.ids for v in t.xpath('w:trPr/w:ins',namespaces=NS))for t in trs):table.getparent().remove(table)
        for p in list(r.xpath('.//w:p',namespaces=NS)):
            if any(v.get(q('id'))in self.ids for v in p.xpath('w:pPr/w:rPr/w:ins',namespaces=NS)):
                p.getparent().remove(p)
        for node in list(r.xpath('.//*[@w:id]',namespaces=NS))[::-1]:
            rid=node.get(q('id'))
            if rid not in self.ids or node.getparent()is None:continue
            parent=node.getparent();at=parent.index(node)
            if node.tag==q('del') and parent.tag not in[q('trPr'),q('rPr')]:
                for child in list(node):
                    for t in child.iter(q('delText')):t.tag=q('t')
                    node.remove(child);parent.insert(at,child);at+=1
            parent.remove(node)
            if rid in self.created_containers and len(parent)==0:parent.getparent().remove(parent)
        return r
    def accept_projection(self):
        r=deepcopy(self.root)
        for p in list(r.xpath('.//w:p',namespaces=NS)):
            if any(v.get(q('id'))in self.ids for v in p.xpath('w:pPr/w:rPr/w:del',namespaces=NS)):
                p.getparent().remove(p)
        for row in list(r.xpath('.//w:tr[w:trPr/w:del]',namespaces=NS)):row.getparent().remove(row)
        for table in list(r.xpath('.//w:tbl',namespaces=NS)):
            if not table.findall('w:tr',NS):table.getparent().remove(table)
        for d in list(r.xpath('.//w:del',namespaces=NS)):
            if d.getparent()is not None:d.getparent().remove(d)
        return r
    def finish(self,commit=False):
        rejected=self.reject_own()
        if E.tostring(rejected,method='c14n')!=E.tostring(self.before,method='c14n'):
            (ROOT/(self.code+'-reject.xml')).write_bytes(E.tostring(rejected,pretty_print=True))
            raise AssertionError('reject differs '+self.code)
        assert not any(x.get(q('id'))in self.ids for x in self.root.xpath('.//w:ins[ancestor::w:ins or ancestor::w:del]',namespaces=NS)),'new insertion nested'
        new=[x for x in self.root.xpath('.//*[@w:id]',namespaces=NS)if x.get(q('id'))in self.ids]
        assert all(x.get(q('author'))=='김은수'for x in new)
        self.parts['word/document.xml']=E.tostring(self.root,encoding='UTF-8',xml_declaration=True,standalone=True)
        output=ROOT/'candidate'/self.path.name;output.parent.mkdir(exist_ok=True)
        with zipfile.ZipFile(output,'w')as z:
            for info,data in self.entries:z.writestr(info,self.parts[info.filename])
            for name,data in self.new_parts.items():z.writestr(name,data,compress_type=zipfile.ZIP_DEFLATED)
        with zipfile.ZipFile(output)as z:
            for info,data in self.entries:
                if info.filename not in['word/document.xml','word/_rels/document.xml.rels','[Content_Types].xml']:assert z.read(info.filename)==data
            for info,data in self.entries:
                if info.filename.startswith('word/media/'):assert z.read(info.filename)==data
            for info in z.infolist():
                if info.filename.endswith('.xml')or info.filename.endswith('.rels'):E.fromstring(z.read(info.filename))
        accepted=self.accept_projection();(ROOT/(self.code+'-accepted.xml')).write_bytes(E.tostring(accepted,encoding='UTF-8',pretty_print=True))
        lines=[]
        for i,b in enumerate(accepted.find('w:body',NS),1):
            if b.tag==q('p'):lines.append('['+str(i)+' P] '+visible(b))
            elif b.tag==q('tbl'):
                lines.append('['+str(i)+' TABLE]');lines.extend(' | '.join(visible(c)for c in t.findall('w:tc',NS))for t in b.findall('w:tr',NS))
        (ROOT/(self.code+'-accepted.txt')).write_text('\n'.join(lines),encoding='utf-8')
        report={'code':self.code,'path':str(self.path),'candidate':str(output),'before_sha256':self.before_hash,'after_sha256':sha(output),'date':DATE,'new_revision_count':len(new),'new_revision_ids':sorted(self.ids,key=int),'new_revision_authors':['김은수'],'reject_new_c14n_equals_before':True,'new_nested_insertions':0,'prior_revisions_and_other_parts_preserved':True,'original_media_bytes_preserved':True,'changes':self.changes,'figures':self.figures,'styles_saved':len([v for k,v in self.styles.items()if k.startswith('TDC')]),'native_render':'root serialized render pending','status':'미실행'}
        if commit:
            assert sha(self.path)==self.before_hash,'Concurrent target edit'
            self.path.write_bytes(output.read_bytes());report['committed']=True
        (ROOT/(self.code+'-authored.json')).write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
        return report
