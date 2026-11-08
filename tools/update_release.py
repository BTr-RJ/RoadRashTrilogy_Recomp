"""Verified, staged updates for installed Road Rash Trilogy games.

The launcher checks in the background; installation always needs the player's
action. Only a built and probed stage reaches the durable replacement journal.
"""
from pathlib import Path
import argparse
import contextlib
import ctypes
import hashlib
import json
import os
import re
import shutil
import stat
import subprocess
import sys
import time
import tempfile
import urllib.parse
import urllib.request
import uuid
import zipfile

REPO='BTr-RJ/RoadRashTrilogy_Recomp'
FEED='https://api.github.com/repos/'+REPO+'/releases?per_page=20'
OWNER='RoadRashUpdater/v1'
WORK_BASE=Path(os.environ.get('LOCALAPPDATA',tempfile.gettempdir()))/'RRTrilogy/Updates'
GAMES=('rr1','rr2','rr3')
TOP={'CMakeLists.txt','README.md','CREDITS.md','THIRD_PARTY_NOTICES.md',
     'CONTRIBUTING.md','Jogar.cmd','RoadRashTrilogyRecomp.exe','SDL2.dll'}
RESERVED={'CON','PRN','AUX','NUL',*(f'COM{i}' for i in range(1,10)),*(f'LPT{i}' for i in range(1,10))}

def version(text):
    if not isinstance(text,str) or not re.fullmatch(r'v?\d+\.\d+(?:\.\d+)?',text):
        raise ValueError('Invalid release version')
    return tuple(int(n) for n in text.removeprefix('v').split('.'))+(0,)*(3-text.count('.')-1)

def no_links(path):
    path=Path(os.path.abspath(path))
    for part in (path,*path.parents):
        if part.exists() or part.is_symlink():
            s=part.lstat()
            if stat.S_ISLNK(s.st_mode) or getattr(s,'st_file_attributes',0)&0x400:
                raise ValueError('Updates cannot follow symbolic links or junctions')
    return path

def child(root,relative):
    if not isinstance(relative,str) or not relative or '\\' in relative:
        raise ValueError('Invalid update path')
    parts=relative.split('/')
    if any(not p or p in ('.','..') or ':' in p or p.endswith((' ','.')) or
           p.split('.')[0].upper() in RESERVED for p in parts):raise ValueError('Invalid update path')
    root=no_links(root);target=no_links(root.joinpath(*parts))
    if not target.is_relative_to(root) or target==root:raise ValueError('Update path escaped its root')
    return target

def digest(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as f:
        for data in iter(lambda:f.read(1024*1024),b''):h.update(data)
    return h.hexdigest()

def atomic(path,data):
    path=no_links(path);path.parent.mkdir(parents=True,exist_ok=True)
    temp=path.with_name('.rr-update-'+uuid.uuid4().hex)
    try:
        with temp.open('xb') as f:f.write(data);f.flush();os.fsync(f.fileno())
        os.replace(temp,path)
    finally:temp.unlink(missing_ok=True)

def write_json(path,value):atomic(path,(json.dumps(value,indent=2)+'\n').encode())

def select_release(releases,current):
    newest=[]
    if not isinstance(releases,list):raise ValueError('Invalid GitHub release list')
    for r in releases:
        if not isinstance(r,dict) or r.get('draft'):continue
        try:v=version(r.get('tag_name'))
        except ValueError:continue
        newest.append((v,r))
    if not newest:return None
    v,r=max(newest,key=lambda x:x[0])
    if v<=version(current):return None
    label=r['tag_name'].removeprefix('v');name=f'RoadRashTrilogyRecomp-{label}-Windows-x64.zip'
    assets=r.get('assets',[])
    if not isinstance(assets,list):raise ValueError('Invalid release assets')
    candidates=[a for a in assets if isinstance(a,dict) and a.get('name')==name]
    if len(candidates)!=1:raise ValueError('The release has no unique Windows installer')
    a=dict(candidates[0]);a['version']=label
    validate_asset(a);return a

def validate_asset(a):
    label=a.get('version');version(label)
    if label.startswith('v'):raise ValueError('Invalid asset version')
    name=f'RoadRashTrilogyRecomp-{label}-Windows-x64.zip'
    expected='https://github.com/'+REPO+'/releases/download/v'+label+'/'+name
    if a.get('name')!=name or a.get('browser_download_url')!=expected or \
       type(a.get('size')) is not int or not 0<a['size']<2*1024**3 or \
       not re.fullmatch(r'sha256:[0-9a-fA-F]{64}',a.get('digest','')) or a.get('state','uploaded')!='uploaded':
        raise ValueError('Invalid installer address, size or SHA-256')

class GitHubRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self,req,fp,code,msg,headers,newurl):
        u=urllib.parse.urlsplit(newurl)
        if u.scheme!='https' or not (u.hostname=='github.com' or (u.hostname or '').endswith('.githubusercontent.com')):
            raise ValueError('Unexpected update download redirect')
        return super().redirect_request(req,fp,code,msg,headers,newurl)

def request(url):
    req=urllib.request.Request(url,headers={'User-Agent':'RoadRashTrilogyRecomp-Updater','Accept':'application/vnd.github+json'})
    return urllib.request.build_opener(GitHubRedirect()).open(req,timeout=20)

def check(current):
    with request(FEED) as response:
        data=response.read(2*1024*1024+1)
        if len(data)>2*1024*1024:raise ValueError('Release metadata is too large')
    return select_release(json.loads(data),current)

def installation(root):
    root=no_links(root)
    if (root/'.git').exists():raise ValueError('Update development checkouts from source')
    marker=child(root,'.local/installation.json')
    data=json.loads(marker.read_text(encoding='utf-8-sig'))
    games=data.get('games')
    if data.get('product')!='RoadRashTrilogyRecomp' or data.get('status')=='installing' or \
       not isinstance(games,list) or not games or len(set(games))!=len(games) or any(g not in GAMES for g in games):
        raise ValueError('Complete game installation before updating')
    for g in games:
        for rel in (f'.local/build-inputs/{g}/game.bin',f'build/native/bin/{g}/{g}.exe'):
            if not child(root,rel).is_file():raise ValueError('Installed game or verified ROM is missing')
    return games

def games_closed(root):
    if os.name!='nt':return
    from ctypes import wintypes as w
    class Process(ctypes.Structure):
        _fields_=[('size',w.DWORD),('usage',w.DWORD),('pid',w.DWORD),('heap',ctypes.c_size_t),
                  ('module',w.DWORD),('threads',w.DWORD),('parent',w.DWORD),('priority',w.LONG),('flags',w.DWORD),('exe',w.WCHAR*260)]
    k=ctypes.WinDLL('kernel32',use_last_error=True)
    k.CreateToolhelp32Snapshot.restype=w.HANDLE;k.OpenProcess.restype=w.HANDLE
    k.CloseHandle.argtypes=[w.HANDLE];k.Process32FirstW.argtypes=[w.HANDLE,ctypes.POINTER(Process)]
    k.Process32NextW.argtypes=k.Process32FirstW.argtypes
    k.QueryFullProcessImageNameW.argtypes=[w.HANDLE,w.DWORD,w.LPWSTR,ctypes.POINTER(w.DWORD)]
    snap=k.CreateToolhelp32Snapshot(2,0)
    if snap==ctypes.c_void_p(-1).value:raise OSError('Could not check running games')
    try:
        e=Process();e.size=ctypes.sizeof(e);ok=k.Process32FirstW(snap,ctypes.byref(e))
        while ok:
            if e.exe.lower() in {g+'.exe' for g in GAMES}:
                h=k.OpenProcess(0x1000,False,e.pid)
                if h:
                    try:
                        size=w.DWORD(32768);path=ctypes.create_unicode_buffer(size.value)
                        if k.QueryFullProcessImageNameW(h,0,path,ctypes.byref(size)) and Path(path.value).is_relative_to(root):
                            raise ValueError('Close the running game before updating')
                    finally:k.CloseHandle(h)
            ok=k.Process32NextW(snap,ctypes.byref(e))
    finally:k.CloseHandle(snap)

@contextlib.contextmanager
def update_lock(root):
    path=child(root,'.local/updates/update.lock');path.parent.mkdir(parents=True,exist_ok=True)
    if os.name=='nt':
        from ctypes import wintypes as w
        k=ctypes.WinDLL('kernel32',use_last_error=True);k.CreateFileW.restype=w.HANDLE
        k.CreateFileW.argtypes=[w.LPCWSTR,w.DWORD,w.DWORD,ctypes.c_void_p,w.DWORD,w.DWORD,w.HANDLE]
        k.CloseHandle.argtypes=[w.HANDLE]
        h=k.CreateFileW(str(path),0xc0010000,0,None,4,0x04000002,None)
        if h==ctypes.c_void_p(-1).value:raise ValueError('Another update is already running')
        try:yield
        finally:k.CloseHandle(h)
    else:
        import fcntl
        with path.open('a+b') as f:
            fcntl.flock(f,fcntl.LOCK_EX|fcntl.LOCK_NB)
            try:yield
            finally:fcntl.flock(f,fcntl.LOCK_UN)

def new_job(root):
    installation(root);base=child(root,'.local/updates');base.mkdir(parents=True,exist_ok=True)
    job=base/('update-'+uuid.uuid4().hex);job.mkdir();write_json(job/'owner.json',{'owner':OWNER,'root':str(no_links(root))})
    work=work_dir(job);work.mkdir(parents=True,exist_ok=False)
    write_json(work/'owner.json',{'owner':OWNER,'root':str(no_links(root)),'job':job.name})
    atomic(job/'workspace.txt',str(work).encode('utf-8'))
    return job

def work_dir(job):
    if not re.fullmatch('update-[0-9a-f]{32}',job.name):raise ValueError('Invalid update job name')
    return child(WORK_BASE,'u-'+job.name[7:19])

def workspace(job):
    root=Path(json.loads(child(job,'owner.json').read_text())['root'])
    validate_job(root,job);return work_dir(job)

def validate_job(root,job):
    root=no_links(root);job=no_links(job)
    if job.parent!=child(root,'.local/updates') or not re.fullmatch('update-[0-9a-f]{32}',job.name):
        raise ValueError('Invalid update working directory')
    owner=json.loads(child(job,'owner.json').read_text())
    if owner!={'owner':OWNER,'root':str(root)}:raise ValueError('Update folder ownership mismatch')
    if json.loads(child(work_dir(job),'owner.json').read_text())!={'owner':OWNER,'root':str(root),'job':job.name}:
        raise ValueError('Update workspace ownership mismatch')
    return job

def package_manifest(package,asset):
    manifest=json.loads(child(package,'PACKAGE-MANIFEST.json').read_text())
    if manifest.get('version')!=asset['version'] or manifest.get('roms_included') is not False or manifest.get('kind')!='installer':
        raise ValueError('Unexpected installer manifest')
    entries=manifest.get('files');seen=set()
    if not isinstance(entries,list):raise ValueError('Invalid installer file manifest')
    for entry in entries:
        rel=entry['path'];p=child(package,rel);key=rel.casefold()
        if key in seen or not p.is_file() or p.stat().st_size!=entry['bytes'] or digest(p)!=entry['sha256']:
            raise ValueError('Installer manifest verification failed')
        seen.add(key)
    actual={p.relative_to(package).as_posix().casefold() for p in package.rglob('*') if p.is_file()}
    if actual!=seen|{'package-manifest.json'}:raise ValueError('Unexpected files outside the installer manifest')
    for rel in ['RoadRashTrilogyRecomp.exe','scripts/install.ps1','tools/update_release.py','CMakeLists.txt']:
        if rel.casefold() not in seen:raise ValueError('The release lacks the automatic updater payload')
    return manifest

def extract(archive,asset,job):
    validate_asset(asset)
    if archive.stat().st_size!=asset['size'] or digest(archive)!=asset['digest'][7:].lower():
        raise ValueError('Installer SHA-256 mismatch')
    folder=asset['name'][:-4];package=child(workspace(job),'package/'+folder)
    if package.exists():raise ValueError('Update package folder already exists')
    with zipfile.ZipFile(archive) as z:
        seen=set();total=0
        if len(z.infolist())>5000:raise ValueError('Too many installer entries')
        for e in z.infolist():
            name=e.filename
            if e.is_dir():name=name[:-1]
            p=child(workspace(job),name);parts=name.split('/')
            if not parts or parts[0]!=folder or len(parts)<2 or name.casefold() in seen or \
               stat.S_ISLNK(e.external_attr>>16):raise ValueError('Unexpected installer ZIP path')
            seen.add(name.casefold());total+=e.file_size
            if total>4*1024**3:raise ValueError('Installer expands beyond its size limit')
        package.mkdir(parents=True)
        for e in z.infolist():
            rel=e.filename[len(folder)+1:]
            if not rel:continue
            dest=child(package,rel.rstrip('/'))
            if e.is_dir():dest.mkdir(parents=True,exist_ok=True);continue
            dest.parent.mkdir(parents=True,exist_ok=True)
            with z.open(e) as source,dest.open('xb') as target:shutil.copyfileobj(source,target)
    package_manifest(package,asset);return package

def prepare(root,asset,opener=request,progress=print):
    validate_asset(asset);games_closed(no_links(root));job=new_job(root)
    if shutil.disk_usage(root).free<4*1024**3:raise ValueError('At least 4 GB of free space is needed to prepare an update')
    archive=workspace(job)/'installer.zip';h=hashlib.sha256();size=0;last=-1
    with opener(asset['browser_download_url']) as response,archive.open('xb') as target:
        while True:
            data=response.read(65536)
            if not data:break
            size+=len(data)
            if size>asset['size']:raise ValueError('Installer is larger than expected')
            target.write(data);h.update(data);percent=size*100//asset['size']
            if percent!=last:progress(f'[PROGRESS] Downloading update: {percent}%');last=percent
    if size!=asset['size'] or h.hexdigest()!=asset['digest'][7:].lower():raise ValueError('Installer download is incomplete or has a wrong SHA-256')
    package=extract(archive,asset,job)
    write_json(job/'release.json',asset);return job,package

def load_package(root,job):
    job=validate_job(root,job);asset=json.loads(child(job,'release.json').read_text());validate_asset(asset)
    package=child(workspace(job),'package/'+asset['name'][:-4]);package_manifest(package,asset)
    return package,asset

def managed(rel):
    if rel in TOP or rel=='.local/installation.json':return True
    parts=rel.split('/')
    if parts[0] in {'src','tools','scripts','games','assets','docs','tests','music'}:return True
    if parts[:3]==['.tools','downloads'] and len(parts)==3:return True
    if parts[:3]==['build','native','bin'] and Path(rel).suffix.lower() in {'.exe','.dll'}:return True
    return False

def journal_file(job):return child(job,'transaction.json')

def recover(root,job,log=print):
    root=no_links(root);games_closed(root);job=validate_job(root,job);j=json.loads(journal_file(job).read_text())
    if j.get('owner')!=OWNER or j.get('root')!=str(root):raise ValueError('Invalid recovery journal')
    for e in reversed(j['entries']):
        rel=e['path']
        if not managed(rel):raise ValueError('Recovery tried to replace a personal file')
        target=child(root,rel)
        if e['existed']:
            if not re.fullmatch(r'backup/\d{6}\.bin',e.get('backup','')):raise ValueError('Invalid recovery backup path')
            backup=child(job,e['backup'])
            if digest(backup)!=e['sha256']:raise ValueError('Recovery backup is damaged')
            atomic(target,backup.read_bytes())
        else:target.unlink(missing_ok=True)
    j['status']='rolled-back';write_json(journal_file(job),j);child(root,'.local/updates/pending.json').unlink(missing_ok=True)
    log('[PROGRESS] Previous version restored')

def recovery_job(root):
    pointer=child(root,'.local/updates/pending.json')
    if not pointer.exists():return None
    value=json.loads(pointer.read_text())
    if value.get('owner')!=OWNER:raise ValueError('Invalid pending update ownership')
    return validate_job(root,child(root,'.local/updates/'+value['job']))

def deploy(root,job,stage,log=print,after_replace=None):
    root=no_links(root);games_closed(root);job=validate_job(root,job);stage=no_links(stage)
    if stage!=workspace(job)/'s':raise ValueError('Invalid update stage')
    if recovery_job(root):raise ValueError('Recover the pending update first')
    games=installation(root)
    for rel in ['RoadRashTrilogyRecomp.exe',*(f'build/native/bin/{g}/{g}.exe' for g in games)]:
        if not child(stage,rel).is_file():raise ValueError('The staged runtime is incomplete')
    files=[]
    for source in sorted(stage.rglob('*')):
        no_links(source)
        if source.is_file():
            rel=source.relative_to(stage).as_posix()
            if managed(rel):files.append((rel,source))
    entries=[]
    for rel,source in files:
        target=child(root,rel)
        if target.is_file() and digest(target)==digest(source):continue
        e={'path':rel,'existed':target.exists(),'sha256':digest(target) if target.exists() else None}
        if e['existed']:
            e['backup']=f'backup/{len(entries):06d}.bin'
            backup=child(job,e['backup']);backup.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(target,backup)
        entries.append(e)
    personal=child(root,'.local/saves')
    save_entries=[]
    if personal.exists():
        for p in personal.rglob('*'):
            no_links(p)
            if p.is_file():
                name=f'previous-saves/{len(save_entries):06d}.bin';backup=child(job,name);backup.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,backup)
                save_entries.append({'path':p.relative_to(personal).as_posix(),'backup':name,'sha256':digest(p)})
    write_json(job/'previous-saves.json',save_entries)
    journal={'owner':OWNER,'root':str(root),'status':'applying','entries':entries}
    write_json(journal_file(job),journal);write_json(child(root,'.local/updates/pending.json'),{'owner':OWNER,'job':job.name})
    log('[APPLY] Installing the verified update')
    sources=dict(files)
    try:
        for i,e in enumerate(entries):
            target=child(root,e['path'])
            if e['existed'] and digest(target)!=e['sha256']:raise ValueError('An installed file changed during update preparation')
            atomic(target,sources[e['path']].read_bytes())
            if after_replace:after_replace(i)
        journal['status']='complete';write_json(journal_file(job),journal)
        child(root,'.local/updates/pending.json').unlink()
    except BaseException:
        recover(root,job,log);raise
    log('[PROGRESS] Update complete. Settings, ROMs and achievements preserved')

def run(command,job,env=None):
    p=subprocess.Popen([str(x) for x in command],env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,
                       creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
    for line in iter(p.stdout.readline,b''):print(line.decode('utf-8',errors='replace').rstrip(),flush=True)
    if p.wait():raise RuntimeError('Update build or validation failed; the previous version is unchanged')

def build_stage(root,job,package):
    games=installation(root);work=workspace(job);stage=work/'s';roms=work/'r';roms.mkdir()
    write_json(stage/'.local/installation.json',{'product':'RoadRashTrilogyRecomp','status':'installing'})
    for g in games:shutil.copy2(child(root,f'.local/build-inputs/{g}/game.bin'),roms/(g+'.bin'))
    cache=stage/'.tools/downloads';cache.mkdir(parents=True)
    old=child(root,'.tools/downloads')
    if old.exists():
        for p in old.iterdir():
            no_links(p)
            if p.is_file() and p.suffix.lower() in {'.zip','.nupkg'}:shutil.copy2(p,cache/p.name)
    powershell=Path(os.environ['SystemRoot'])/'System32/WindowsPowerShell/v1.0/powershell.exe'
    print('[PROGRESS] Building the new version separately. Your installed games are unchanged',flush=True)
    run([powershell,'-NoProfile','-ExecutionPolicy','Bypass','-File',package/'scripts/install.ps1',
         '-Destination',stage,'-RomFolder1',roms,'-NoToolInstall'],job)
    ctest=stage/'.tools/cmake/cmake-4.4.5-windows-x86_64/bin/ctest.exe'
    print('[PROGRESS] Checking the staged runtime',flush=True)
    run([ctest,'--test-dir',stage/'build/native','--output-on-failure'],job)
    script=job/'probe.input';script.write_text('WAIT 900\nPRESS START 12\nWAIT 240\nPRESS START 12\nWAIT 600\nHOLD B\nWAIT 300\nEXIT\n')
    for g in games:
        exe=stage/f'build/native/bin/{g}/{g}.exe';stats=job/(g+'-geometry.json')
        (exe.parent/'settings.ini').write_text('[video]\nfullscreen=0\n[rewind]\nenabled=0\n[input.p1]\ndevice=0\n[input.p2]\ndevice=0\n')
        env=os.environ.copy();env.update(RR_ROOT=str(stage),RR_GEOMETRY='1',RR_WIDE='1',RR_TEST_HIDDEN='1',RR_MUSIC='original',
            RR_GEOMETRY_STATS=str(stats),RR_SHADER_PATH='',RR_OUTPUT_WIDTH='960',RR_OUTPUT_HEIGHT='540',SDL_AUDIODRIVER='dummy',SDL_RENDER_VSYNC='0')
        run([exe,stage/f'.local/build-inputs/{g}/game.bin','--benchmark','3000','--input-script',script],job,env)
        data=json.loads(stats.read_text())
        if data['intermediate_objects']<100 or data['terrain_pixels']<10000:raise ValueError('Staged gameplay validation failed')
    return stage

def cleanup_stage(root,job):
    job=validate_job(root,job)
    if recovery_job(root):raise ValueError('Recover the interrupted update before removing staging files')
    stage=child(workspace(job),'s')
    if stage.exists():
        # Validate the entire owned subtree before any recursive removal.
        for p in stage.rglob('*'):no_links(p)
        shutil.rmtree(stage)

def result(path,**values):
    if path:
        # Win32 GetPrivateProfileString reads this without a JSON dependency.
        text='[update]\n'+'\n'.join(f'{k}={str(v).replace(chr(10)," ").replace(chr(13)," ")}' for k,v in values.items())+'\n'
        atomic(path,text.encode('utf-16'))

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('action',choices=['check','prepare','apply','recover','recovery-info','cleanup'])
    p.add_argument('--root',type=Path,required=True);p.add_argument('--current',default='0.60');p.add_argument('--result',type=Path)
    p.add_argument('--asset',type=Path);p.add_argument('--job',type=Path);a=p.parse_args();root=no_links(a.root)
    try:
        if a.action=='check':
            asset=check(a.current)
            if asset:
                path=child(root,'.local/updates/offer.json');write_json(path,asset)
                result(a.result,state='available',version=asset['version'],asset=str(path))
            else:result(a.result,state='current',version=a.current)
        elif a.action=='prepare':
            job,package=prepare(root,json.loads(no_links(a.asset).read_text()),progress=lambda x:print(x,flush=True))
            result(a.result,state='prepared',job=str(job),host=str(package/'RoadRashTrilogyRecomp.exe'))
        elif a.action=='apply':
            with update_lock(root):
                games_closed(root);package,asset=load_package(root,a.job);stage=build_stage(root,a.job,package)
                deploy(root,a.job,stage,log=lambda x:print(x,flush=True))
                try:cleanup_stage(root,a.job)
                except OSError:print('Temporary build files retained in the update log folder',flush=True)
            result(a.result,state='complete')
        elif a.action=='recover':
            with update_lock(root):
                recover(root,a.job)
                try:cleanup_stage(root,a.job)
                except OSError:print('Temporary build files retained in the update log folder',flush=True)
            result(a.result,state='recovered')
        elif a.action=='cleanup':
            with update_lock(root):cleanup_stage(root,a.job)
            result(a.result,state='cleaned')
        else:
            job=recovery_job(root)
            if job:
                package,_=load_package(root,job);result(a.result,state='pending',job=str(job),host=str(package/'RoadRashTrilogyRecomp.exe'))
            else:result(a.result,state='current')
    except Exception as error:
        result(a.result,state='error',error=str(error));print('Update failed: '+str(error),flush=True);return 1
    return 0

if __name__=='__main__':sys.exit(main())
