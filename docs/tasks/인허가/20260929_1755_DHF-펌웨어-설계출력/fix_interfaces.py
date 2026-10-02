from build_architecture import *
f=next(STAGE.glob('SDDD*.docx'))
with zipfile.ZipFile(f) as z:parts={n:z.read(n) for n in z.namelist()}
root=E.fromstring(parts['word/document.xml']);rels=E.fromstring(parts['word/_rels/document.xml.rels']);byid={r.get('Id'):r for r in rels}
font=ImageFont.truetype('C:/Windows/Fonts/malgun.ttf',30)
def picture(name,labels,links):
 im=Image.new('RGB',(1600,700),'white');d=ImageDraw.Draw(im)
 boxes=[(50,80,650,230),(950,80,1550,230),(50,470,650,620),(950,470,1550,620)]
 for box,label in zip(boxes,labels):
  d.rectangle(box,outline='black',width=3)
  bb=d.multiline_textbbox((0,0),label,font=font,spacing=12,align='center');d.multiline_text(((box[0]+box[2]-bb[2])/2,(box[1]+box[3]-bb[3])/2),label,font=font,fill='black',spacing=12,align='center')
 for points,label,at in links:
  d.line(points,fill='black',width=3)
  a,b=points[-2:];import math;t=math.atan2(b[1]-a[1],b[0]-a[0]);d.polygon([b,(b[0]-18*math.cos(t-.4),b[1]-18*math.sin(t-.4)),(b[0]-18*math.cos(t+.4),b[1]-18*math.sin(t+.4))],fill='black');d.text(at,label,font=font,fill='black')
 path=OUT/'이미지 참조'/name;im.save(path);return path
pics={
 'rId13':picture('시스템 제어 연계.png',['QCC5181\n전원 정보 / LED 요청','Cortex-M3\n상태 및 동작 제어','부트로더\n초기화 / 이미지 적재','CFX\n실행 / 대기'],[([(650,150),(950,150)],'SPI',(715,100)),([(1250,230),(1250,470)],'공유 상태',(1280,330)),([(350,470),(350,320),(1050,320),(1050,230)],'코어 기동',(550,270))]),
 'rId14':picture('신호처리 연계.png',['Cortex-M3\n맵 / 자극 파라미터 계산','CFX\n오디오 혼합 / 신호처리','QCC5181\n외부 오디오','HEAR\n최댓값 / FFT'],[([(650,150),(950,150)],'공유 메모리',(680,100)),([(650,545),(810,545),(810,200),(950,200)],'I2S',(725,365)),([(1150,230),(1150,470)],'기동',(1010,335)),([(1400,470),(1400,230)],'완료',(1430,335))]),
 'rId15':picture('내부기 실행 연계.png',['Cortex-M3\n내부기 설정 / 출력 모드','CFX\nPCM 생성','내부 이식장치\n자극 출력','FPGA / TX PMIC\n근접 무선 전송'],[([(650,150),(950,150)],'공유 메모리',(680,100)),([(1250,230),(1250,470)],'PCM',(1290,335)),([(950,545),(650,545)],'코일',(745,490)),([(350,230),(350,350),(1070,350),(1070,470)],'I2C 제어',(580,300))]),
 'rId16':picture('블루투스 명령 연계.png',['QCC5181\n외부 명령 / 응답 전달','Cortex-M3\nSPI 수신 및 명령 구분','외부 앱 / 매핑 프로그램','리모콘 / 매핑 / 시스템 정보\n명령 실행 및 응답 준비'],[([(350,470),(350,230)],'블루투스',(90,330)),([(650,130),(950,130)],'SPI 명령',(700,80)),([(950,200),(650,200)],'SPI 응답',(700,240)),([(1250,230),(1250,470)],'실행 / 응답',(1280,335))])}
for rid,pic in pics.items():
 target='media/sound1_interface_'+rid+'.png';byid[rid].set('Target',target);parts['word/'+target]=pic.read_bytes()
 for blip in root.xpath('.//a:blip',namespaces=N):
  if blip.get('{'+N['r']+'}embed')!=rid:continue
  draw=blip
  while draw is not None and E.QName(draw).localname not in ('inline','anchor'):draw=draw.getparent()
  if draw is not None:
   ext=draw.find('{http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing}extent')
   if ext is not None:
    cx=int(ext.get('cx'));cy=round(cx*700/1600);ext.set('cy',str(cy))
    for e in draw.xpath('.//a:xfrm/a:ext',namespaces=N):e.set('cy',str(cy))
parts['word/document.xml']=E.tostring(root,xml_declaration=True,encoding='UTF-8',standalone=True)
parts['word/_rels/document.xml.rels']=E.tostring(rels,xml_declaration=True,encoding='UTF-8',standalone=True)
with zipfile.ZipFile(f,'w',zipfile.ZIP_DEFLATED) as z:
 for n,b in parts.items():z.writestr(n,b)
print('4 interface diagrams corrected')
