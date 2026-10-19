from pathlib import Path
import json,re
from editor import ROOT,sha
from followup import UNITS,SUPPORT

def choose(book,lo,hi):
    for name,cells in book.items():
        if any(re.fullmatch(r'[A-Z]+\d+',k)and lo<=int(re.search(r'\d+$',k).group())<=hi and v.strip()for k,v in cells.items()):return name,cells
    raise AssertionError('sheet missing')
def main():
    tests=ROOT.parent/'qcc_tests/current-values.json';tv=json.loads(tests.read_text(encoding='utf-8'))
    requirement=ROOT.parent/'qcc_requirements/requirements-contract.json';rv=json.loads(requirement.read_text(encoding='utf-8'))
    rs={x['rid']:x for x in rv['requirements']};assert 82 not in rs and len(rs)==82
    assert rs[69]['H'].strip()=='SYS-RID-SP-FW-15'
    assert all(x in rs[83]['H']for x in ['SYS-RID-ACDOC-17','SYS-RID-ACDOC-18','SYS-RID-ACDOC-20'])
    sheet,cells=choose(tv['SUVC'],1727,1764);unitrows=[]
    for n in range(1727,1765):
        testid=cells.get('D'+str(n),'').strip()
        if not testid:continue
        refs=re.findall(r'SU-ID-QCC-\d+',cells.get('G'+str(n),''));assert refs
        for ref in refs:
            unit=UNITS[int(ref.rsplit('-',1)[1])-1]
            assert testid in [t['id']for t in unit['tests']]
        unitrows.append({'test_id':testid,'sheet':sheet,'row':n,'purpose_cell':'E'+str(n),'unit_cell':'G'+str(n),'units':refs,'purpose':cells.get('E'+str(n),''),'scope':'actual conditions K1727..1764 compared to design purpose/input/output/process; supports are not full-system pass'})
    assert len(unitrows)==15
    si,cc=choose(tv['SITC'],131,144);integration=[]
    for n in range(131,145):
        testid=cc.get('C'+str(n),'').strip()
        if testid:integration.append({'test_id':testid,'sheet':si,'row':n,'description_cell':'E'+str(n),'trace_cell':'F'+str(n),'criteria_column':'M','purpose':cc.get('E'+str(n),''),'unit_and_requirement_trace':cc.get('F'+str(n),'')})
    assert [v['test_id']for v in integration]==['SW-FW-IntegrationTest-ID-'+str(n).zfill(3)for n in range(18,25)]
    pending=[]
    for code,lo,hi in [('SUVR',1727,1764),('SITR',131,144)]:
        sn,cs=choose(tv[code],lo,hi)
        rows=[]
        for n in range(lo,hi+1):
            values=[v for k,v in cs.items()if k.endswith(str(n))and int(re.search(r'\d+$',k).group())==n]
            text=' '.join(values)
            assert '미실행'in text and ('판정 대기'in text or '판정대기'in text),(code,n)
            rows.append(n)
        pending.append({'document':code,'sheet':sn,'rows':rows,'unexecuted_and_pending_preserved':True})
    findings={'unit6':'production Standalone1/TWS2/one device alone; normal immutable composition matches Q003',
      'unit8':'main004/SI001 and support008/009/SI003 activation boundary; identity/RF and stimulation pass separate',
      'audio':'RID69/FW15 downlink Classic→I2S→E8300 and DMIC2→QCC→phone call uplink separate; SYS82 is a test ID',
      'SPI':'logical type+payload<=20byte and separate physical length+20byte=21; response index20 length storage distinct',
      'authentication':'12byte AUTH transport patterns and status30 distinct from nonce16byte/real status0..3/CRA',
      'timers':'30sec auth/180sec noTXRX/5 failures→60sec limitation distinct; reset/tolerance unresolved',
      'keys':'installed shared_service/clinician_master distinct from derived clinic_service; SHA256 key derivation vs AESCMAC authenticator; vectors/bootstrap pending',
      'remaining':'No conflict requiring tests-owned edits found after support trace followup; no physical QCC/product trial performed.'}
    result={'status':'통과','scope':'read-only semantic cross-review of source-current extracted values; native final rendering is separate',
      'input_sha256':{'requirements_contract':sha(requirement),'test_current_values':sha(tests)},'unit_tests':unitrows,'integration_tests':integration,'report_pending':pending,'findings':findings,
      'support_trace_reinspection':'followup/qa.json','product_verdict':'판정대기'}
    (ROOT/'cross-doc.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');print('15 unit groups/38 conditions and 7 integration groups/14 conditions semantic cross-review PASS; results remain unexecuted.')
if __name__=='__main__':main()
