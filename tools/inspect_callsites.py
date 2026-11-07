from pathlib import Path
import re,sys
g,target=sys.argv[1:3]
for p in (Path(__file__).resolve().parents[1]/'build/native/generated'/g).glob('*part*.c'):
    s=p.read_text()
    for m in re.finditer(r'recomp_call_func\(func_'+target+r'\)',s):
        prefix=s[:m.start()];fn=re.findall(r'void func_(\w+)',prefix)[-1]
        pc=re.findall(r'recomp_push_return\(0x(\w+)u',prefix)[-1]
        print(fn,pc,p.name)
