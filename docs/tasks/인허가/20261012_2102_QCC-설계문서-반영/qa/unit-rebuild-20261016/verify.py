from rebuild import *
b=Book(H/'base.xlsx');q=Book(H/'qcc.xlsx');c=Book(H/'candidate.xlsx');p=readj('plan.json');n=p['sheet'];errors=[];checks={}
def check(key,ok,detail=None):
 checks[key]={'pass':bool(ok),'detail':detail}
 if not ok:errors.append(key)
def canon(el):return E.tostring(el,method='c14n')
check('source_hashes',digest(BASE)==readj('baseline.json')['baseSHA'] and digest(QCC)==readj('baseline.json')['qccSHA'])
check('sheet_order',list(c.names)==list(b.names)+[p['comparisonSheet']])
check('cover_byte_exact',b.parts[b.names['표지']]==c.parts[c.names['표지']])
allowedParts={'xl/worksheets/sheet2.xml','xl/styles.xml','xl/workbook.xml','xl/_rels/workbook.xml.rels','[Content_Types].xml','docProps/app.xml'}
check('other_parts_byte_exact',all(c.parts.get(k)==v for k,v in b.parts.items() if k not in allowedParts))
exceptions=set(p['updates']);diff=[];count=0
for a,cell in b.cells[n].items():
 if int(re.search(r'\d+',a)[0])>1726:continue
 if a in exceptions:continue
 count+=1
 if canon(cell)!=canon(c.cells[n].get(a)):diff.append(a)
check('original_cells_exact',not diff,{'count':count,'differences':diff})
baseRows={r.get('r'):r for r in b.sheets[n].find(S+'sheetData')};newRows={r.get('r'):r for r in c.sheets[n].find(S+'sheetData')}
check('original_row_attributes',all(dict(r.attrib)==dict(newRows[k].attrib) for k,r in baseRows.items() if int(k)<=1726))
for key,r in baseRows.items():
 if int(key)<=1726:continue
 nr=deepcopy(newRows[str(int(key)+38)]);nr.set('r',key)
 for cell in nr:cell.set('r',shiftref(cell.get('r'),-38,1765))
 check('footer_row_'+key,stripns(nr)==stripns(r))
check('column_widths_styles',canon(b.sheets[n].find(S+'cols'))==canon(c.sheets[n].find(S+'cols')))
for tag in ['sheetFormatPr','sheetViews','pageMargins','pageSetup','headerFooter','printOptions','rowBreaks','colBreaks','sheetProtection']:
 old=b.sheets[n].find(S+tag);new=c.sheets[n].find(S+tag)
 check('preserve_'+tag,(old is None and new is None) or (old is not None and new is not None and canon(old)==canon(new)))
bm=[m.get('ref') for m in b.sheets[n].findall('.//'+S+'mergeCell')];cm=[m.get('ref') for m in c.sheets[n].findall('.//'+S+'mergeCell')]
check('merges_existing_and_new',cm==bm+p['merges'],{'existing':len(bm),'added':len(p['merges'])})
for group in ['fonts','fills','borders','cellXfs','cellStyleXfs','numFmts','cellStyles','dxfs']:
 old=b.styles.find(S+group);new=c.styles.find(S+group)
 if old is not None:check('old_'+group+'_unchanged',all(canon(x)==canon(y) for x,y in zip(old,new)) and len(new)>=len(old))
check('qcc_all_values',all(c.value(n,a)==v for a,v in p['values'].items()),len(p['values']))
richbad=[];stylebad=[]
for a in p['values']:
 old=q.cells[n][a];new=c.cells[n][a]
 oldtxt=deepcopy(q.strings[int(old.find(S+'v').text)] if old.get('t')=='s' else old.find(S+'is'));newtxt=deepcopy(new.find(S+'is'))
 oldtxt.tag=newtxt.tag=S+'is'
 if stripns(oldtxt)!=stripns(newtxt):richbad.append(a)
 ox=q.styles.find(S+'cellXfs')[int(old.get('s','0'))];nx=c.styles.find(S+'cellXfs')[int(new.get('s','0'))]
 for attr,group in [('fontId','fonts'),('fillId','fills'),('borderId','borders')]:
  if stripns(q.styles.find(S+group)[int(ox.get(attr))])!=stripns(c.styles.find(S+group)[int(nx.get(attr))]):stylebad.append([a,group])
check('qcc_exact_rich_text',not richbad,richbad)
check('qcc_native_font_fill_border',not stylebad,stylebad)
check('qcc_empty_cells',all(c.value(n,col+str(r))==q.value(n,col+str(r)) for r in range(1727,1765) for col in 'BCDEFGHIJKLM'))
check('no_current_diff_markers',all('[+]' not in c.value(n,a) and '[-]' not in c.value(n,a) for a in c.cells[n]))
check('pending_all_38',all('미실행' in c.value(n,'M'+str(r)) and '판정 대기' in c.value(n,'M'+str(r)) for r in range(1727,1765)))
newids=[c.value(n,a).strip() for a in c.cells[n] if re.fullmatch(r'D\d+',a) and 'SW-FW-UnitTest-' in c.value(n,a)]
check('115_plus_15_unique',len(newids)==130 and len(set(newids))==130 and newids[-15:]==p['ids'])
check('height_limit',all(float(r.get('ht','0'))<=409.5 for r in c.sheets[n].find(S+'sheetData')))
cmp=p['comparisonSheet'];bad=[]
for i,ch in enumerate(p['changes'],5):
 if c.value(cmp,'F'+str(i))!='[-] '+(ch['before'].strip() or '(없음)') or c.value(cmp,'G'+str(i))!='[+] '+ch['after'].strip():bad.append(i)
 for col,color in [('F','FFC00000'),('G','FF008000')]:
  xf=c.styles.find(S+'cellXfs')[int(c.cells[cmp][col+str(i)].get('s','0'))];font=c.styles.find(S+'fonts')[int(xf.get('fontId'))]
  if font.find(S+'color').get('rgb')!=color:bad.append(col+str(i))
check('comparison_values_and_colors',not bad,{'changes':len(p['changes']),'bad':bad})
check('print_area_expanded',any(x.get('name')=='_xlnm.Print_Area' and x.get('localSheetId')=='1' and x.text.endswith('$M$1772') for x in c.wb.findall('.//'+S+'definedName')))
check('no_cell_errors',not any(cell.get('t')=='e' for cells in c.cells.values() for cell in cells.values()))
prefixes={}
for a in c.cells[n]:
 for prefix in ['[QCC 자료 입력]','[QCC/E8300 자료 입력]','[E8300 보완]']:
  if prefix in c.value(n,a):prefixes.setdefault(prefix,[]).append(a)
savej('validation.json',{'status':'PASS' if not errors else 'FAIL','candidateSHA':digest(H/'candidate.xlsx'),'checks':checks,'errors':errors,'prefixLocations':prefixes})
print(json.dumps({'status':'PASS' if not errors else 'FAIL','checks':len(checks),'errors':errors,'originalCellsCompared':count},ensure_ascii=True))
assert not errors
