from pathlib import Path
import re,sys
game,target=sys.argv[1:3]
root=Path(__file__).resolve().parents[1]/f'build/native/generated/{game}'
for p in root.glob('*part*.c'):
    s=p.read_text()
    for match in re.finditer(r'recomp_call_func\(func_'+target+r'\)',s):
        prev=s[:match.start()]
        fn=re.findall(r'void func_([0-9A-F]+)',prev)[-1]
        blocks=re.split(r'  /\* \$([0-9A-F]+) \*/\n',prev)[-24:]
        if len(sys.argv)>3 and not any('0x00000010u' in line or '0x0010u' in line for line in ''.join(blocks).splitlines()):continue
        print('caller',fn)
        for line in ''.join(blocks).splitlines():
            if any(x in line for x in ['g_cpu.D[0] =','g_cpu.D[1] =','m68k_read16(','m68k_read8(','m68k_read32(']) and not '_fv' in line:
                print(line.strip())
