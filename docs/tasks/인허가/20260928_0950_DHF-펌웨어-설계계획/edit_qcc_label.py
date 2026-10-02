"""User-authorized deterministic local label edit, no diagram regeneration."""
from PIL import Image, ImageDraw, ImageFont, ImageChops
from build_plans import DEST
from pathlib import Path

original=Image.open('tmp/scmp-image-edit/word/media/image4.png').convert('RGBA')
result=original.copy()
box=(762,1219,828,1239)
bg=original.getpixel((750,1225))
scale=4
patch=Image.new('RGBA',((box[2]-box[0])*scale,(box[3]-box[1])*scale),bg)
draw=ImageDraw.Draw(patch)
font=ImageFont.truetype('C:/Windows/Fonts/arialbd.ttf',18*scale)
b=draw.textbbox((0,0),'QCC',font=font)
x=(patch.width-(b[2]-b[0]))/2-b[0]
y=(patch.height-(b[3]-b[1]))/2-b[1]
draw.text((x,y),'QCC',font=font,fill=(0,0,0,255))
result.paste(patch.resize((box[2]-box[0],box[3]-box[1]),Image.Resampling.LANCZOS),box[:2])
diff=ImageChops.difference(original,result)
bounds=diff.convert('RGB').getbbox()
assert bounds and bounds[0]>=box[0] and bounds[1]>=box[1] and bounds[2]<=box[2] and bounds[3]<=box[3]
assert original.getchannel('A').tobytes()==result.getchannel('A').tobytes()
folder=DEST/'이미지 참조';folder.mkdir(exist_ok=True)
output=folder/'SCMP 형상관리 분기 구성.png'
result.save(output)
result.crop((720,1200,870,1255)).resize((900,330)).save('tmp/scmp-image-edit/qcc-check.png')
print(output)
print('Changed pixels bounded by',bounds,'; outside pixels and full alpha unchanged')
