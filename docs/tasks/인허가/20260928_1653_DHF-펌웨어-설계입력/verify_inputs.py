from build_inputs import *
import pypdfium2 as pdfium
from PIL import Image,ImageDraw
QA=TASK/'qa';checks={}
checks['source_unchanged']=all(hashlib.sha256((SRC/p).read_bytes()).hexdigest()==v['sha256'] for p,v in base.items())
checks['reference_copies']=all(hashlib.sha256((OUT/p).read_bytes()).hexdigest()==v['sha256'] for p,v in base.items() if p.startswith('시스템 Data/'))
jobs=json.loads((TASK/'workbook-plan.json').read_text(encoding='utf-8'))
for job in jobs:
    path=OUT/job['name'];wb=openpyxl.load_workbook(path);sh=wb.worksheets[1]
    vals=[str(c.value) for row in sh for c in row if c.value is not None]
    ids=set(int(n) for v in vals for n in re.findall(r'SYS-RID-FW-(\d+)',v))
    checks[job['kind']+'_fw_coverage']=sorted(ids)==list(range(1,51))
    checks[job['kind']+'_bad_formulas']=[c.coordinate for row in sh for c in row if c.data_type=='f' and ('#REF!' in c.value or '[1]' in c.value)]
    before=E.fromstring(readzip(job['source'])['xl/styles.xml']);after=E.fromstring(readzip(path)['xl/styles.xml'])
    checks[job['kind']+'_original_styles_preserved']=all(E.tostring(a)==E.tostring(b) for a,b in zip(before.find(f'{{{S}}}cellXfs'),after.find(f'{{{S}}}cellXfs')))
    checks[job['kind']+'_cover']=wb.worksheets[0]['H12'].value=='김은수' and wb.worksheets[0]['L3'].value=='Rev. 0'
    if job['kind']=='SRS':
        data=openpyxl.load_workbook(path,data_only=True).worksheets[1]
        checks['srs_formula_values']=[data[f'G{r}'].value for r in range(6,92) if data[f'F{r}'].value]
    else:
        data=openpyxl.load_workbook(path,data_only=True).worksheets[1]
        checks['sstc_tests']=sum(bool(data[f'C{r}'].value) for r in range(25,105))
        checks['sstc_references']=all(re.fullmatch(r'SW-FW-RID-\d+',str(data[f'E{r}'].value or '')) for r in range(25,105))
for path in OUT.glob('*.docx'):
    source=SRC/path.name.replace('TD24A','TD20B')
    final=readzip(path);root=E.fromstring(final['word/document.xml'])
    ins=len(root.xpath('//w:ins',namespaces=NS));dels=len(root.xpath('//w:del',namespaces=NS))
    checks[path.name]={'insertions':ins,'deletions':dels,'tracking':b'trackRevisions' in final['word/settings.xml']}
    def plain(p):
        z=readzip(p);r=E.fromstring(z['word/document.xml'])
        # Compare source body, ignoring field-driven TOC page-number refresh.
        return [txt(n).strip() for n in r.xpath('//w:p',namespaces=NS) if not n.xpath('ancestor::w:sdt|.//w:hyperlink|.//w:instrText',namespaces=NS) and txt(n).strip()]
    old=plain(source);rev=plain(QA/('rejected-'+path.name))
    checks[path.name]['reject_restores_body']=old==rev
    if old!=rev:
        import difflib
        (QA/(path.name+'.reject-diff.txt')).write_text('\n'.join(difflib.unified_diff(old,rev)),encoding='utf-8')
    stage=readzip(STAGE/path.name)
    checks[path.name]['template_style_parts_unchanged']=all(stage.get(p)==readzip(source).get(p) for p in ('word/styles.xml','word/numbering.xml','word/theme/theme1.xml'))
for pdf in QA.glob('*.pdf'):
    folder=QA/(pdf.stem+'-pages');folder.mkdir(exist_ok=True)
    with pdfium.PdfDocument(pdf) as doc:
        thumbs=[]
        for i,p in enumerate(doc):
            im=p.render(scale=1.25).to_pil().convert('RGB');im.save(folder/f'{i+1:02}.png')
            im.thumbnail((580,800));thumbs.append(im)
        for start in range(0,len(thumbs),6):
            sheet=Image.new('RGB',(1770,1680),'#dddddd');d=ImageDraw.Draw(sheet)
            for j,im in enumerate(thumbs[start:start+6]):
                xx=j%3*590;yy=j//3*840;d.text((xx+8,yy+5),str(start+j+1),fill='black');sheet.paste(im,(xx+5,yy+25))
            sheet.save(folder/f'contact-{start//6+1}.png')
        checks[pdf.name]={'pages':len(doc)}
(QA/'checks.json').write_text(json.dumps(checks,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({k:v for k,v in checks.items() if k!='srs_formula_values'},ensure_ascii=False,indent=2))
