"""Record the reviewed evidence identity and shared integration correction."""
from pathlib import Path
import json, hashlib, sys
sys.stdout.reconfigure(encoding='utf-8')
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
contract=HERE/'evidence/integration-contract.json'
data=json.loads(contract.read_text(encoding='utf-8-sig'))
data['requirements']['confirmed_values']['52']='128bit CRA keys. QCC SRS lists remocon AES-CMAC/mapping SHA256; QCC SDDD uses SHA256 for clinician service-key derivation and AES-CMAC for CRA. Preserve this role conflict as QCC gap; do not call SHA256 a proved mapping CRA or transport-encryption algorithm.'
data['requirements']['confirmed_values']['76']='Manufacturing-installed shared_service_key/clinician_master_key are separate from derived clinic_service_key; exact NVM IDs, encryption mode/access/integrity/bootstrap remain pending.'
contract.write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
path=ROOT/'docs/참고/시스템/분석 기준 파일.json'
data=json.loads(path.read_text(encoding='utf-8-sig'))
for item in data['files']:
    if item['path']=='src/1__cfx/.cproject':
        item['sha256']=hashlib.sha256((ROOT/item['path']).read_bytes()).hexdigest()
data['qcc_design_review_date']='2026-10-12'
data['qcc_design_review_scope']='Provided QCC design documents, full text/tables/embedded diagrams and E8300 SPI/GPIO/I2S interface comparison. Four modules and fourteen design units; no QCC source/build/device execution. See Sound1 시스템 구조.md section 6.2.'
data['cfx_configuration_review']='2026-10-12: current .cproject differs from Git HEAD only by test sourcePath inclusion; inspected sourceEntries and retained the existing test-only/normal-build boundary explanation. No toolchain/macro/optimization change shown by this diff and no firmware edit in this document task.'
baseline=json.loads((HERE/'evidence/source-baseline.json').read_text(encoding='utf-8-sig'))
data['qcc_document_evidence']=[{'path':i['path'],'sha256':i['sha256'].lower(),'basis':'Provided design/criteria, not implementation or execution result'} for i in baseline if Path(i['path']).name.startswith('QCC_')]
path.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'qcc_documents':len(data['qcc_document_evidence']),'updated_source':'src/1__cfx/.cproject'},ensure_ascii=False))
