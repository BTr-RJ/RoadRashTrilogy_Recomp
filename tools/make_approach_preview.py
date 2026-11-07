"""Package synchronized 0.4.1/0.4.2 traffic and corner QA captures."""
from pathlib import Path
import base64
import struct
import sys
import zlib
sys.path.insert(0, str(Path(__file__).resolve().parent))
from png_utils import read, chunk

ROOT = Path(__file__).resolve().parents[1]
out = ROOT / 'build/validation/approach042'
sections = []
scenes = [
    ('RR1: tráfego e golpes', ROOT/'build/validation/regressions04/rr1/fixed/combat', out/'rr1/fixed/combat', 120, 2002),
    ('RR1: curva e bordas da pista', out/'rr1/before/corner', out/'rr1/fixed/corner', 240, 1001),
    ('RR1: carro em sentido contrário', out/'rr1/before/traffic', out/'rr1/fixed/traffic', 480, 1001),
    ('RR2: aproximação e marcações', ROOT/'build/validation/regressions04/rr2/fixed/combat', out/'rr2/fixed/combat', 120, 2002),
    ('RR3: aproximação e marcações', ROOT/'build/validation/regressions04/rr3/fixed/combat', out/'rr3/fixed/combat', 120, 2002),
]
for title, before, after, count, delay in scenes:
    frames = []
    for n in range(count):
        a, b = read(before/f'{n:03d}.png'), read(after/f'{n:03d}.png')
        w, h, c = a[:3]
        assert a[:3] == b[:3] and c == 3
        frames.append(zlib.compress(b''.join(b'\0'+x+y for x,y in zip(a[3], b[3])), 6))
    png = bytearray(b'\x89PNG\r\n\x1a\n')
    png += chunk(b'IHDR', struct.pack('>IIBBBBB', w*2, h, 8, 2, 0, 0, 0))
    png += chunk(b'acTL', struct.pack('>II', count, 0))
    seq = 0
    for n, data in enumerate(frames):
        png += chunk(b'fcTL', struct.pack('>IIIIIHHBB', seq, w*2, h, 0, 0, delay, 60000, 0, 0))
        seq += 1
        if not n:
            png += chunk(b'IDAT', data)
        else:
            png += chunk(b'fdAT', struct.pack('>I', seq)+data)
            seq += 1
    png += chunk(b'IEND', b'')
    sections.append(f'''<section><h2>{title}</h2>
<div class="legend"><span>Antes · 0.4.1</span><span>Atual · 0.4.2</span></div>
<img alt="{title}: antes à esquerda, atual à direita, com os mesmos comandos"
src="data:image/png;base64,{base64.b64encode(png).decode()}" width="{w*2}" height="{h}"></section>''')
page = '''<!doctype html><html lang="pt-BR"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Road Rash 0.4.2 — aproximação e estabilidade</title>
<style>body{background:#15191b;color:#f4f0e4;font:17px system-ui;max-width:1200px;margin:40px auto;padding:0 20px}
h1{font-size:28px}h2{color:#ffcd35}p{color:#b7bdbd;line-height:1.5}section{margin:36px 0}
.legend{display:grid;grid-template-columns:1fr 1fr;margin:12px 0}img{width:100%;height:auto;aspect-ratio:32/9;object-fit:fill;image-rendering:pixelated}</style>
<h1>Aproximação e estabilidade · 0.4.2</h1>
<p>Capturas reais e sincronizadas da engine, sem filtros de imagem. Cada loop mostra quatro segundos
com aceleração, golpes ou direção. O tráfego ganha até 18% de escala perto da câmera;
as peças de cada ator permanecem juntas. As marcações centrais seguem a profundidade do chão.</p>'''
page += '\n'.join(sections)
page += '''<p>A comparação de estado usa RAM, VRAM e HUD. A auditoria de raster verifica
composição do piloto, perda de atores nativos e faixas azuis no asfalto.
As trajetórias cobrem pistas iniciais, sem homologar todas as campanhas.</p></html>'''
target = out/'comparacao.html'
target.write_text(page, encoding='utf-8')
print(target)
