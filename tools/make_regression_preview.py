"""Package synchronized 0.4.0/0.4.1 combat captures into a local APNG page."""
from pathlib import Path
import base64,struct,sys,zlib
sys.path.insert(0,str(Path(__file__).resolve().parent))
from png_utils import read,chunk
ROOT=Path(__file__).resolve().parents[1]
out=ROOT/'build/validation/regressions04';sections=[]
for game in ('rr1','rr2'):
    folder=out/game;frames=[]
    for n in range(120):
        a=read(folder/'before/combat'/f'{n:03d}.png')
        b=read(folder/'fixed/combat'/f'{n:03d}.png')
        w,h,c=a[:3];assert a[:3]==b[:3] and c==3
        frames.append(zlib.compress(b''.join(b'\0'+x+y for x,y in zip(a[3],b[3])),6))
    png=bytearray(b'\x89PNG\r\n\x1a\n')
    png+=chunk(b'IHDR',struct.pack('>IIBBBBB',w*2,h,8,2,0,0,0))
    png+=chunk(b'acTL',struct.pack('>II',len(frames),0));seq=0
    for n,data in enumerate(frames):
        png+=chunk(b'fcTL',struct.pack('>IIIIIHHBB',seq,w*2,h,0,0,2002,60000,0,0));seq+=1
        if not n:png+=chunk(b'IDAT',data)
        else:png+=chunk(b'fdAT',struct.pack('>I',seq)+data);seq+=1
    png+=chunk(b'IEND',b'');(folder/'combat.apng').write_bytes(png)
    sections.append(f'''<section><h2>{game.upper()}</h2>
<div class="legend"><span>Antes · 0.4.0</span><span>Corrigido · 0.4.1</span></div>
<img alt="{game.upper()}: antes à esquerda, corrigido à direita; golpes e direção sincronizados"
src="data:image/png;base64,{base64.b64encode(png).decode()}" width="{w*2}" height="{h}"></section>''')
page='''<!doctype html><html lang="pt-BR"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Road Rash 0.4.1 — correções visuais</title>
<style>body{background:#15191b;color:#f4f0e4;font:17px system-ui;max-width:1200px;margin:40px auto;padding:0 20px}
h1{font-size:28px}h2{color:#ffcd35}p{color:#b7bdbd;line-height:1.5}section{margin:36px 0}
.legend{display:grid;grid-template-columns:1fr 1fr;margin:12px 0}img{width:100%;height:auto;aspect-ratio:32/9;object-fit:fill;image-rendering:pixelated}</style>
<h1>Composição e resposta corrigidas · 0.4.1</h1>
<p>Capturas reais com a mesma sequência de aceleração, golpes, chute e direção.
Cada loop contém quatro segundos simulados, com uma captura a cada dois frames.
O cenário mantém projeção e textura por profundidade. Pilotos e tráfego usam as peças da animação atual,
e o buffer visual de cinco frames foi removido.</p>'''
page+='\n'.join(sections)
page+='''<p>As comparações de RAM, VRAM e HUD são feitas em processos independentes.
A auditoria adicional compara cores e donos dos pixels no instante do raster;
um dump de VRAM ao fim do frame pode já conter a próxima animação.
Os testes cobrem estas trajetórias e outros trechos com tráfego e colinas, sem homologar todas as campanhas.</p></html>'''
(out/'comparacao.html').write_text(page,encoding='utf-8');print(out/'comparacao.html')
