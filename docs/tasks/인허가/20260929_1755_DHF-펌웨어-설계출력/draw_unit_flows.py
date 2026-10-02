"""Render authored technical flowcharts with measurable print-size typography."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import hashlib, json, math
import re
from functools import lru_cache
from unit_flowcharts import FLOWS

T=Path(__file__).resolve().parent
ROOT=T.parents[3]
OUT=ROOT/'docs/참고/인허가/사운드1/DHF/03_설계 출력/SW/펌웨어/이미지 참조'
QA=T/'qa/design-readability'
MODEL=json.loads((T/'qa/unit-reconciliation/model.json').read_text(encoding='utf8'))
BY_ID={u['su'].removeprefix('SU-ID-'):u for u in MODEL}
WIDTH=1600
FONT=ImageFont.truetype('C:/Windows/Fonts/malgun.ttf',38)
SMALL=ImageFont.truetype('C:/Windows/Fonts/malgun.ttf',33)
BOLD=ImageFont.truetype('C:/Windows/Fonts/malgunbd.ttf',38)
INK='#243743';EDGE='#405666';BLUE='#EDF3F6'

def lines(text,width,font=FONT):
    result=[]
    for paragraph in text.split('\n'):
        current=''
        for token in re.findall(r'\S+\s*',paragraph):
            if current and font.getlength((current+token).rstrip())>width:
                result.append(current.rstrip());current=token
            else:current+=token
        result.append(current.rstrip())
    return result

def write_center(draw,box,text,font=FONT):
    x0,y0,x1,y1=box
    ls=lines(text,x1-x0-40,font)
    line_height=font.size*1.36
    height=len(ls)*line_height
    assert height <= y1-y0-10,(text,box,height)
    y=(y0+y1-height)/2
    for line in ls:
        draw.text(((x0+x1)/2,y),line,font=font,fill=INK,anchor='mt')
        y+=line_height

def arrow(draw,points,head=True):
    draw.line(points,fill=EDGE,width=3,joint='curve')
    if head:
        a,b=points[-2:];ang=math.atan2(b[1]-a[1],b[0]-a[0]);length=15
        p=[b,(b[0]-length*math.cos(ang-.43),b[1]-length*math.sin(ang-.43)),
           (b[0]-length*math.cos(ang+.43),b[1]-length*math.sin(ang+.43))]
        draw.polygon(p,fill=EDGE)

def box(draw,b,text,terminal=False):
    if terminal:draw.rounded_rectangle(b,radius=35,fill=BLUE,outline=EDGE,width=3)
    else:draw.rectangle(b,fill='white',outline=EDGE,width=3)
    write_center(draw,b,text)

@lru_cache(None)
def decision_layout(text):
    # Bound every glyph line inside the sloped edges, including its top/bottom.
    # Choose a word wrap that keeps the decision compact without shrinking type.
    choices=[]
    for wrap in [470,440,410,380,350,320]:
        ls=lines(text,wrap);lh=FONT.size*1.36
        needed=190
        for i,line in enumerate(ls):
            bx=FONT.getbbox(line,anchor='mt')
            reach=max(abs(bx[0]),abs(bx[2]))+16
            distance=max(abs(-len(ls)*lh/2+i*lh+bx[1]),abs(-len(ls)*lh/2+i*lh+bx[3]))+8
            needed=max(needed,math.ceil(2*distance/(1-reach/375)))
        choices.append((needed,len(ls),ls))
    height,_,ls=min(choices,key=lambda x:(x[0],x[1]))
    return height,ls

def diamond(draw,cx,top,text):
    width=750
    height,ls=decision_layout(text)
    cy=top+height/2
    draw.polygon([(cx,top),(cx+width/2,cy),(cx,top+height),(cx-width/2,cy)],fill=BLUE,outline=EDGE,width=3)
    lh=FONT.size*1.36
    for i,line in enumerate(ls):
        draw.text((cx,cy-len(ls)*lh/2+i*lh),line,font=FONT,fill=INK,anchor='mt')
    return height

def measure(row):
    if row['kind']=='process':return max(100,len(lines(row['text'],1180))*52+24)
    h=decision_layout(row['text'])[0]
    if row['kind']=='choice':
        child_h=max(110,max(len(lines(row[k],640)) for k in ['yes','no'])*52+30)
        return h+80+child_h+35
    side_h=max(115,len(lines(row['branch'],510))*52+32)
    return max(h,side_h)

def entry_y(row,top):
    if row['kind'] in ['process','choice']:return top
    dh=decision_layout(row['text'])[0]
    return top+(measure(row)-dh)/2

def render(key,flow):
    # Wide single-column process boxes; side terminals/loops reserve the right lane.
    rowtops=[];y=135
    for row in flow['rows']:
        rowtops.append(y);y+=measure(row)+65
    height=y+110
    im=Image.new('RGB',(WIDTH,height),'white');d=ImageDraw.Draw(im)
    box(d,(590,15,1010,100),flow['start'],True)
    row_connects=[]
    for i,row in enumerate(flow['rows']):
        kind=row['kind'];top=rowtops[i];h=measure(row)
        cx=800 if kind in ['process','choice'] else 460
        if kind=='process':
            box(d,(180,top,1420,top+h),row['text']);bottom=top+h
        elif kind=='choice':
            dh=diamond(d,800,top,row['text']);branch_top=top+dh+80
            bh=h-dh-115
            for x,txt,label in [(410,row['yes'],'예'),(1190,row['no'],'아니요')]:
                arrow(d,[(800,top+dh),(800,top+dh+20),(x,top+dh+20),(x,branch_top)])
                d.text((x+15,top+dh+30),label,font=SMALL,fill=INK,anchor='lt')
                box(d,(x-340,branch_top,x+340,branch_top+bh),txt)
                arrow(d,[(x,branch_top+bh),(x,top+h),(800,top+h)],False)
            bottom=top+h
        else:
            node_top=entry_y(row,top)
            dh=diamond(d,cx,node_top,row['text'])
            cy=top+h/2;side_h=max(115,len(lines(row['branch'],510))*52+32)
            sb=(970,cy-side_h/2,1520,cy+side_h/2)
            arrow(d,[(835,cy),(970,cy)])
            label=row.get('side','예')
            d.text((895,cy-42),label,font=SMALL,fill=INK,anchor='mt')
            box(d,sb,row['branch'],kind=='guard')
            down=row.get('down','아니요')
            d.text((480,node_top+dh+8),down,font=SMALL,fill=INK,anchor='lt')
            if kind=='loop':
                target=row['target'];assert 0<=target<=i
                tx=800 if flow['rows'][target]['kind'] in ['process','choice'] else 460
                # Use a separate outside lane and enter the target from the top.
                ty=rowtops[target]-43
                arrow(d,[(1520,cy),(1570,cy),(1570,ty),(tx,ty),(tx,entry_y(flow['rows'][target],rowtops[target]))])
            bottom=node_top+dh
        row_connects.append((cx,entry_y(row,top),bottom))
    previous=(800,100)
    for cx,top,bottom in row_connects:
        mid=top-18
        if previous[0]==cx:arrow(d,[previous,(cx,top)])
        else:arrow(d,[previous,(previous[0],mid),(cx,mid),(cx,top)])
        previous=(cx,bottom)
    final_y=y
    arrow(d,[previous,(previous[0],final_y-18),(800,final_y-18),(800,final_y)])
    box(d,(390,final_y,1210,final_y+90),flow['end'],True)
    path=OUT/f'SDDD 순서도 {key}.png';im.save(path,dpi=(300,300))
    # Print at 16cm; very tall flows use their own page without shrinking text.
    cm=16*height/WIDTH
    assert cm<=22,(key,cm)
    return dict(key=key,path=str(path),width=WIDTH,height=height,print_width_cm=16,
                print_height_cm=round(cm,2),font_pt=round(38*16/2.54*72/WIDTH,2),
                label_pt=round(33*16/2.54*72/WIDTH,2),sha256=hashlib.sha256(path.read_bytes()).hexdigest())

def main():
    OUT.mkdir(parents=True,exist_ok=True);QA.mkdir(parents=True,exist_ok=True)
    manifest=[]
    for key,flow in FLOWS.items():
        assert key in BY_ID,key
        assert '·' not in json.dumps(flow,ensure_ascii=False)
        entry=render(key,flow)
        entry.update(name=BY_ID[key]['name'],reason=flow['reason'],refs=BY_ID[key]['refs'],note=flow['note'])
        manifest.append(entry)
    source_paths={r['path'] for x in manifest for r in x['refs']}
    source_paths.update(['src/2__cm3/processorDirective.h','src/1__cfx/signalProcessing/definitionsForAlgorithm.h'])
    data={'flows':manifest,'source_hashes':{p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in sorted(source_paths)}}
    (QA/'flowcharts.json').write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf8')
    # One project-local selection ledger, not a handoff or duplicate system description.
    md=['# 상세 설계 순서도 선정','',
        '분기, 반복, 상태 유지 또는 완료 순서가 중요한 유닛에만 그림을 넣는다. 세부 레지스터 쓰기와 단순 조회는 본문의 처리 설명을 사용한다.', '',
        '115개 유닛의 번호와 경계는 유지한다. 아래는 도식화 범위의 판단이며 기능 검증 결과가 아니다.', '',
        '| 유닛 | 이름 | 처리 | 이유 |','|---|---|---|---|']
    for u in MODEL:
        key=u['su'].removeprefix('SU-ID-')
        reason=FLOWS[key]['reason'] if key in FLOWS else '본문의 입출력/처리와 상위 흐름으로 설명. 개별 설정, 조회, 반복 복사 또는 이미 도식화한 호출 경로와 중복되는 상세는 생략'
        md.append(f"| {key} | {u['name']} | {'순서도 추가' if key in FLOWS else '그림 생략'} | {reason} |")
    md+=['','## 확인 범위','',
          '- 현재 C 코드의 활성 분기와 연결 심볼을 읽어 흐름을 작성했다. 이미지별 근거 파일/심볼과 파일 해시는 `qa/design-readability/flowcharts.json`에 기록한다.',
          '- 내부기 전원 응답 방식과 전원 조절은 현재 컴파일 설정을 따른다. 비활성 패스키 비교, 상태 파일 CRC 검사, NofM 대역 부족 시 자동 중단을 구현된 기능처럼 추가하지 않았다.',
          '- 순서도는 설계 수준으로 처리 단계를 묶는다. 본문에 있는 식, Q 형식, 조건과 인터페이스를 대체하지 않는다.',
          '- 반복 호출형 상태 처리는 한 호출에서 모두 실행되는 흐름과 구분한다. QCC 내부 자료 대기 범위는 그대로다.','']
    (T/'상세 설계 순서도 선정.md').write_text('\n'.join(md),encoding='utf8')
    print(json.dumps({'charts':len(manifest),'max_height_cm':max(x['print_height_cm'] for x in manifest)},ensure_ascii=False))

if __name__=='__main__':main()
