from pathlib import Path
import re,sys
game,func=sys.argv[1:3]
root=Path(__file__).resolve().parents[1]/f'build/native/generated/{game}'
for p in root.glob('*part*.c'):
    s=p.read_text();match=re.search(r'void func_'+func+r'\(void\) \{(.*?)(?=\nvoid func_|\Z)',s,re.S)
    if not match:continue
    blocks=re.split(r'  /\* \$([0-9A-F]+) \*/\n',match[1])
    print(p.name,func)
    for i in range(1,len(blocks),2):
        body=blocks[i+1]
        lines=[x.strip() for x in body.splitlines() if any(y in x for y in ['m68k_read','m68k_write','g_cpu.D[','g_cpu.A[','recomp_call_func','recomp_tail_call']) and not any(y in x for y in ['_fv','_fa','_fr','_fb','>> 15','>> 31','g_cpu.SR'])]
        if lines:print(blocks[i],'; '.join(lines)[:300])
    break
