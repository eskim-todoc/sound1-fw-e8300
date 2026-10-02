from build_architecture import *
for f in STAGE.glob('*.docx'):
 with zipfile.ZipFile(f) as z:parts={n:z.read(n) for n in z.namelist()}
 for name,data in list(parts.items()):
    if not name.startswith('word/') or not name.endswith('.xml'):continue
    root=E.fromstring(data);changed=False
    for p in root.xpath('//w:p',namespaces=N):
        t=txt(p)
        if 'TD24A-A-EXT/TD24A-A-EXT' in t:settext(p,t.replace('TD24A-A-EXT/TD24A-A-EXT','TD24A-A-EXT'));changed=True
        if name.startswith('word/header') and re.fullmatch(r'20\d{2}[./-]\d{1,2}[./-]\d{1,2}',t.strip()):settext(p,'');changed=True
    if name.startswith('word/header'):
        for row in root.xpath('//w:tr',namespaces=N):
            cells=row.findall(q('tc'))
            for i,c in enumerate(cells[:-1]):
                if '개정번호' in txt(c).replace(' ',''):celltext(cells[i+1],'0');changed=True
    if changed:parts[name]=E.tostring(root,xml_declaration=True,encoding='UTF-8',standalone=True)
 with zipfile.ZipFile(f,'w',zipfile.ZIP_DEFLATED) as z:
    for n,b in parts.items():z.writestr(n,b)
 print(f.name)
