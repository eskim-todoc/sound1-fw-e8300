from pathlib import Path
from PIL import Image
import json, hashlib
import pdfplumber

H = Path(__file__).parent
F = H.parent/'final-word'
previous = json.loads((H/'full-png-reviewed.json').read_text(encoding='utf8'))
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
def body(p):
    im=Image.open(p).convert('RGB')
    return hashlib.sha256(im.crop((0,198,im.width,im.height)).tobytes()).hexdigest()

result = {'scope':'assigned full-page visual review; exact body pixels reconnect prior individually inspected PNGs when only section PAGE/NUMPAGES changes',
          'product_verdict':'미실행 / 판정 대기','pages':{},'PDFs':{}}
direct = {'SDDD':{7,96,99,123,124,125},'SIP':{13,14,15,17}}
required = {'SDDD':sorted({*range(87,119),*range(148,172),7,123,124,125}),'SIP':list(range(1,18))}
for code in ['SDDD','SIP']:
    result['pages'][code] = []
    with pdfplumber.open(F/(code+'.pdf')) as pdf:
        total = len(pdf.pages)
        result['PDFs'][code] = {'path':str(F/(code+'.pdf')),'sha256':sha(F/(code+'.pdf')),'pages':total}
        for n in required[code]:
            path = F/code/('page-'+str(n).zfill(3)+'.png')
            item = {'physical_page':n,'PNG':str(path),'sha256':sha(path),'body_pixels_sha256':body(path),'status':'통과'}
            if n in direct[code]:
                item['inspection'] = 'latest full individual PNG directly opened'
            else:
                oldn = n+1 if code == 'SDDD' and n >= 125 else n
                old = previous['pages'][code][str(oldn)]
                assert old['status']=='통과', (code,n,oldn,'previous not inspected')
                assert old['body_pixels_sha256']==item['body_pixels_sha256'],(code,n,oldn,'body pixels differ')
                item.update({'inspection':'previous full individual PNG review reconnected by exact body pixels',
                             'old_physical_page':oldn,'old_full_PNG_sha256':old['sha256'],'old_archive':old['archive_path'],
                             'body_exact':True})
            expected_page = n-1 if code=='SDDD' else n
            header = pdf.pages[n-1].crop((0,0,pdf.pages[n-1].width,130)).extract_text()or''
            assert f'{expected_page} / {total}'in header,(code,n,header)
            item.update({'displayed_PAGE':expected_page,'NUMPAGES':total,'native_header_PAGE_checked':True})
            result['pages'][code].append(item)
result['status']='통과'
result['assigned_page_count']={c:len(v)for c,v in result['pages'].items()}
result['corrections_rechecked']={'SDDD':'QCC microphone arrow has confirmed call-input label, both DAC references146, OTA explanation+figure34 one page; no tiny font',
                                'SIP':'integration22 adds three actual CFX receive roles; all17pages readable, boot/OTA notice16 and closing17 complete'}
(H/'full-native-final.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
print(json.dumps({'status':'pass','pages':result['assigned_page_count']}))
