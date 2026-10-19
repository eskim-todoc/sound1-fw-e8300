"""Read-only current owned design styles and native manifest linkage."""
from pathlib import Path
from zipfile import ZipFile
import json, hashlib
from lxml import etree as E
from validate_candidates import styles,effective,boolnorm

ROOT=Path(__file__).parent
G=Path('G:/공유 드라이브/20_프로젝트별/01_project sound1/08_DHF/03_설계 출력/SW/외부기')
DOTX=Path('E:/workspace/rules-for-codex/지침/양식/Word/TDC 공통 스타일.dotx')
N={'w':'http://schemas.openxmlformats.org/wordprocessingml/2006/main'}
Q=lambda t:'{'+N['w']+'}'+t
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def main():
    ti,tn,td=styles(DOTX)
    report={'source_dotx':str(DOTX),'source_dotx_sha256':sha(DOTX),'documents':{},'scope':'Current saved DOCX read-only style registration, reference graph and support inheritance checks. Flattened property differences are diagnostics, not an assertion that every default/numbering property is identical. Actual font/baseline/table and pagination checks are linked separately to full native PNG coverage.',
            'comparison_limits':['Word omits explicit zero/default values and may materialize inherited document defaults.','Preserved E8300 heading numbering uses saved numbering IDs, not DOTX numbering IDs.','Table-style inherited paragraph defaults differ from DOTX document defaults; actual new cells explicitly use TDC table paragraph styles.','Native saved font defaults must be checked in actual rendered paragraphs; absence of a raw sz element does not by itself establish a font size defect.']}
    for code in ['SAD','SDDD','SIP']:
        path=next(G.glob('[[]E8300-QCC[]] '+code+'*.docx'))
        before=sha(path);di,dn,dd=styles(path)
        findings=[]
        for name,tkey in tn.items():
            if not name.startswith('TDC'):continue
            assert name in dn,name
            expected=effective(ti,tkey,td)
            actual=effective(di,dn[name],dd)
            differences=[]
            for k in sorted(set(expected)|set(actual)):
                ev=boolnorm(k,expected.get(k));av=boolnorm(k,actual.get(k))
                if ev!=av:differences.append({'property':k,'template':ev,'saved':av})
            findings.append({'name':name,'saved_id':dn[name],'effective_differences':differences})
        assert len(findings)==59
        source_name_by_id={v:k for k,v in tn.items()}
        saved_name_by_id={v:k for k,v in dn.items()}
        support={'Normal':'공통 양식 본문 기반','Normal Table':'공통 양식 표 기반'}
        graph=[]
        for name,tkey in tn.items():
            if not name.startswith('TDC'):continue
            for relation in ('basedOn','next','link'):
                tr=ti[tkey].find('w:'+relation,N);dr=di[dn[name]].find('w:'+relation,N)
                if tr is None:continue
                target_name=source_name_by_id[tr.get(Q('val'))]
                target_name=support.get(target_name,target_name)
                actual_name=saved_name_by_id.get(dr.get(Q('val'))) if dr is not None else None
                if relation=='next' and dr is None and target_name==name:
                    actual_name=name # Omitted self-next has the same native meaning.
                assert actual_name==target_name,(code,name,relation,target_name,actual_name)
                graph.append({'style':name,'reference':relation,'expected_name':target_name,'saved_name':actual_name,'status':'통과'})
        base=di[dn['공통 양식 본문 기반']]
        pp=base.find('w:pPr',N)
        spacing=pp.find('w:spacing',N)
        base_effective=effective(di,dn['공통 양식 본문 기반'],dd)
        assert base_effective.get('/pPr/spacing/@after')=='160' and base_effective.get('/pPr/spacing/@line')=='300' and base_effective.get('/pPr/spacing/@lineRule')=='auto',base_effective
        assert sha(path)==before
        report['documents'][code]={'path':str(path),'sha256':before,'TDC_count':59,'style_properties':findings,
                                    'style_registration':'통과','style_reference_graph':graph,'support_inheritance':'통과',
                                    'support_Normal_spacing':{'after_twips':160,'line':300,'lineRule':'auto'},
                                    'source_unchanged':True,'flattened_diagnostic_difference_style_count':sum(bool(s['effective_differences'])for s in findings)}
    (ROOT/'current-style-qa.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps({k:{'sha':v['sha256'],'registered_TDC_styles':v['TDC_count'],'reference_graph':'PASS','support_spacing':'PASS','diagnostic_difference_styles':v['flattened_diagnostic_difference_style_count']}for k,v in report['documents'].items()},ensure_ascii=True))
if __name__=='__main__':main()
