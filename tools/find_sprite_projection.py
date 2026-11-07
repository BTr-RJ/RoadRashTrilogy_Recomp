from pathlib import Path
import re
root=Path(__file__).resolve().parents[1]
for game in ['rr1','rr2','rr3']:
    for p in (root/'build/native/generated'/game).glob('*part*.c'):
        s=p.read_text()
        for fn in re.finditer(r'void func_(\w+)\(void\) \{(.*?)(?=\nvoid func_|\Z)',s,re.S):
            b=fn.group(2)
            if '0x3FFFu' in b:
                print(game,fn.group(1),p.name)
