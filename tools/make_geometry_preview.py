"""Package paired native/geometry captures into a standalone animated preview."""
from pathlib import Path
import base64
import json
import struct
import sys
import zlib
import argparse
sys.path.insert(0, str(Path(__file__).resolve().parent))
from png_utils import read, chunk

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--root', default='build/validation/organic-previous')
args = p.parse_args()
output = ROOT / args.root
sections = []
for game in ['rr1', 'rr2', 'rr3']:
    folder = output / game / 'wide-'
    report = json.loads((folder / 'comparison.json').read_text())
    assert report['passed'], report
    left_label = 'Implementação anterior' if report.get('baseline') == 'previous' else 'Original'
    frames = []
    for n in range(report['frames']):
        a, b = read(folder / 'off' / f'{n:03d}.png'), read(folder / 'on' / f'{n:03d}.png')
        w, h, channels = a[:3]
        assert a[:3] == b[:3] and channels == 3
        frames.append(zlib.compress(b''.join(b'\0' + x + y for x, y in zip(a[3], b[3])), 6))
    animation = bytearray(b'\x89PNG\r\n\x1a\n')
    animation += chunk(b'IHDR', struct.pack('>IIBBBBB', w*2, h, 8, 2, 0, 0, 0))
    animation += chunk(b'acTL', struct.pack('>II', len(frames), 0))
    sequence = 0
    for n, data in enumerate(frames):
        animation += chunk(b'fcTL', struct.pack('>IIIIIHHBB', sequence, w*2, h, 0, 0, 1001, 60000, 0, 0))
        sequence += 1
        if n == 0:
            animation += chunk(b'IDAT', data)
        else:
            animation += chunk(b'fdAT', struct.pack('>I', sequence) + data)
            sequence += 1
    animation += chunk(b'IEND', b'')
    (folder / 'comparison.apng').write_bytes(animation)
    encoded = base64.b64encode(animation).decode('ascii')
    sections.append(f'''<section><h2>{game.upper()}</h2>
<div class="legend"><span>{left_label} · {report['native_distinct']} imagens distintas</span>
<span>Cenário contínuo · {report['geometry_distinct']} imagens distintas</span></div>
<img alt="{game.upper()}: {left_label} à esquerda, cenário contínuo à direita"
src="data:image/png;base64,{encoded}" width="{w*2}" height="{h}">
<p>120 frames sincronizados · RAM, VRAM, pausa e HUD preservados.</p></section>''')
page = '''<!doctype html><html lang="pt-BR"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Road Rash 0.4.0 — cenário contínuo</title>
<style>body{background:#15191b;color:#f4f0e4;font:17px system-ui;max-width:1200px;margin:40px auto;padding:0 20px}
h1{font-size:28px}h2{font-size:23px;color:#ffcd35}p{color:#b7bdbd;line-height:1.5}
section{margin:38px 0 56px}.legend{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin:12px 0}
img{width:100%;height:auto;aspect-ratio:32/9;object-fit:fill;image-rendering:pixelated}
@media(max-width:650px){body{font-size:14px}.legend{font-size:12px}}</style>
<h1>Zoom e cenário contínuos na engine · 0.4.0</h1>
<p>Capturas reais em 16:9. Cada comparação repete dois segundos de aceleração a 59,94 Hz.
As árvores, construções e veículos usam as artes originais completas com escala por profundidade.
O terreno recebe textura em coordenadas de mundo e os planos compartilham o tempo visual.</p>'''
page += '\n'.join(sections)
page += '<p>Teste de uma trajetória por jogo. Tela dividida mantém o renderizador original. Detalhes: docs/ORGANIC-SCENE.md.</p></html>'
(output / 'comparacao.html').write_text(page, encoding='utf-8')
print(output / 'comparacao.html')
