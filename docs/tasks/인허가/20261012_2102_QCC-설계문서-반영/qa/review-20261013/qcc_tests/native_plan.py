from pathlib import Path
import json,openpyxl,re
H=Path(__file__).resolve().parent;T=H.parents[2]
P=json.loads((H/'author-payload.json').read_text(encoding='utf-8'));Q=json.loads((H/'qa.json').read_text(encoding='utf-8'));C=json.loads((H/'current-values.json').read_text(encoding='utf-8'))
CFG={b['key']:b for b in json.loads((T/'builders/qcc_tests/payload.json').read_text(encoding='utf-8'))['books']}
out={'status':'6 saved workbooks stable; numeric/package/common/rich text checks passed; scoped actual native display passed' if Q.get('final_native_display_pass') is True else '6 saved workbooks stable; numeric/package/common/rich text checks passed; actual native display pending','final_native_display_pass':Q.get('final_native_display_pass') is True,'files':[]}
for k,b in P['books'].items():
 w=openpyxl.load_workbook(b['path'],data_only=False);f=next(x for x in Q['files'] if x['key']==k);cfg=CFG[k]
 main=w[cfg['sheet']];last=cfg.get('verdictCol') or cfg['commonLast'];final={'SUVC':1772,'SUVR':1772,'SITC':144,'SITR':144,'SSTC':303,'SSTR':303}[k]
 sheets=[{'sheet':s.title,'range':str(s.print_area).split('!')[-1].replace('$','') if s.print_area else s.calculate_dimension(),'modified':s.title in b['comparison'],'repeat_rows':s.print_title_rows,'row_breaks':[x.id for x in s.row_breaks.brk]} for s in w]
 focused=[]
 if k.startswith('SU'):focused=[{'id':'QCC1..15','range':f'B1727:{last}1764'},{'id':'QCC4 SPI20+length1/ACK/whole RTT','range':f'B1734:{last}1735'},{'id':'QCC6 production Standalone/TWS','range':f'B1737:{last}1737'}]
 if k.startswith('SI'):focused=[{'id':'018..024','range':f'B131:{last}144'},{'id':'018 full SPI/RTT','range':f'B131:{last}133'}]
 if k.startswith('SS'):focused=[{'id':'82/83 RID69 uplink + manual','range':f'B282:{last}285'}]
 rtt=[]
 for a,v in C[k][cfg['sheet']].items():
  if ('RTT' in v or '[C' in v or 'PASS' in v) and len(v)>350:rtt.append({'cell':a,'characters':len(v),'row':int(re.search(r'\d+',a)[0])})
 rtt.sort(key=lambda x:x['characters'],reverse=True)
 out['files'].append({'key':k,'path':b['path'],'sha256':f['sha256'],'main':{'sheet':cfg['sheet'],'full_range':f'B2:{last}{final}','test_header_start':{'SUVC':28,'SUVR':28,'SITC':23,'SITR':23,'SSTC':32,'SSTR':32}[k],'final_row':final,'common_range':next(x['common_range'] for x in Q['pairs'] if k in x['pair']),'new_condition_rows':cfg['newRows'],'focused':focused,'long_RTT_spotchecks':rtt[:8]},'sheets':sheets,'changed_cells':f['changed_cells'],'height_max':f['height_max']})
 w.close()
(H/'native-plan.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'status':out['status'],'files':[{'key':f['key'],'main':f['main']['full_range'],'sheets':[{'sheet':s['sheet'],'range':s['range']} for s in f['sheets']]} for f in out['files']]},ensure_ascii=True))
