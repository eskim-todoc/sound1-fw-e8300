from pathlib import Path
from lxml import etree as E
from collections import Counter
import zipfile, hashlib, json, datetime, copy

H = Path(__file__).resolve().parent
Q = H.parent / 'qcc_design/native-trace-probe-v2'
W = 'http://schemas.openxmlformats.org/wordprocessingml/2006/main'
PKG = 'http://schemas.microsoft.com/office/2006/xmlPackage'
NEW = 'QCCNativeParagraphProbeV2_20261013'
N = {'w': W, 'pkg': PKG}

def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def flat(p, part='/word/document.xml'):
    return E.parse(str(p)).getroot().xpath('pkg:part[@pkg:name=$part]/pkg:xmlData/*', namespaces=N, part=part)[0]
def structural(e):
    if e is None: return None
    return (e.tag, tuple(sorted(e.attrib.items())), e.text or '', tuple(structural(x) for x in e))
def props(e, strip_mark_revisions=False, ignore_revision_ids=False):
    x = copy.deepcopy(e)
    if x is None: return None
    for c in x.findall('{'+W+'}pPrChange'): x.remove(c)
    if ignore_revision_ids:
        for r in x.findall('{'+W+'}rPr'):
            for child in r:
                if child.tag in {'{'+W+'}ins', '{'+W+'}del'}: child.attrib.pop('{'+W+'}id', None)
    if strip_mark_revisions:
        for r in x.findall('{'+W+'}rPr'):
            for child in list(r):
                if child.tag in {'{'+W+'}ins', '{'+W+'}del'}: r.remove(child)
            if not len(r) and not r.attrib and not (r.text or '').strip(): x.remove(r)
    return structural(x)
def new_changes(r):
    return Counter(E.QName(x).localname for x in r.iter() if x.get('{'+W+'}author') == NEW)
def mark_deletions(r, ignore_id=False):
    out = []
    for n, p in enumerate(r.findall('.//{'+W+'}p'), 1):
        for e in p.xpath('./w:pPr/w:rPr/w:del[@w:author=$author]', namespaces=N, author=NEW):
            out.append((n, tuple(sorted((k,v) for k,v in e.attrib.items() if not ignore_id or k != '{'+W+'}id'))))
    return out
def old_revision_text(r):
    out = Counter()
    for e in r.iter():
        author = e.get('{'+W+'}author')
        if not author or author == NEW: continue
        text = ''.join(e.itertext())
        if text:
            out[(E.QName(e).localname, author, e.get('{'+W+'}date'), text)] += 1
    return out

raw_path = Q / 'SAD-native-probe-author.docx'
before_path = Q / 'candidate-history.xml'
opened_path = Q / 'reject-allmarkup/initial-history.xml'
detail_path = Q / 'reject-allmarkup/open-normalization-format-detail.json'
paths = [raw_path, before_path, opened_path, detail_path]
before_hash = {str(p): sha(p) for p in paths}
with zipfile.ZipFile(raw_path) as z:
    raw = E.fromstring(z.read('word/document.xml'))
    raw_styles = E.fromstring(z.read('word/styles.xml'))
before, opened = flat(before_path), flat(opened_path)
detail = json.loads(detail_path.read_text(encoding='utf-8'))
rp, bp, op = [r.findall('.//{'+W+'}p') for r in [raw, before, opened]]
assert len(rp) == len(bp) == len(op) == 1435
actual_missing = []
for n, (a, b) in enumerate(zip(bp, op), 1):
    old = a.xpath('./w:pPr/w:pPrChange[@w:author=$author]', namespaces=N, author=NEW)
    current = b.xpath('./w:pPr/w:pPrChange[@w:author=$author]', namespaces=N, author=NEW)
    if old and not current: actual_missing.append(n)
assert actual_missing == [r['paragraph_ordinal'] for r in detail['rows']] and len(actual_missing) == 125
checks = []
for n in actual_missing:
    raw_ppr = rp[n-1].find('{'+W+'}pPr')
    before_ppr = bp[n-1].find('{'+W+'}pPr')
    opened_ppr = op[n-1].find('{'+W+'}pPr')
    change = before_ppr.xpath('w:pPrChange[@w:author=$author]', namespaces=N, author=NEW)[0]
    old_ppr = change.find('{'+W+'}pPr')
    checks.append({'paragraph_ordinal': n,
        'raw_vs_saved_current_properties_equal': props(raw_ppr) == props(before_ppr),
        'saved_vs_reopened_current_properties_equal_excluding_revision_ids_only': props(before_ppr, ignore_revision_ids=True) == props(opened_ppr, ignore_revision_ids=True),
        'revision_ids_renumbered': props(before_ppr) != props(opened_ppr),
        'stored_old_vs_current_format_equal_excluding_only_paragraph_mark_ins_del': props(old_ppr, True) == props(before_ppr, True) == props(opened_ppr, True)})
assert all(all(v for k,v in r.items() if k not in {'paragraph_ordinal','revision_ids_renumbered'}) for r in checks)
counts = {'raw': dict(new_changes(raw)), 'saved_native_xml': dict(new_changes(before)), 'reopened_native_xml': dict(new_changes(opened))}
assert counts == {'raw': {'del': 183, 'pPrChange': 177}, 'saved_native_xml': {'del': 183, 'pPrChange': 177}, 'reopened_native_xml': {'del': 183, 'pPrChange': 52}}
marks = [mark_deletions(r, True) for r in [raw, before, opened]]
assert len(marks[0]) == 183 and marks[0] == marks[1] == marks[2]
oldtexts = [old_revision_text(r) for r in [raw, before, opened]]
assert oldtexts[0] == oldtexts[1] == oldtexts[2]
styles_before, styles_opened = flat(before_path, '/word/styles.xml'), flat(opened_path, '/word/styles.xml')
assert structural(styles_before) == structural(styles_opened)
old_missing = []
saved_old_ids = {x.get('{'+W+'}id') for x in before.findall('.//{'+W+'}pPrChange') if x.get('{'+W+'}author') != NEW}
for n,p in enumerate(rp,1):
    for x in p.xpath('./w:pPr/w:pPrChange[@w:author!=$author]', namespaces=N, author=NEW):
        if x.get('{'+W+'}id') not in saved_old_ids:
            old_missing.append({'paragraph_ordinal':n,'id':x.get('{'+W+'}id'),'author':x.get('{'+W+'}author'),'date':x.get('{'+W+'}date'), 'text_length':len(''.join(x.itertext()))})
assert old_missing == [{'paragraph_ordinal':1435,'id':'5214','author':'김은수','date':'2026-10-01T06:57:00Z','text_length':0}]
after_hash = {str(p): sha(p) for p in paths}
assert before_hash == after_hash
out = {'checkedAt':datetime.datetime.now().astimezone().isoformat(), 'status':'PASS_SCOPED',
    'scope':'Independent read-only verification of the 125 omitted new pPrChange snapshots. This does not establish native new-only rejection, accepted cell preservation, final G publication or full visual PASS.',
    'inputs_before_sha256':before_hash,'inputs_after_sha256':after_hash,'readonly_unchanged':True,
    'new_revision_counts':counts,'paragraph_count_each':1435,
    'missing125_ordinals_confirmed_against_design_diff':True,
    'effective_property_verification':{'checked_count':125,'all_raw_saved_current_equal':True,'all_saved_reopened_current_equal':True,
        'all_stored_old_current_equal_excluding_only_paragraph_mark_ins_del':True,
        'revision_ID_renumbered_count':sum(r['revision_ids_renumbered'] for r in checks),
        'method':'Namespace-qualified element names and all property attributes/values/children compared recursively. Ignore only pPrChange wrappers and revision w:id renumbering for saved/reopen; for old/current format equivalence ignore paragraph-mark ins/del plus a resulting empty rPr. No font/spacing/indent/style/numbering or pagination property is ignored.',
        'rows':checks},
    'new183_paragraph_mark_deletions':{'all_present':True,'same_paragraph_ordinals_author_date_dateUtc':True,'revision_ids_renumbered_count':sum(a!=b for a,b in zip(mark_deletions(before),mark_deletions(opened))),'count':183},
    'historical_nonempty_revision_text':{'raw_saved_reopened_exact':True,'signature_count':sum(oldtexts[0].values()),'same_tag_author_date_text_multiplicity':True},
    'styles_saved_vs_reopened_structurally_equal':True,
    'additional_raw_to_saved_native_history_normalization':{'missing_empty_historical_format_node':old_missing,
        'limitation':'One pre-existing empty-text pPrChange snapshot is absent already in saved Content.WordOpenXML, before the new125 omission on reopen. Historical ordinary revision text equality does not assert every historical format revision node is identical. This node is outside the new125 scoped equivalence finding.'},
    'interpretation':'All 125 omitted new snapshots are redundant for current format after paragraph-mark revision metadata is removed. The current properties and all183 paragraph-mark deletion revisions are retained. A native new-only reject test still needs its own baseline comparison and is not inferred from this result.'}
p = H / 'SAD-native-normalization-independent.json'
p.write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'path':str(p),'sha256':sha(p),'status':out['status'],'redundant125':True,'del183':True,'historical_text':True,'extra_old_empty_format':old_missing},ensure_ascii=False))
