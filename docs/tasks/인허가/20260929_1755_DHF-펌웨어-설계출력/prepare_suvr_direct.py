"""유닛별 예상/기록값, RTT, 예상 그림을 원본 보고서의 같은 행에 배치한다."""
import json
import math
from pathlib import Path
import sys
import openpyxl

TASK=Path(__file__).parent
QA=TASK/'qa/suvr'
def read(p): return json.loads(p.read_text(encoding='utf-8'))
def text_lines(value, width, mono=False):
    total=0
    for line in str(value or '').split('\n'):
        pixels=sum(7.4 if mono else 13.4 if ord(ch)>127 else 7.1 for ch in line)
        total+=max(1,math.ceil(pixels/(width-18)))
    return total

def main():
    p=read(QA/'plan.json')
    items={u['id']:u for suffix in ['cfx-hear','cm3','boot'] for u in read(QA/f'pairs-{suffix}.json')}
    assert set(items)=={t['id'] for t in p['tests']}
    images=read(QA/'expected-images/manifest.json')
    widths=[62,145,205,265,46,160,330,315,310,680,680,130]
    rows=[];meta=[];merges=[];units=[];placements=[]
    def add(uid,kind,row):
        assert len(row)==12
        number=34+len(rows);rows.append(row);meta.append(dict(row=number,id=uid,kind=kind))
        return number
    for t in p['tests']:
        uid=t['id'];item=items[uid];start=34+len(rows)
        assert item['comparisons']
        for i,c in enumerate(item['comparisons']):
            assert all(isinstance(c[k],str) and c[k].strip() for k in ['label','expected','actual'])
            prefix=f'[{i+1}] {c["label"]}\n'
            row=[None]*12
            if i==0:
                row[:9]=t['original'][1:10]
                row[11]=t['result']['judgment']
            row[9:11]=[prefix+c['expected'],prefix+c['actual']]
            add(uid,'comparison',row)
        comparison_end=33+len(rows)
        # 준비/제한/절차는 비교값 행들을 묶어 한 번만 보이게 한다.
        for col in ['H','I','J','M']:
            if comparison_end>start:merges.append(f'{col}{start}:{col}{comparison_end}')
        for l in p['logs']:
            if l[0]!=uid:continue
            row=[None]*12
            row[6]=f'{uid} / RTT {l[1]}\n{l[2]}' if l[2] else f'{uid} / RTT {l[1]}'
            row[9:11]=l[3:5]
            row[11]='통과' if l[3]==l[4] else '결과 대조 필요' if l[4]=='결과 대조 필요' else '기록 첨부 필요'
            r=add(uid,'rtt',row);merges.append(f'H{r}:J{r}')
        for im in images:
            if im['id']!=uid:continue
            row=[None]*12;row[6]=im['caption']
            row[10]=(t['result']['photo'] or '(사진필요, 선택 관찰) 동일 조건의 실제 관찰 화면을 첨부한다.')+'\n왼쪽은 예상 도식이며 실제 사진이 아니다.'
            r=add(uid,'image',row);merges.append(f'H{r}:J{r}')
            placements.append(dict(**im,row=r,col=10))
        # 결과의 출처는 보고서 앞에 한 번 명시한다. 유닛마다 같은 안내를 반복하지 않는다.
        notes=[n for n in item.get('notes',[]) if not n.startswith(('측정/검토 내용은 시험자','검토 내용은 시험자','시험자 수행 확인에 따른 결과 전사.'))]
        if t['result']['exception']:
            notes.append(f'{t["result"].get("exception_scope","결과 대조")}: {t["result"]["exception"]}')
        if t['result']['qcc']:notes.append(t['result']['qcc'])
        if t['result']['photo'] and not any(im['id']==uid for im in images):notes.append(t['result']['photo'])
        if notes:
            row=[None]*12;row[6]='확인 범위 / 추가 자료';row[9]='시험 조건 밖의 추가 사항과 자료 대기는 위 판정과 구분한다.';row[10]='\n'.join(notes)
            r=add(uid,'note',row);merges.append(f'H{r}:J{r}')
        end=33+len(rows)
        for col in ['D','E','F','G']:
            if end>start:merges.append(f'{col}{start}:{col}{end}')
        units.append(dict(id=uid,start=start,end=end,comparisonEnd=comparison_end,judgment=t['result']['judgment'],qcc=bool(t['result']['qcc'])))
    # 원본처럼 코어/모듈을 묶는다. 병합 셀에는 첫 값만 기록한다.
    for c,col in [(0,'B'),(1,'C')]:
        group=0
        for i in range(1,len(units)+1):
            if i==len(units) or p['tests'][i]['original'][c+1]!=p['tests'][group]['original'][c+1] or (c==1 and p['tests'][i]['original'][1]!=p['tests'][group]['original'][1]):
                a,b=units[group]['start'],units[i-1]['end']
                if b>a:merges.append(f'{col}{a}:{col}{b}')
                for u in units[group+1:i]:rows[u['start']-34][c]=None
                group=i
    for row,m in zip(rows,meta):
        if m['kind']=='image':height=294
        elif m['kind']=='rtt':
            height=max(42,max(text_lines(row[c],widths[c],c in [9,10]) for c in [9,10,11])*14+16)
            height=max(height,text_lines(row[6],sum(widths[6:9]))*14+16)
        elif m['kind']=='comparison':height=max(44,max(text_lines(row[c],widths[c]) for c in [9,10])*14+18)
        else:height=max(46,max(text_lines(row[c],widths[c]) for c in [9,10])*14+16)
        assert height<=409,(m,height)
        m['height']=height
    for u in units:
        ix=u['start']-34;ie=u['comparisonEnd']-34
        need=max(text_lines(rows[ix][c],widths[c])*14+18 for c in [6,7,8])
        have=sum(m['height'] for m in meta[ix:ie+1])
        if need>have:meta[ix]['height']+=need-have
    before=openpyxl.load_workbook(QA/'before-direct-comparison.xlsx')
    oldmerges=[str(m) for m in before['보고서_유닛 시험'].merged_cells.ranges if m.min_row>=34]
    end=33+len(rows)
    plan=dict(rows=rows,meta=meta,merges=merges,units=units,images=placements,widths=widths,bodyEnd=end,
              qccRow=end+1,summaryRow=end+4,oldMerges=oldmerges,
              counts={j:sum(u['judgment']==j for u in units) for j in ['통과','결과 대조 필요']})
    (QA/'direct-plan.json').write_text(json.dumps(plan,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(dict(units=len(units),comparison_rows=sum(m['kind']=='comparison' for m in meta),rtt_rows=sum(m['kind']=='rtt' for m in meta),image_rows=len(placements),rows=len(rows),max_height=max(m['height'] for m in meta)),ensure_ascii=False))

if __name__=='__main__':main()
