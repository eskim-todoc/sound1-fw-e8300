from pathlib import Path
import json,re,hashlib
import pypdfium2 as pdfium
H=Path(__file__).resolve().parent
V=json.loads((H/'current-values.json').read_text(encoding='utf-8'))
M=json.loads((H.parent/'native-excel/tests-native.json').read_text(encoding='utf-8'))
out=[]
for part in M:
    key=part['code'].split('-')[0]; sn=part['sheet'];ids=[]
    if part['code'].endswith('main'):
        col='D' if key.startswith('SU') else 'C' if key.startswith('SI') else 'B'
        ids=sorted((int(a[1:]),s.strip()) for a,s in V[key][sn].items() if a.startswith(col) and a[1:].isdigit() and s.strip().startswith('SW-FW-'))
    document=pdfium.PdfDocument(part['pdf']);texts=[];rawtexts=[]
    for i in range(len(document)):
        page=document[i];t=page.get_textpage();text=t.get_text_range();rawtexts.append(text);texts.append(re.sub(r'\s+','',text));t.close();page.close()
    positions=[]
    for row,label in ids:
        normalized=re.sub(r'\s+','',label)
        # Keep the last digit boundary in the actual PDF text. Removing line
        # breaks first could join ID-16 to the next field's "100 Hz".
        pattern=r'\s*'.join(re.escape(c) for c in normalized)+r'(?![0-9])'
        hits=[i+1 for i,t in enumerate(rawtexts) if re.search(pattern,t)]
        positions.append({'id':label,'start_row':row,'literal_text_pages':hits})
    out.append({**part,'pdf_sha256':hashlib.sha256(Path(part['pdf']).read_bytes()).hexdigest(),'pages':len(texts),
      'id_positions':positions,'missing_literal_id_labels':[x for x in positions if not x['literal_text_pages']],
      'sparse_page_candidates':[{'page':i+1,'text_characters':len(t),'text':t} for i,t in enumerate(texts) if len(t)<240],
      'scope':'Supporting native PDF text positions, not visible glyph proof. Sparse pages require actual pixel review; retained long merged fields may have identifiers on another page.'})
    document.close()
(H/'native-id-positions-final.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps([{'code':x['code'],'pages':x['pages'],'IDs':len(x['id_positions']),'missing_literal_IDs':len(x['missing_literal_id_labels']),'sparse_pages':[a['page'] for a in x['sparse_page_candidates']]} for x in out]))
