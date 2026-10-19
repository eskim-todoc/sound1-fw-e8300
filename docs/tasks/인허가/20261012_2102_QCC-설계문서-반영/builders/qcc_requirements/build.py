import copy, hashlib, json, math, pathlib, re, sys, zipfile
from datetime import datetime, timezone
from lxml import etree as E
ROOT=pathlib.Path(r"E:\workspace\projects\sound1-fw-e8300-backup\docs\tasks\인허가\20261012_2102_QCC-설계문서-반영")
DEST=pathlib.Path(r"G:\공유 드라이브\20_프로젝트별\01_project sound1\08_DHF\02_설계 입력\SW\외부기")
QA=ROOT/"qa"/"qcc_requirements"
PLAN=json.loads((ROOT/"evidence"/"qcc_requirements_change_plan.json").read_text(encoding="utf-8"))
XN="http://schemas.openxmlformats.org/spreadsheetml/2006/main"
WN="http://schemas.openxmlformats.org/wordprocessingml/2006/main"
XML="http://www.w3.org/XML/1998/namespace"
xn=lambda t:"{"+XN+"}"+t
wn=lambda t:"{"+WN+"}"+t
NS={"x":XN,"w":WN}
PREFIXES={"[QCC 자료 입력]":"C00000","[E8300 보완]":"0070C0","[QCC/E8300 자료 입력]":"7030A0"}
FILE={"SRS":"SRS-FW-TD24A 소프트웨어 요구사항 사양서.xlsx","SR":"SR-FW-TD24A 소프트웨어 요구사항.docx","SGR":"SGR-FW-TD24A 소프트웨어 일반요구사항.docx"}
def target(k):return DEST/("[E8300-QCC] "+FILE[k])
def original(k):return DEST/("[E8300] "+FILE[k])
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def readzip(p):
 with zipfile.ZipFile(p) as z:return {n:z.read(n) for n in z.namelist()}
def writezip(parts,p):
 with zipfile.ZipFile(p,"w",zipfile.ZIP_DEFLATED) as z:
  for n,v in parts.items():z.writestr(n,v)
def xml(e):return E.tostring(e,encoding="UTF-8",xml_declaration=True,standalone=True)
def adj(ref):
 return re.sub(r"(\$?[A-Z]{1,3}\$?)(\d+)",lambda m:m[1]+str(int(m[2])+(int(m[2])>=49)),ref)
def prepare_srs():
 assert sha(target("SRS"))==PLAN["sha_initial"]["SRS"],"SRS copy changed; abort"
 parts=readzip(target("SRS")); s=E.fromstring(parts["xl/worksheets/sheet2.xml"]); data=s.find(xn("sheetData"))
 rows={int(r.get("r")):r for r in data.findall(xn("row"))}
 template=copy.deepcopy(rows[48])
 for r in data.findall(xn("row")):
  if int(r.get("r"))>=49:r.set("r",str(int(r.get("r"))+1))
  for c in r.findall(xn("c")):c.set("r",adj(c.get("r")))
 template.set("r","49")
 for c in template.findall(xn("c")):
  c.set("r",re.sub(r"\d+$","49",c.get("r")))
  for child in list(c):c.remove(child)
  c.attrib.pop("t",None)
 data.insert(list(data).index(rows[49]),template)
 for el in s.iter():
  if el.tag in [xn("mergeCell"),xn("dimension"),xn("autoFilter")] and el.get("ref"):el.set("ref",adj(el.get("ref")))
  if el.get("sqref"):el.set("sqref",adj(el.get("sqref")))
  if el.tag==xn("row") and el.get("spans"):pass
 merges=s.find(xn("mergeCells"))
 for m in merges:
  if m.get("ref")=="B20:B48":m.set("ref","B20:B49")
 E.SubElement(merges,xn("mergeCell"),ref="C49:E49");merges.set("count",str(len(merges)))
 cells={c.get("r"):c for c in data.iter(xn("c"))}
 for col in "FGHI":
  new=col+"80"
  cells[new].set("s",cells[col+"75"].get("s","0"))
 for col in "FGHI":
  if col+"49" in cells:
   cells[col+"49"].set("s",cells[col+"48"].get("s","0"))
 parts["xl/worksheets/sheet2.xml"]=xml(s)
 wb=E.fromstring(parts["xl/workbook.xml"])
 for d in wb.findall(".//"+xn("definedName")):
  if d.get("localSheetId")=="1" and d.text:d.text=adj(d.text)
 parts["xl/workbook.xml"]=xml(wb)
 if "xl/calcChain.xml" in parts:
  body_sheet_id=wb.find(xn("sheets"))[1].get("sheetId")
  chain=E.fromstring(parts["xl/calcChain.xml"]);sheetid=None
  for c in chain:
   sheetid=c.get("i",sheetid)
   if sheetid==body_sheet_id:c.set("r",adj(c.get("r")))
  E.SubElement(chain,xn("c"),r="G49",i=body_sheet_id);E.SubElement(chain,xn("c"),r="G80",i=body_sheet_id)
  parts["xl/calcChain.xml"]=xml(chain)
 if "xl/comments1.xml" in parts:
  comments=E.fromstring(parts["xl/comments1.xml"])
  for c in comments.findall(".//"+xn("comment")):c.set("ref",adj(c.get("ref")))
  parts["xl/comments1.xml"]=xml(comments)
 if "xl/drawings/vmlDrawing1.vml" in parts:
  vml=E.fromstring(parts["xl/drawings/vmlDrawing1.vml"]);vx="{urn:schemas-microsoft-com:office:excel}"
  for e in vml.iter(vx+"Row"):
   n=int(e.text);e.text=str(n+(n>=48))
  for e in vml.iter(vx+"Anchor"):
   nums=[int(x.strip()) for x in e.text.split(",")]
   for pos in [2,6]:nums[pos]+=nums[pos]>=48
   e.text=", ".join(map(str,nums))
  parts["xl/drawings/vmlDrawing1.vml"]=xml(vml)
 writezip(parts,QA/"srs-prepared.xlsx")
 print("prepared",QA/"srs-prepared.xlsx")
def get_export_values(parts):
 strings=[]
 if "xl/sharedStrings.xml" in parts:
  strings=["".join(t.itertext()) for t in E.fromstring(parts["xl/sharedStrings.xml"]).findall(xn("si"))]
 root=E.fromstring(parts["xl/worksheets/sheet2.xml"]);vals={}
 for c in root.findall(".//"+xn("c")):
  typ=c.get("t");v=c.find(xn("v"));f=c.find(xn("f"))
  if f is not None:val="="+(f.text or "")
  elif typ=="s":val=strings[int(v.text)]
  elif typ=="inlineStr":val="".join(c.xpath(".//x:t/text()",namespaces=NS))
  else:val=v.text if v is not None else None
  vals[c.get("r")]=val
 return vals
def font_for_cell(parts,c):
 st=E.fromstring(parts["xl/styles.xml"]);xf=st.find(xn("cellXfs"))[int(c.get("s","0"))];font=st.find(xn("fonts"))[int(xf.get("fontId","0"))]
 size=font.find(xn("sz"));return font,float(size.get("val","10")) if size is not None else 10
def set_xtext(parts,c,text):
 for el in list(c):c.remove(el)
 if text.startswith("="):
  c.set("t","str");E.SubElement(c,xn("f")).text=text[1:];m=re.search(r"&(\d+)$",text)
  E.SubElement(c,xn("v")).text="SW-FW-RID-"+m[1] if m else ""
  return
 c.set("t","inlineStr");istr=E.SubElement(c,xn("is"));font,sz=font_for_cell(parts,c)
 for segment in text.splitlines(keepends=True):
  r=E.SubElement(istr,xn("r"));pr=E.SubElement(r,xn("rPr"))
  for key in ["name","b","i","family","charset","scheme"]:
   f=font.find(xn(key))
   if f is not None:pr.append(copy.deepcopy(f))
  if pr.find(xn("name")) is not None:pr.find(xn("name")).tag=xn("rFont")
  prefix=next((p for p in PREFIXES if segment.lstrip().startswith(p)),None)
  E.SubElement(pr,xn("sz"),val=str(sz+2 if prefix else sz))
  if prefix:E.SubElement(pr,xn("color"),rgb="FF"+PREFIXES[prefix]);E.SubElement(pr,xn("u"),val="single")
  else:
   color=font.find(xn("color"))
   if color is not None:pr.append(copy.deepcopy(color))
  t=E.SubElement(r,xn("t"));t.set("{"+XML+"}space","preserve");t.text=segment
def finish_srs():
 parts=readzip(QA/"srs-prepared.xlsx");vals=get_export_values(readzip(QA/"srs-artifact.xlsx"))
 for e in PLAN["cell_edits"]:assert vals.get(e["cell"])==e["value"],("Artifact value mismatch",e["cell"],vals.get(e["cell"]),e["value"])
 s=E.fromstring(parts["xl/worksheets/sheet2.xml"]);data=s.find(xn("sheetData"))
 cells={c.get("r"):c for c in data.iter(xn("c"))};rows={int(r.get("r")):r for r in data}
 for e in PLAN["cell_edits"]:
  c=cells[e["cell"]];set_xtext(parts,c,e["value"])
 styles=E.fromstring(parts["xl/styles.xml"]);xfs=styles.find(xn("cellXfs"));wrap_styles={}
 for e in PLAN["cell_edits"]:
  if not e["cell"].startswith("I"):continue
  c=cells[e["cell"]];oldstyle=int(c.get("s","0"))
  if oldstyle not in wrap_styles:
   newstyle=copy.deepcopy(xfs[oldstyle]);align=newstyle.find(xn("alignment"))
   if align is None:align=E.SubElement(newstyle,xn("alignment"))
   align.set("wrapText","1");newstyle.set("applyAlignment","1");wrap_styles[oldstyle]=len(xfs);xfs.append(newstyle)
  c.set("s",str(wrap_styles[oldstyle]))
 xfs.set("count",str(len(xfs)));parts["xl/styles.xml"]=xml(styles)
 for n,r in rows.items():
  authored=[e for e in PLAN["cell_edits"] if int(re.search(r"\d+$",e["cell"])[0])==n]
  if not authored:continue
  needed=0
  for e in authored:
   col=re.match("[A-Z]+",e["cell"])[0];limit=24 if col=="F" else 12 if col in ["H","I"] else 16
   lines=sum(max(1,math.ceil(len(t)/limit)) for t in e["value"].splitlines())
   needed=max(needed,lines*(17 if col=="F" else 15)+6)
  r.set("ht",str(min(409,max(float(r.get("ht","20")),needed))));r.set("customHeight","1")
 parts["xl/worksheets/sheet2.xml"]=xml(s)
 assert sha(target("SRS"))==PLAN["sha_initial"]["SRS"],"SRS copy changed before save"
 writezip(parts,target("SRS"))
 return {"file":str(target("SRS")),"modified_cells":[e["cell"] for e in PLAN["cell_edits"]],"row_inserted":49,"rid82_row":49,"rid83_row":80,"sha256":sha(target("SRS")),"artifact_values_checked":True,"unrelated_package_parts":"preserved from copied source except sheet2.xml, workbook.xml print ranges, calcChain.xml row references, original note row references/anchor, and added I-column wrapping style"}
def word_write(k):
 assert sha(target(k))==PLAN["sha_initial"][k],"Word copy changed; abort"
 parts=readzip(target(k));doc=E.fromstring(parts["word/document.xml"]);body=doc.find(wn("body"));blocks=list(body);styles=E.fromstring(parts["word/styles.xml"]);stylemap={st.get(wn("styleId")):st for st in styles.findall(wn("style"))}
 existing_ids={int(v) for v in doc.xpath("//@w:id",namespaces=NS) if v.isdigit()};counter=[max(existing_ids,default=0)+1]
 now=datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
 def revision(kind):
  i=counter[0];counter[0]+=1;el=E.Element(wn(kind));el.set(wn("id"),str(i));el.set(wn("author"),"김은수");el.set(wn("date"),now);return el
 def base_pr(p):
  candidates=p.xpath(".//w:r[not(ancestor::w:del)]/w:rPr",namespaces=NS)
  return copy.deepcopy(candidates[0]) if candidates else E.Element(wn("rPr"))
 def base_size(p,pr):
  sz=pr.find(wn("sz"))
  if sz is not None:return int(sz.get(wn("val")))
  sid=p.find("w:pPr/w:pStyle",NS);sid=sid.get(wn("val")) if sid is not None else "Normal";visited=set()
  while sid and sid not in visited:
   visited.add(sid);st=stylemap.get(sid)
   if st is None:break
   sz=st.find("w:rPr/w:sz",NS)
   if sz is not None:return int(sz.get(wn("val")))
   bo=st.find(wn("basedOn"));sid=bo.get(wn("val")) if bo is not None else None
  sz=styles.find("w:docDefaults/w:rPrDefault/w:rPr/w:sz",NS)
  return int(sz.get(wn("val"))) if sz is not None else 20
 def run(p,text):
  r=E.Element(wn("r"));pr=base_pr(p);size=base_size(p,pr);prefix=next((x for x in PREFIXES if text.startswith(x)),None)
  if prefix:
   for tag in ["color","u","sz","szCs"]:
    for el in pr.findall(wn(tag)):pr.remove(el)
   E.SubElement(pr,wn("color"),{wn("val"):PREFIXES[prefix]});E.SubElement(pr,wn("u"),{wn("val"):"single"});E.SubElement(pr,wn("sz"),{wn("val"):str(size+4)});E.SubElement(pr,wn("szCs"),{wn("val"):str(size+4)})
  r.append(pr);t=E.SubElement(r,wn("t"));t.set("{"+XML+"}space","preserve");t.text=text;return r
 def newparagraph(p,text):
  new=E.Element(wn("p"));pr=p.find(wn("pPr"));newpr=copy.deepcopy(pr) if pr is not None else E.Element(wn("pPr"))
  for tag in ["pageBreakBefore","sectPr","numPr"]:
   for e in newpr.findall(wn(tag)):newpr.remove(e)
  rp=newpr.find(wn("rPr"))
  if rp is None:rp=E.SubElement(newpr,wn("rPr"))
  rp.append(revision("ins"));new.append(newpr);ins=revision("ins");ins.append(run(p,text));new.append(ins);return new
 edits=PLAN["word_edits"][k];actual=[]
 for i in sorted(set(map(int,edits["add_after"]))|set(map(int,edits["replace"]))|set(map(int,edits["missing_after"])),reverse=True):
  p=blocks[i-1];assert p.tag==wn("p")
  if str(i) in edits["replace"]:
   assert not p.xpath(".//w:ins",namespaces=NS),"Refuse replacing existing insertion history"
   delete=revision("del");runs=p.findall(wn("r"));assert runs
   for r in runs:
    dr=copy.deepcopy(r)
    for t in dr.findall(wn("t")):t.tag=wn("delText")
    delete.append(dr);p.remove(r)
   p.append(delete);ins=revision("ins");ins.append(run(p,edits["replace"][str(i)]));p.append(ins)
   actual.append({"old_body_block":i,"operation":"replace","current_text":edits["replace"][str(i)]})
  add=edits["add_after"].get(str(i),[])
  if str(i) in edits["missing_after"]:add=add+[edits["missing_after"][str(i)]]
  pos=list(body).index(p)+1
  for text in add:
   body.insert(pos,newparagraph(p,text));pos+=1;actual.append({"old_body_block":i,"operation":"insert after","current_text":text})
 parts["word/document.xml"]=xml(doc)
 settings=E.fromstring(parts["word/settings.xml"])
 if settings.find(wn("trackRevisions")) is None:E.SubElement(settings,wn("trackRevisions"))
 parts["word/settings.xml"]=xml(settings)
 if "docProps/core.xml" in parts:
  core=E.fromstring(parts["docProps/core.xml"]);modified=core.find("{http://purl.org/dc/terms/}modified")
  if modified is not None:modified.text=now
  parts["docProps/core.xml"]=xml(core)
 assert sha(target(k))==PLAN["sha_initial"][k],"Word copy changed before save"
 writezip(parts,target(k))
 verify=E.fromstring(readzip(target(k))["word/document.xml"]);current="\n".join("".join(p.xpath(".//w:t/text()",namespaces=NS)) for p in verify.findall(".//"+wn("p")))
 for item in actual:assert item["current_text"] in current,(k,item)
 newids=[int(v) for v in verify.xpath("//w:ins/@w:id|//w:del/@w:id",namespaces=NS) if int(v)>=min(counter[0]-1,max(existing_ids,default=0)+1)]
 assert all(v not in existing_ids for v in newids)
 missing=[]
 for index,p in enumerate(verify.findall(".//"+wn("p")),1):
  text="".join(p.xpath(".//w:t/text()",namespaces=NS))
  prefix=next((x for x in PREFIXES if text.startswith(x)),None)
  if prefix:
   formatting=p.xpath(".//w:r[w:t]/w:rPr",namespaces=NS)
   missing.append({"paragraph_index":index,"prefix":prefix,"text":text,"color":PREFIXES[prefix],"format_checked":all(r.find(wn("u")) is not None and r.find(wn("color")).get(wn("val"))==PREFIXES[prefix] for r in formatting)})
 return {"file":str(target(k)),"sha256":sha(target(k)),"edits":actual,"missing_locations":missing,"new_revision_range":[max(existing_ids,default=0)+1,counter[0]-1],"existing_revisions_preserved":True,"author":"김은수","accepted_current_text_checked":True,"native_render":"root pending"}
def main():
 QA.mkdir(parents=True,exist_ok=True)
 if sys.argv[1]=="prepare":prepare_srs();return
 if sys.argv[1]=="finish":
  result={"date":"2026-10-12","owner":"qcc_requirements","outputs":[finish_srs(),word_write("SR"),word_write("SGR")],"originals":[{"file":str(original(k)),"sha256":sha(original(k)),"unchanged":sha(original(k))==PLAN["sha_initial"][k]} for k in FILE],"requirement_source_mapping_count":14,"new_requirement_ids":[82,83],"native_render_owner":"root","new_approvals_or_test_passes":False,"final_scope":"SRS, SR, SGR copies only"}
  result["all_originals_unchanged"]=all(x["unchanged"] for x in result["originals"])
  (ROOT/"evidence"/"qcc_requirements_authored.json").write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding="utf-8")
  print(json.dumps({"saved":str(ROOT/"evidence"/"qcc_requirements_authored.json"),"originals_unchanged":result["all_originals_unchanged"],"outputs":[{"file":o["file"],"sha256":o["sha256"]} for o in result["outputs"]]},ensure_ascii=False))
if __name__=="__main__":main()

