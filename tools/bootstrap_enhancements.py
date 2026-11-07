"""Project-local enhancement dependencies from their official distributions."""
from pathlib import Path
import hashlib,json,urllib.request,zipfile
ROOT=Path(__file__).resolve().parents[1]
def fetch(url,path):
    path.parent.mkdir(parents=True,exist_ok=True)
    if not path.exists():
        print('Downloading',path.name,flush=True)
        urllib.request.urlretrieve(url,path)
    return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    lock={}
    url='https://github.com/SnowflakePowered/librashader/releases/download/librashader-v0.12.0/librashader-x86_64-windows-v0.12.0-optimized.zip'
    archive=ROOT/'.tools/downloads/librashader-0.12.0.zip'
    lock['librashader']={'url':url,'sha256':fetch(url,archive)}
    dest=ROOT/'.tools/librashader';dest.mkdir(exist_ok=True)
    with zipfile.ZipFile(archive) as z:
        for n in z.namelist():
            p=(dest/n).resolve()
            if not p.is_relative_to(dest.resolve()):raise ValueError('unsafe archive')
            if n.endswith('/'):continue
            p.parent.mkdir(parents=True,exist_ok=True)
            data=z.read(n)
            if not p.exists() or p.read_bytes()!=data:p.write_bytes(data)
    url='https://api.nuget.org/v3-flatcontainer/microsoft.dxsdk.d3dx/9.29.952.8/microsoft.dxsdk.d3dx.9.29.952.8.nupkg'
    package=ROOT/'.tools/downloads/microsoft.dxsdk.d3dx.9.29.952.8.nupkg'
    lock['d3dx']={'url':url,'sha256':fetch(url,package)}
    with zipfile.ZipFile(package) as z:
        for name in ['D3DX9_43.dll','D3DCompiler_43.dll']:
            (dest/name).write_bytes(z.read('build/native/release/bin/x64/'+name))
        for n in z.namelist():
            if n.lower().endswith(('license.txt','notice.txt')):
                (dest/Path(n).name).write_bytes(z.read(n))
    for header in ['librashader.h','librashader_ld.h']:
        url='https://raw.githubusercontent.com/SnowflakePowered/librashader/librashader-v0.12.0/include/'+header
        try: fetch(url,dest/'include'/header)
        except urllib.error.HTTPError as e:
            if header!='librashader_ld.h':raise
    url='https://api.github.com/repos/kokoko3k/koko-aio-slang/commits/main'
    info=json.load(urllib.request.urlopen(url));commit=info['sha']
    url='https://codeload.github.com/kokoko3k/koko-aio-slang/zip/'+commit
    archive=ROOT/'.tools/downloads'/('koko-aio-'+commit+'.zip')
    lock['koko-aio']={'url':url,'commit':commit,'sha256':fetch(url,archive)}
    dest=ROOT/'assets/shaders/koko-aio';dest.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(archive) as z:
        prefix=z.namelist()[0].split('/')[0]+'/'
        for n in z.namelist():
            relative=n.removeprefix(prefix)
            if not relative or n.endswith('/'):continue
            p=(dest/relative).resolve()
            if not p.is_relative_to(dest.resolve()):raise ValueError('unsafe archive')
            p.parent.mkdir(parents=True,exist_ok=True)
            data=z.read(n)
            if not p.exists() or p.read_bytes()!=data:p.write_bytes(data)
    (ROOT/'tools/enhancements.lock.json').write_text(json.dumps(lock,indent=2)+'\n')
    url='https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe'
    fetch(url,ROOT/'.tools/media/yt-dlp.exe')
if __name__=='__main__': main()
