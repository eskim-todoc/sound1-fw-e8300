"""Read-only proof aggregator. It never renders or edits an Office document.

Optional final SAD input: --sad-native <native metadata JSON> --sad-proof <JSON>
(repeat --sad-proof for split reviewers). A SAD proof uses sourceSHA256 and
pages: [{page, png, sha256, direct_original_size_viewed: true, verdict: '통과'}].
The native JSON uses the same code/path/pdf/pages/sourceSHA256/sourceUnchanged
schema as the root's Office exports. Missing SAD evidence leaves PENDING.
"""
from pathlib import Path
import argparse, datetime, hashlib, json, re
from PIL import Image
from pypdf import PdfReader

HERE = Path(__file__).resolve().parent
QA = HERE.parent
REPO = next(p for p in HERE.parents if (p / 'AGENTS.md').is_file())
PASS = {'통과', 'PASS'}

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()

def load(path):
    return json.loads(Path(path).read_text(encoding='utf-8'))

def resolve(path):
    p = Path(path)
    return p if p.is_absolute() else REPO / p

def demand(condition, message):
    if not condition:
        raise ValueError(message)

def check_file(path, expected):
    p = resolve(path)
    demand(p.is_file(), f'Missing file: {p}')
    actual = sha(p)
    demand(actual == expected.upper(), f'Hash mismatch: {p}: {actual} != {expected}')
    return p

def pixel(path, crop_top=0):
    with Image.open(path) as im:
        demand(im.width == 893 and im.height == 1263, f'Unexpected full page size: {path}')
        body = im.convert('RGB').crop((0, crop_top, im.width, im.height))
        return hashlib.sha256(body.tobytes()).hexdigest().upper()

def record_metadata(path, code):
    data = load(path)
    items = data if isinstance(data, list) else [data]
    selected = [x for x in items if x.get('code') == code]
    demand(len(selected) == 1, f'Expected one {code} native record: {path}')
    return selected[0]

def native_context(code, metadata_path):
    meta = record_metadata(metadata_path, code)
    demand(meta.get('sourceUnchanged') is True, f'{code}: native source unchanged not proved')
    source = check_file(meta['path'], meta['sourceSHA256'])
    folder = QA / 'final-word' / code
    pages_path = folder / 'pages.json'
    pages = load(pages_path)
    count = meta['pages']
    demand(isinstance(count, int) and count > 0, f'{code}: invalid page count')
    demand([x['page'] for x in pages] == list(range(1, count + 1)), f'{code}: stale/missing pages.json range')
    pdf = resolve(meta['pdf'])
    demand(len(PdfReader(pdf).pages) == count, f'{code}: PDF/page metadata mismatch')
    pngs = {}
    for n in range(1, count + 1):
        p = folder / f'page-{n:03}.png'
        demand(p.is_file(), f'{code}: missing current PNG {n}')
        with Image.open(p) as im:
            demand(im.size == (893, 1263), f'{code}/{n}: not a full-size PNG')
        pngs[n] = {'png': str(p), 'sha256': sha(p)}
    return {'code': code, 'source': str(source), 'sourceSHA256_before': sha(source),
        'native': str(metadata_path), 'nativeSHA256': sha(metadata_path), 'native_record': meta,
        'PDF': str(pdf), 'PDF_SHA256': sha(pdf), 'pages_json': str(pages_path),
        'pages_jsonSHA256': sha(pages_path), 'actual_page_count': count,
        'current_pngs': pngs, 'proofs': {}, 'errors': []}

def add(ctx, n, proof, expected, png):
    demand(n in ctx['current_pngs'], f"{ctx['code']}: proof page {n} is out of range")
    actual = ctx['current_pngs'][n]
    demand(resolve(png) == Path(actual['png']), f"{ctx['code']}/{n}: proof points to another render")
    demand(expected.upper() == actual['sha256'], f"{ctx['code']}/{n}: stale proof PNG hash")
    ctx['proofs'].setdefault(n, []).append(proof)

def requirements_word(ctx, proof_path):
    data = load(proof_path)['Word'][ctx['code']]
    demand(data['sha256'] == ctx['sourceSHA256_before'], f"{ctx['code']}: stale requirements Word proof")
    follow = data.get('latest_figure_only_reinspection')
    rows = follow['pages'] if follow else data['pages']
    if follow:
        demand(follow['readonly_unchanged'] and follow['verdict'] in PASS, 'SR latest reinspection is not PASS')
        demand(follow['sourceSHA256_before'] == ctx['sourceSHA256_before'] == follow['sourceSHA256_after'], 'SR latest source mismatch')
    for row in rows:
        verdict = row.get('verdict', row.get('direct_original_size_review'))
        demand(verdict in PASS, f"{ctx['code']}/{row['page']}: review not PASS")
        if row.get('same_pixels_as_reviewed_layout_candidate'):
            old = QA / 'final-word' / f"{ctx['code']}-layout" / f"page-{row['page']:03}.png"
            demand(pixel(old) == pixel(row['png']), f"{ctx['code']}/{row['page']}: layout pixels differ")
            method = 'previous individual full page review and independently rechecked full-pixel equality'
        else:
            demand(follow and row['page'] == follow['modified_page'] and '직접' in row['verification'], 'No latest direct full-page proof')
            method = 'latest direct original-size full page review'
        add(ctx, row['page'], {'evidence': str(proof_path), 'evidenceSHA256': sha(proof_path), 'method': method}, row['sha256'], row['png'])

def requirements_sddd(ctx, proof_path):
    data = load(proof_path)
    latest = data['latest_native_reinspection']
    demand(latest['unchanged'] and latest['verdict'] in PASS, 'Requirements SDDD latest review is not PASS')
    demand(latest['sourceSHA256_before'] == ctx['sourceSHA256_before'] == latest['sourceSHA256_after'], 'Requirements SDDD stale source')
    originals = {x['page']: x for x in data['reviewed']}
    direct = {x['page']: x for x in latest['current_direct_reinspections']}
    for n, row in direct.items():
        demand(row['direct_original_size_viewed'] and row['verdict'] in PASS, f'SDDD/{n}: direct reinspection not PASS')
        add(ctx, n, {'evidence': str(proof_path), 'evidenceSHA256': sha(proof_path), 'method': 'latest direct original-size full PNG review'}, row['sha256'], row['png'])
    mapped = set()
    for row in latest['mapping']:
        n = row['current_page']; mapped.add(n)
        demand(row['layout_verdict'] in PASS, f'SDDD/{n}: mapped review not PASS')
        original = originals[row['old_page']]
        demand('individual' in original['method'], 'Prior SDDD proof is not individual full page review')
        old = check_file(row['old_reviewed_png'], row['old_pngSHA256'])
        demand(row['old_pngSHA256'] == original['sha256'], 'Prior SDDD archive does not match original review')
        if row['body_pixel_same']:
            demand(original['layout_verdict'] in PASS, f'SDDD/{n}: failed historical page cannot be reused without direct reinspection')
            old_pix = pixel(old, 200); new_pix = pixel(row['current_png'], 200)
            demand(old_pix == row['old_body_pixelSHA256'] == new_pix == row['current_body_pixelSHA256'], f'SDDD/{n}: body pixel proof mismatch')
            add(ctx, n, {'evidence': str(proof_path), 'evidenceSHA256': sha(proof_path), 'method': 'previous individual full review plus exact current body pixels',
                'old_page': row['old_page'], 'oldPNG': str(old), 'oldPNG_SHA256': sha(old), 'crop_top': 200, 'bodyPixelSHA256': new_pix}, row['current_pngSHA256'], row['current_png'])
        else:
            demand(n in direct, f'SDDD/{n}: changed body has no latest direct review')
    demand(len(mapped) == latest['current_unique_scope_count'] == 115, 'Requirements SDDD unique mapped scope mismatch')
    ctx['requirements_unique_mapped_pages'] = len(mapped)

def design_word(ctx, proof_path):
    data = load(proof_path)
    pdf = data['PDFs'][ctx['code']]
    demand(pdf['pages'] == ctx['actual_page_count'] and pdf['sha256'].upper() == ctx['PDF_SHA256'], f"{ctx['code']}: stale design PDF proof")
    rows = data['pages'][ctx['code']]
    for row in rows:
        n = row['physical_page']
        demand(row['status'] in PASS and row['native_header_PAGE_checked'], f"{ctx['code']}/{n}: non-PASS page/header review")
        current_pix = pixel(row['PNG'], 198)
        demand(current_pix == row['body_pixels_sha256'].upper(), f"{ctx['code']}/{n}: current body pixels mismatch")
        method = row['inspection']
        if row.get('body_exact'):
            old = check_file(row['old_archive'], row['old_full_PNG_sha256'])
            demand(pixel(old, 198) == current_pix, f"{ctx['code']}/{n}: reused body is not pixel exact")
            demand('full individual' in method, f"{ctx['code']}/{n}: reuse not linked to individual full review")
        else:
            demand('full individual' in method and 'directly opened' in method, f"{ctx['code']}/{n}: no direct full-page proof")
        add(ctx, n, {'evidence': str(proof_path), 'evidenceSHA256': sha(proof_path), 'method': method,
            'crop_top': 198, 'bodyPixelSHA256': current_pix, 'native_header_PAGE_checked': True}, row['sha256'], row['PNG'])
    ctx['design_review_count'] = len(rows)

def sad_word(ctx, proof_paths):
    demand(proof_paths, 'SAD final full-page evidence not provided')
    for path in proof_paths:
        data = load(path)
        demand(data['sourceSHA256'].upper() == ctx['sourceSHA256_before'], 'SAD proof source is stale')
        rows = data['pages']
        demand(isinstance(rows, list), 'SAD proof pages must use documented list schema')
        for row in rows:
            demand(row['direct_original_size_viewed'] is True and row['verdict'] in PASS, 'SAD page not a direct full-size PASS review')
            add(ctx, row['page'], {'evidence': str(path), 'evidenceSHA256': sha(path), 'method': 'final SAD direct original-size full PNG review'}, row['sha256'], row['png'])

def finish(ctx):
    covered = sorted(ctx['proofs'])
    expected = list(range(1, ctx['actual_page_count'] + 1))
    ctx['covered_page_count'] = len(covered)
    ctx['covered_pages'] = covered
    ctx['missing_pages'] = sorted(set(expected) - set(covered))
    ctx['sourceSHA256_after'] = sha(ctx['source'])
    demand(ctx['sourceSHA256_after'] == ctx['sourceSHA256_before'], f"{ctx['code']}: G changed during verification")
    demand(not ctx['missing_pages'], f"{ctx['code']}: uncovered pages {ctx['missing_pages']}")
    ctx['status'] = 'PASS'

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sad-native', type=Path)
    parser.add_argument('--sad-proof', type=Path, action='append', default=[])
    args = parser.parse_args()
    req = HERE / 'native-final-qa.json'
    sd = HERE / 'sddd-first86-native-qa.json'
    design = QA / 'qcc_design/full-native-final.json'
    result = {'checkedAt': datetime.datetime.now().astimezone().isoformat(), 'status': 'PENDING',
        'scope': 'Current saved five Word documents and all actual final rendered full pages. No product test or new approval claim.',
        'documents': {}, 'errors': [], 'pending': [], 'current_hashes': {}}
    setup = {'SR': QA / 'final-word/requirements-native.json', 'SGR': QA / 'final-word/requirements-native.json',
        'SDDD': QA / 'final-word/SDDD-final-native.json', 'SIP': QA / 'final-word/design-native.json'}
    source_paths = {}
    for code, meta in {**setup, 'SAD': args.sad_native or QA / 'final-word/design-native.json'}.items():
        source_paths[code] = resolve(record_metadata(meta, code)['path'])
        result['current_hashes'][code] = sha(source_paths[code])
    for code, meta in setup.items():
        ctx = {'code': code, 'status': 'FAIL'}
        try:
            ctx = native_context(code, meta)
            if code in {'SR', 'SGR'}:
                requirements_word(ctx, req)
            if code == 'SDDD':
                requirements_sddd(ctx, sd)
                design_word(ctx, design)
                demand(ctx['design_review_count'] == 60, 'Design SDDD review count is not 60')
            if code == 'SIP':
                design_word(ctx, design)
                demand(ctx['design_review_count'] == 17, 'Design SIP review count is not 17')
            finish(ctx)
        except Exception as e:
            ctx['status'] = 'FAIL'; ctx['error'] = str(e)
            result['errors'].append({'code': code, 'message': str(e)})
        result['documents'][code] = ctx
    if args.sad_native:
        ctx = {'code': 'SAD', 'status': 'FAIL'}
        try:
            ctx = native_context('SAD', args.sad_native)
            sad_word(ctx, args.sad_proof)
            finish(ctx)
        except Exception as e:
            ctx['status'] = 'FAIL'; ctx['error'] = str(e)
            result['errors'].append({'code': 'SAD', 'message': str(e)})
        result['documents']['SAD'] = ctx
    else:
        rollback = QA / 'SAD-native-rollback.json'
        result['documents']['SAD'] = {'code': 'SAD', 'status': 'PENDING',
            'reason': 'Final SAD native/full-page proof inputs have not been provided to this aggregation run. Historical failed-candidate evidence is retained separately and is never counted as current visual PASS.',
            'rollback_evidence': str(rollback), 'rollback_evidenceSHA256': sha(rollback) if rollback.exists() else None,
            'required_input': '--sad-native final-native.json --sad-proof reviewer1.json [--sad-proof reviewer2.json ...]'}
        result['pending'].append('SAD final native/current hash and all individual full-page proofs')
    result['current_hashes_after'] = {k: sha(p) for k, p in source_paths.items()}
    result['all_five_readonly_unchanged'] = result['current_hashes'] == result['current_hashes_after']
    if not result['all_five_readonly_unchanged']:
        result['errors'].append({'code': 'ALL', 'message': 'Current G source changed during coverage verification'})
    result['status'] = 'FAIL' if result['errors'] else ('PENDING' if result['pending'] else 'PASS')
    result['fail_closed'] = 'PASS requires five current G/native hashes, exact current PNG hashes, all physical pages covered by full-page review or independently rechecked body pixel equality. Contact thumbnails do not satisfy coverage.'
    output = HERE / 'word-visual-coverage.json'
    output.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'path': str(output), 'sha256': sha(output), 'status': result['status'],
        'documents': {k: {'status': v['status'], 'covered': v.get('covered_page_count'), 'pages': v.get('actual_page_count'), 'error': v.get('error')} for k, v in result['documents'].items()},
        'pending': result['pending']}, ensure_ascii=False))
    return 0 if result['status'] == 'PASS' else (2 if result['status'] == 'PENDING' else 1)

if __name__ == '__main__':
    raise SystemExit(main())
