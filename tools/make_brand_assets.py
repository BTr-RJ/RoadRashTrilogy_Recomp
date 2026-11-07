"""Render the original code-native application monogram and Windows icon."""
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'.tools/gif-python'))
from PIL import Image,ImageDraw,ImageFont,PngImagePlugin
folder=ROOT/'assets/launcher'
image=Image.new('RGBA',(256,256),(0,0,0,0))
draw=ImageDraw.Draw(image)
draw.rounded_rectangle((8,8,248,248),radius=42,fill=(21,26,29,255))
font=ImageFont.truetype(str(folder/'BarlowCondensed-Bold.ttf'),166)
draw.text((27,-1),'RR',font=font,fill=(244,240,226,255),stroke_width=0)
for y in (175,197,219):draw.rectangle((199,y,214,y+12),fill=(255,201,37,255))
metadata=PngImagePlugin.PngInfo();metadata.add_text('Origin','Original code-native monogram; tools/make_brand_assets.py; Barlow Condensed OFL font')
image.save(folder/'app-icon.png',pnginfo=metadata)
image.save(folder/'app.ico',sizes=[(16,16),(24,24),(32,32),(48,48),(64,64),(128,128),(256,256)])
print(folder/'app.ico')
