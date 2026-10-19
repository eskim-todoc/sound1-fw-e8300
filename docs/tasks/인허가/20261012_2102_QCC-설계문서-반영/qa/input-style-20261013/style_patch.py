"""Formatting only. The caller owns DOCX saving and native validation."""
from copy import deepcopy
from pathlib import Path
from datetime import datetime, timezone
import hashlib, re, zipfile
from lxml import etree as E

W = 'http://schemas.openxmlformats.org/wordprocessingml/2006/main'
NS = {'w': W}
q = lambda name: '{' + W + '}' + name
DOTX = Path(r'E:\workspace\rules-for-codex\지침\양식\Word\TDC 공통 스타일.dotx')

def _visible(e):
    if e.tag in [q('del'), q('moveFrom'), q('pPrChange'), q('rPrChange')]: return ''
    if e.tag == q('tr') and e.find('w:trPr/w:del', NS) is not None: return ''
    if e.tag == q('t'): return e.text or ''
    if e.tag == q('br'): return '\n'
    return ''.join(_visible(c) for c in e)

def _set(parent, name, attrs):
    e = parent.find('w:' + name, NS)
    if e is None:
        e = E.Element(q(name))
        # Keep property change snapshots last and do not touch their contents.
        change = next((c for c in parent if E.QName(c).localname.endswith('Change')), None)
        parent.insert(parent.index(change) if change is not None else len(parent), e)
    e.attrib.clear()
    for k, v in attrs.items(): e.set(q(k), str(v))
    return e

def apply_style(parts: dict, code):
    result = dict(parts)
    doc = E.fromstring(parts['word/document.xml'])
    styles = E.fromstring(parts['word/styles.xml'])
    by_name = {s.find('w:name', NS).get(q('val')): s for s in styles.findall('w:style', NS)
               if s.find('w:name', NS) is not None}
    by_id = {s.get(q('styleId')): s for s in styles.findall('w:style', NS)}
    with zipfile.ZipFile(DOTX) as z:
        template = E.fromstring(z.read('word/styles.xml'))
    tstyles = {s.find('w:name', NS).get(q('val')): s for s in template.findall('w:style', NS)
               if s.find('w:name', NS) is not None}
    required = [n for n in tstyles if n.startswith('TDC ')]
    assert len(required) == 59 and all(n in by_name for n in required), '59 saved TDC styles are required'
    for n in required:
        base = by_name[n].find('w:basedOn', NS)
        assert base is None or base.get(q('val')) in by_id, 'Broken basedOn: ' + n
    # Style definitions and their support-ID/numbering mappings are already saved.
    # This patch changes current paragraph/run overrides only.
    old_props = [E.tostring(e, method='c14n') for e in doc.xpath('.//w:pPrChange|.//w:rPrChange', namespaces=NS)]
    old_text = _visible(doc); old_p_count = len(doc.xpath('//w:p', namespaces=NS))
    edits = []; scope = False; level = 1
    for i, p in enumerate(doc.xpath('//w:body//w:p', namespaces=NS)):
        if any(a.tag in [q('del'), q('moveFrom')] for a in p.iterancestors()): continue
        tr = next((a for a in p.iterancestors() if a.tag == q('tr')), None)
        if tr is not None and tr.find('w:trPr/w:del', NS) is not None: continue
        if any(a.tag == q('sdt') and a.xpath('.//w:docPartObj', namespaces=NS) for a in p.iterancestors()): continue
        text = _visible(p)
        if text.strip() == '서론': scope = True
        if not scope: continue
        pp = p.find('w:pPr', NS)
        if pp is None: pp = E.Element(q('pPr')); p.insert(0, pp)
        style = pp.find('w:pStyle', NS); sid = style.get(q('val')) if style is not None else ''
        current = by_id.get(sid); name = current.find('w:name', NS).get(q('val')) if current is not None else ''
        before = E.tostring(p, method='c14n')
        table = any(a.tag == q('tbl') for a in p.iterancestors())
        restored = None
        if not table and text.strip() == '표준 및 규제': restored = 'TDC 제목 3'
        elif code == 'SGR' and not table and text.strip() == '적용 범위': restored = 'TDC 제목 2'
        if restored:
            name = restored; _set(pp, 'pStyle', {'val': by_name[name].get(q('styleId'))})
            np = pp.find('w:numPr', NS)
            if np is None: np = E.Element(q('numPr')); pp.insert(1, np)
            _set(np, 'ilvl', {'val': int(name[-1]) - 1}); _set(np, 'numId', {'val': 7})
        heading = re.fullmatch(r'TDC 제목 ([1-6])', name)
        body = re.fullmatch(r'TDC 본문 ([1-6])', name)
        if heading:
            level = int(heading.group(1)); target = tstyles[name]
            spacing = target.find('w:pPr/w:spacing', NS)
            _set(pp, 'spacing', {E.QName(k).localname:v for k,v in spacing.attrib.items()})
            _set(pp, 'jc', {'val':'left'}); _set(pp, 'keepNext', {}); _set(pp, 'keepLines', {})
            ind = target.find('w:pPr/w:ind', NS)
            _set(pp, 'ind', {E.QName(k).localname:v for k,v in ind.attrib.items()})
        elif body:
            if code == 'SR' and not table and text.lstrip().startswith('- ') and level == 1:
                name = 'TDC 본문 2'; _set(pp, 'pStyle', {'val':by_name[name].get(q('styleId'))})
            target = tstyles[name]
            _set(pp, 'spacing', {'before':0,'after':160,'line':300,'lineRule':'auto'})
            _set(pp, 'jc', {'val':'left'})
            ind = target.find('w:pPr/w:ind', NS)
            if ind is not None: _set(pp, 'ind', {E.QName(k).localname:v for k,v in ind.attrib.items()})
        elif table and name in ['TDC 표 머리행','TDC 표 본문','TDC 표 항목','TDC 표 설명']:
            target = tstyles[name]
            for prop in ['spacing','jc']:
                e = target.find('w:pPr/w:' + prop, NS)
                if e is not None: _set(pp, prop, {E.QName(k).localname:v for k,v in e.attrib.items()})
            _set(pp, 'ind', {'left':0,'right':0,'hanging':0})
        elif name.startswith('TDC 그림 캡션') or name.startswith('TDC 표 캡션'):
            target = tstyles[name]
        else: continue
        # Current run properties only. Preserve meaningful inline emphasis,
        # superscripts/symbols, equations, images and all past format snapshots.
        whole_body_bold = bool(body and text.strip() and not text.rstrip().endswith('?')
                               and not text.lstrip().startswith(('•','√','- ')))
        active_runs = p.xpath('.//w:r[w:t and not(ancestor::w:del)]', namespaces=NS)
        all_bold = bool(active_runs) and all(r.find('w:rPr/w:b', NS) is not None
                     and r.find('w:rPr/w:b', NS).get(q('val'),'1') not in ['0','false'] for r in active_runs)
        for r in active_runs:
            rt = ''.join(r.xpath('w:t/text()', namespaces=NS))
            if rt.startswith(('[QCC 자료 입력]','[QCC/E8300 자료 입력]','[E8300 보완]')): continue
            rp = r.find('w:rPr', NS)
            if rp is None: rp = E.Element(q('rPr')); r.insert(0, rp)
            if rp.find('w:vertAlign', NS) is not None: continue
            _set(rp, 'rFonts', {'ascii':'맑은 고딕','eastAsia':'맑은 고딕','hAnsi':'맑은 고딕','cs':'맑은 고딕'})
            size = target.find('w:rPr/w:sz', NS)
            _set(rp, 'sz', {'val':size.get(q('val')) if size is not None else 20})
            _set(rp, 'szCs', {'val':size.get(q('val')) if size is not None else 20})
            if heading: _set(rp, 'b', {})
            elif whole_body_bold and all_bold:
                for b in rp.findall('w:b', NS): rp.remove(b)
        if E.tostring(p, method='c14n') != before:
            edits.append({'i':i,'ordinal':i+1,'paraId':p.get('{http://schemas.microsoft.com/office/word/2010/wordml}paraId'),
                          'text':text[:180],'style':name,'heading_restored':restored,
                          'format_mode':'current properties updated; existing format/insertion histories preserved; no nested new format revision'})
    # Delete unnecessary blank paragraph marks through real tracked deletions.
    # Preserve tables' following normal paragraph, section/page breaks, fields,
    # drawings and every pre-existing paragraph deletion.
    blank_changes=[]; started=False
    rid=max([int(x) for x in doc.xpath('.//@w:id',namespaces=NS) if x.isdigit()]+[0])+1
    date=datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace('+00:00','Z')
    for i,p in enumerate(doc.xpath('//w:body//w:p',namespaces=NS)):
        if _visible(p).strip()=='서론': started=True
        if not started or p.getparent().tag != q('body'): continue
        pp=p.find('w:pPr',NS)
        if pp is None or _visible(p).strip(): continue
        if p.xpath('.//w:drawing[not(ancestor::w:del)]|.//w:pict[not(ancestor::w:del)]|.//w:fldChar|.//w:instrText|.//w:br|.//w:sectPr|./w:pPr/w:pageBreakBefore',namespaces=NS):continue
        if pp.find('w:rPr/w:del',NS) is not None:continue
        prev=p.getprevious()
        if prev is not None and prev.tag==q('tbl'):continue
        rp=pp.find('w:rPr',NS)
        if rp is None:rp=E.SubElement(pp,q('rPr'))
        E.SubElement(rp,q('del'),{q('id'):str(rid),q('author'):'김은수',q('date'):date})
        blank_changes.append({'i':i,'revision_id':str(rid)});rid+=1
    assert _visible(doc) == old_text and len(doc.xpath('//w:p', namespaces=NS)) == old_p_count
    assert [E.tostring(e, method='c14n') for e in doc.xpath('.//w:pPrChange|.//w:rPrChange', namespaces=NS)] == old_props
    result['word/document.xml'] = E.tostring(doc, encoding='UTF-8', xml_declaration=True, standalone=True)
    numbering = E.fromstring(parts['word/numbering.xml'])
    num = numbering.xpath('//w:num[@w:numId="7"]', namespaces=NS)[0]
    aid = num.find('w:abstractNumId', NS).get(q('val'))
    abstract = numbering.xpath('//w:abstractNum[@w:abstractNumId="'+aid+'"]', namespaces=NS)[0]
    for lvl in abstract.findall('w:lvl', NS):
        n = int(lvl.get(q('ilvl')))
        if n > 2: continue
        rp = lvl.find('w:rPr', NS)
        if rp is None: rp = E.SubElement(lvl,q('rPr'))
        _set(rp, 'b', {}); _set(rp, 'rFonts', {'ascii':'맑은 고딕','eastAsia':'맑은 고딕','hAnsi':'맑은 고딕','cs':'맑은 고딕'})
        sz = tstyles[f'TDC 제목 {n+1}'].find('w:rPr/w:sz',NS).get(q('val'))
        _set(rp,'sz',{'val':sz});_set(rp,'szCs',{'val':sz})
    result['word/numbering.xml'] = E.tostring(numbering,encoding='UTF-8',xml_declaration=True,standalone=True)
    return result, {'code':code,'edited_paragraphs':edits,'saved_TDC_style_count':59,
                    'styles_xml_byte_unchanged':result['word/styles.xml']==parts['word/styles.xml'],
                    'all_past_pPrChange_rPrChange_byte_semantically_unchanged':True,
                    'paragraph_count_order_and_text_unchanged':True,'numbering_change':'num7 existing levels0..2 glyph font/size/bold only; IDs, starts, numbering text and suffix preserved',
                    'blank_paragraphs_deleted':len(blank_changes),'blank_deletion_revisions':blank_changes,'blank_policy':'Unneeded plain blank paragraph marks deleted with revisions; table-following normal paragraph and structural breaks retained.',
                    'tracking_scope':'Only current formatting properties adjusted; no new format revisions added. Existing snapshots and text revisions retained. Root owns tracked content replacements and final approval of this format scope.'}
