from pathlib import Path
import zipfile, json, hashlib, re
from lxml import etree as E
from final_layout_patch import BASE, W, N, Q, P, visible, dead
ROOT=Path(__file__).parent
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def ids(text,prefix):
    out=set()
    for m in re.finditer(re.escape(prefix)+r'(\d+(?:/\d+)*)',text):
        out.update(prefix+v.zfill(3)if prefix.startswith(('SU-','SI-'))or 'IntegrationTest'in prefix else prefix+str(int(v))for v in m.group(1).split('/'))
    return out
def load(code):
    p=next(BASE.glob('[[]E8300-QCC[]] '+code+'*.docx'))
    with zipfile.ZipFile(p)as z:r=E.fromstring(z.read('word/document.xml'))
    return p,r
def main():
    files={c:load(c)for c in ['SAD','SDDD','SIP']}
    requirement=ROOT.parent/'qcc_requirements/requirements-contract.json';req=json.loads(requirement.read_text(encoding='utf8'))
    expected={'SW-FW-RID-'+str(v['rid'])for v in req['requirements']}
    assert expected=={'SW-FW-RID-'+str(n)for n in [*range(1,82),83]}
    sad=files['SAD'][1];forward={}
    for tr in sad.iter(Q('tr')):
        if dead(tr):continue
        cells=tr.findall('w:tc',N)
        if not cells:continue
        rid=visible(cells[0]).strip()
        if re.fullmatch(r'SW-FW-RID-\d+',rid):
            assert rid not in forward;forward[rid]={'modules':sorted(ids(visible(tr),'SI-ID-QCC-')),'units':sorted(ids(visible(tr),'SU-ID-QCC-')),'text':visible(tr)}
    assert set(forward)==expected
    assert all(x in forward['SW-FW-RID-83']['text']for x in ['SYS-RID-ACDOC-17','SYS-RID-ACDOC-18','SYS-RID-ACDOC-20'])
    assert 'SYS-RID-SP-HW-11'not in forward['SW-FW-RID-69']['text']
    sddd=files['SDDD'][1];units={};current=None
    for b in sddd.find('w:body',N):
        t=visible(b)
        if b.tag==Q('p')and re.match(r'SU-ID-QCC-\d{3}\b',t.strip()):
            current=t.strip().split()[0];units[current]={'title':t.strip(),'parts':[]}
        elif b.tag==Q('p')and t.strip().startswith('인증 키와 비휘발성 저장 데이터'):
            current=None
        if current is not None:units[current]['parts'].append(t)
    assert len(units)==14,('unit headings',len(units))
    for u,v in units.items():
        text='\n'.join(v.pop('parts'));v['requirements']=sorted(ids(text,'SW-FW-RID-'));v['tests']=sorted(ids(text,'SW-FW-UnitTest-QCC-ID-'));v['purpose']=text.split('\n')[1];v['module']='SI-ID-QCC-'+str(1 if int(u[-3:])<=4 else 2 if int(u[-3:])<=7 else 3 if int(u[-3:])<=11 else 4).zfill(3)
        assert v['purpose'].strip() and v['tests'] and v['requirements']
        assert set(v['requirements'])<=expected
        for rid in v['requirements']:assert u in forward[rid]['units'],('SAD forward missing',u,rid)
    tests_path=ROOT.parent/'qcc_tests/current-values.json';tv=json.loads(tests_path.read_text(encoding='utf8'))
    def sheet(code,start,column):
        return next((n,v)for n,v in tv[code].items()if v.get(column+str(start),'').strip())
    _,uc=sheet('SUVC',1727,'D');unit_checks=[]
    for row in range(1727,1765):
        test=uc.get('D'+str(row),'').strip()
        if not test:continue
        us=ids(uc.get('G'+str(row),''),'SU-ID-QCC-');rs=ids(uc.get('E'+str(row),''),'SW-FW-RID-');mods=ids(uc.get('C'+str(row),''),'SI-ID-QCC-')
        assert us and mods
        for u in us:assert test in units[u]['tests'] and units[u]['module'] in mods,(row,u,'test/module trace')
        assert rs<=set().union(*(set(units[u]['requirements'])for u in us)),(row,rs,'SDDD missing test requirement')
        unit_checks.append({'row':row,'test':test,'units':sorted(us),'modules':sorted(mods),'requirements':sorted(rs),'requirement_scope':'direct requirements'if rs else'basic NVM serial-number interface; supports the referenced design unit and does not establish security/NVM protection product pass','status':'통과'})
    assert len(unit_checks)==15
    _,ic=sheet('SITC',131,'C');siptext=visible(files['SIP'][1]);siptests=ids(siptext,'SW-FW-IntegrationTest-ID-');integration=[]
    for row in range(131,145):
        test=ic.get('C'+str(row),'').strip()
        if not test:continue
        us=ids(ic.get('F'+str(row),''),'SU-ID-QCC-');rs=ids(ic.get('F'+str(row),''),'SW-FW-RID-');mods=ids(ic.get('D'+str(row),''),'SI-ID-QCC-')
        assert test in siptests and us<=set(units) and rs<=expected,(row,test in siptests,sorted(us-set(units)),sorted(rs-expected))
        assert rs<=set().union(*(set(units[u]['requirements'])for u in us)),(row,rs,'SDDD missing integration requirement')
        assert all(units[u]['module']in mods for u in us),(row,'integration module')
        integration.append({'row':row,'test':test,'units':sorted(us),'modules':sorted(mods),'requirements':sorted(rs),'SIP_actual_test_reference':True,'status':'통과'})
    assert len(integration)==7
    receive_units={'SU-ID-CFX-005':'I2S 인터페이스 초기화','SU-ID-CFX-008':'I2S 입력 상태 판정','SU-ID-CFX-015':'I2S 수신 버퍼 관리'}
    for u,title in receive_units.items():
        assert u in ic['F141'] and u in siptext and u in visible(sddd),(u,'integration22 direct receiver boundary')
        assert title in visible(sddd),(u,'actual detailed design title')
    _,sc=sheet('SSTC',282,'B');_,sr=sheet('SSTR',282,'B')
    assert sc['B282'].strip()=='SW-FW-SYS-Test-ID-82'
    assert sc['E282'].strip()=='SW-FW-RID-69' and sc['F282'].strip()=='SYS-RID-SP-FW-15'
    assert 'DMIC2' in sc['C282'] and '통화' in sc['C282'] and sc['D282'].strip()=='D'
    assert '미실행' in sr['K282'] and '판정 대기' in sr['L282']
    assert sc['E285'].strip()=='SW-FW-RID-83'
    assert all(u in sc['F285'] for u in ('SYS-RID-ACDOC-17','SYS-RID-ACDOC-18','SYS-RID-ACDOC-20'))
    for c,(_,r)in files.items():
        assert 'SW-FW-RID-82'not in visible(r)
        assert ids(visible(r),'SU-ID-QCC-')==set(units)
        assert ids(visible(r),'SI-ID-QCC-')=={'SI-ID-QCC-'+str(n).zfill(3)for n in range(1,5)}
    out={'status':'통과','scope':'Saved OOXML current-body semantic contract checks for 82 RID, 14 QCC units / 15 QCC unit groups / 7 integration groups, including integration22 direct CFX receiver boundary and system82/83. This does not replace actual native AcceptAll cell preservation or whole E8300 declaration/trace review.','input_hashes':{**{c:sha(p)for c,(p,r)in files.items()},'requirements_contract':sha(requirement),'test_current_values':sha(tests_path)},'requirements':{'active_count':82,'RID82_active_refs':0,'RID69_transmit_receive_integrated':True,'RID83_actual_upper_ids':True,'SAD_forward_current_rows':len(forward)},'units':units,'unit_tests':unit_checks,'integration':integration,'integration22_receiver_units':receive_units,'system82':{'test':'SW-FW-SYS-Test-ID-82','requirement':'SW-FW-RID-69','upper':'SYS-RID-SP-FW-15','method':'D','path':'DMIC2 -> QCC -> Classic call uplink','result':'미실행 / 판정 대기'},'system83':{'requirement':'SW-FW-RID-83','upper':['SYS-RID-ACDOC-17','SYS-RID-ACDOC-18','SYS-RID-ACDOC-20']},'pending_product_verdict':'미실행 / 판정 대기; source/design trace verification does not establish product test pass'}
    (ROOT/'cross-actual-saved.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps({'status':'pass','forward_requirements':len(forward),'design_units':len(units),'unit_groups':len(unit_checks),'integration_groups':len(integration)},ensure_ascii=True))
if __name__=='__main__':main()
