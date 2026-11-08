"""Synthetic updater integrity, transaction and personal-data regressions."""
from pathlib import Path
import hashlib
import importlib.util
import io
import json
import os
import subprocess
import sys
import tempfile
import unittest
import zipfile

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('updater',ROOT/'tools/update_release.py')
u=importlib.util.module_from_spec(spec);spec.loader.exec_module(u)

class Updates(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix='rr update a\u00e7\u00e3o ')
        self.work_base=u.WORK_BASE;u.WORK_BASE=Path(self.temp.name)/'work'
        self.root=Path(self.temp.name)/'installed game';self.root.mkdir()
        self.put('.local/installation.json',json.dumps({'product':'RoadRashTrilogyRecomp','version':'0.50','games':['rr1']}))
        for n,data in {'.local/build-inputs/rr1/game.bin':'SYNTHETIC ROM',
                       'build/native/bin/rr1/rr1.exe':'old game','RoadRashTrilogyRecomp.exe':'old launcher',
                       'build/native/bin/rr1/settings.ini':'custom input',
                       '.local/controls.ini':'custom controls','.local/achievements/rr1.dat':'progress',
                       '.local/saves/rr1-campaign.state':'checkpoint','.local/music/custom.m4a':'custom music'}.items():self.put(n,data)
        self.personal={n:(self.root/n).read_bytes() for n in ['.local/controls.ini','.local/achievements/rr1.dat',
            '.local/saves/rr1-campaign.state','.local/music/custom.m4a','build/native/bin/rr1/settings.ini','.local/build-inputs/rr1/game.bin']}
    def tearDown(self):u.WORK_BASE=self.work_base;self.temp.cleanup()
    def put(self,name,data):
        p=self.root/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(data);return p
    def asset(self,label='0.60'):
        name=f'RoadRashTrilogyRecomp-{label}-Windows-x64.zip'
        return dict(version=label,name=name,size=100,digest='sha256:'+'a'*64,state='uploaded',
                    browser_download_url='https://github.com/'+u.REPO+'/releases/download/v'+label+'/'+name)
    def job(self):return u.new_job(self.root)
    def stage(self,job):
        stage=u.workspace(job)/'s'
        for n,data in {'RoadRashTrilogyRecomp.exe':'new launcher','build/native/bin/rr1/rr1.exe':'new game',
                       '.local/installation.json':json.dumps({'product':'RoadRashTrilogyRecomp','version':'0.60','games':['rr1']}),
                       'assets/new.txt':'new asset','.local/controls.ini':'WRONG DEFAULT',
                       'build/native/bin/rr1/settings.ini':'WRONG DEFAULT','.local/saves/rr1-campaign.state':'WRONG DEFAULT'}.items():
            p=stage/n;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(data)
        return stage
    def assertPersonal(self):
        for n,data in self.personal.items():self.assertEqual((self.root/n).read_bytes(),data,n)
    def archive(self,job,extra=None):
        a=self.asset();prefix=a['name'][:-4]+'/'
        data={'RoadRashTrilogyRecomp.exe':b'synthetic executable','scripts/install.ps1':b'synthetic installer',
              'tools/update_release.py':b'synthetic worker','CMakeLists.txt':b'synthetic project'}
        manifest={'version':'0.60','roms_included':False,'kind':'installer','files':[
            dict(path=n,bytes=len(b),sha256=hashlib.sha256(b).hexdigest()) for n,b in data.items()]}
        data['PACKAGE-MANIFEST.json']=json.dumps(manifest).encode();path=job/'download.zip'
        with zipfile.ZipFile(path,'w') as z:
            for n,b in data.items():z.writestr(prefix+n,b)
            if extra:z.writestr(extra,b'invalid path')
        a['size']=path.stat().st_size;a['digest']='sha256:'+u.digest(path);return path,a

    def test_newest_published_test_release_is_selected_and_drafts_ignored(self):
        a=self.asset();feed=[dict(tag_name='v0.50',draft=False,assets=[]),dict(tag_name='v9.0',draft=True,assets=[]),
                             dict(tag_name='v0.60',draft=False,prerelease=True,assets=[a])]
        self.assertEqual(u.select_release(feed,'0.50')['version'],'0.60')
        self.assertIsNone(u.select_release(feed,'0.60'))
        self.assertIsNone(u.select_release(feed,'0.60.0'))
    def test_missing_duplicate_and_wrong_repository_assets_are_rejected(self):
        a=self.asset()
        for assets in [[],[a,a],[dict(a,browser_download_url='https://example.com/update.zip')]]:
            with self.subTest(assets=assets),self.assertRaises(ValueError):u.select_release([dict(tag_name='v0.60',assets=assets)],'0.50')
        for changes in [dict(size=True),dict(size=-1),dict(digest='a'*64),dict(state='uploading')]:
            with self.subTest(changes=changes),self.assertRaises(ValueError):u.validate_asset(dict(a,**changes))
    def test_invalid_versions_are_rejected(self):
        for v in ['0.6-rc1','v0.60/../x',None,'latest','0']:
            with self.subTest(v=v),self.assertRaises(ValueError):u.version(v)
    def test_parent_ads_reserved_and_aliased_paths_are_rejected(self):
        for name in ['../outside','/absolute','x/../y','x//y','C:/x','x\\y','x/CON.txt','x/file:stream','x/name.','x/name ']:
            with self.subTest(name=name),self.assertRaises(ValueError):u.child(self.root,name)
    def test_development_and_incomplete_installations_are_rejected(self):
        self.put('.git','gitdir: fixture')
        with self.assertRaisesRegex(ValueError,'development'):u.installation(self.root)
        (self.root/'.git').unlink();self.put('.local/installation.json',json.dumps({'product':'RoadRashTrilogyRecomp','status':'installing'}))
        with self.assertRaises(ValueError):u.installation(self.root)
    def test_verified_package_extracts_only_manifest_members(self):
        job=self.job();path,a=self.archive(job);p=u.extract(path,a,job)
        self.assertTrue((p/'tools/update_release.py').is_file());self.assertPersonal()
    def test_bad_download_hash_size_and_unlisted_files_are_rejected(self):
        for change in ['hash','size','extra']:
            with self.subTest(change=change):
                job=self.job();a=self.asset();extra=a['name'][:-4]+'/unlisted.txt' if change=='extra' else None
                p,a=self.archive(job,extra)
                if change=='hash':a['digest']='sha256:'+'0'*64
                if change=='size':a['size']+=1
                with self.assertRaises(ValueError):u.extract(p,a,job)
                self.assertPersonal()
    def test_zip_slip_wrong_prefix_and_case_duplicates_are_rejected(self):
        name=self.asset()['name'][:-4]
        for extra in [name+'/../escape.txt','other/file',name+'/CON',name+'/roadRashTrilogyRecomp.EXE']:
            with self.subTest(extra=extra):
                job=self.job();p,a=self.archive(job,extra)
                with self.assertRaises(ValueError):u.extract(p,a,job)
    def test_successful_transaction_preserves_personal_files_and_backs_up_state(self):
        job=self.job();stage=self.stage(job);u.deploy(self.root,job,stage,log=lambda s:None)
        self.assertEqual((self.root/'build/native/bin/rr1/rr1.exe').read_text(),'new game');self.assertPersonal()
        saved=json.loads((job/'previous-saves.json').read_text());self.assertEqual((job/saved[0]['backup']).read_text(),'checkpoint')
        self.assertIsNone(u.recovery_job(self.root));self.assertEqual(json.loads((job/'transaction.json').read_text())['status'],'complete')
    def test_replacement_failure_restores_prior_runtime_and_new_files(self):
        job=self.job();stage=self.stage(job)
        def fail(i):raise OSError('synthetic replacement failure')
        with self.assertRaises(OSError):u.deploy(self.root,job,stage,log=lambda s:None,after_replace=fail)
        self.assertEqual((self.root/'RoadRashTrilogyRecomp.exe').read_text(),'old launcher')
        self.assertEqual((self.root/'build/native/bin/rr1/rr1.exe').read_text(),'old game')
        self.assertFalse((self.root/'assets/new.txt').exists());self.assertPersonal();self.assertIsNone(u.recovery_job(self.root))
    def test_abrupt_process_exit_is_recoverable_from_the_durable_journal(self):
        job=self.job();stage=self.stage(job);script=Path(self.temp.name)/'crash.py'
        script.write_text('import importlib.util,os\nfrom pathlib import Path\n'+
            f's=importlib.util.spec_from_file_location("u",{str(ROOT/"tools/update_release.py")!r})\nu=importlib.util.module_from_spec(s);s.loader.exec_module(u)\n'+
            f'u.WORK_BASE=Path({str(u.WORK_BASE)!r})\n'+
            f'u.deploy(Path({str(self.root)!r}),Path({str(job)!r}),Path({str(stage)!r}),after_replace=lambda i:os._exit(77))\n',encoding='utf-8')
        r=subprocess.run([sys.executable,str(script)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
        self.assertEqual(r.returncode,77,r.stdout);self.assertEqual(u.recovery_job(self.root),job)
        u.recover(self.root,job,log=lambda s:None);self.assertEqual((self.root/'RoadRashTrilogyRecomp.exe').read_text(),'old launcher');self.assertPersonal()
    def test_wrong_job_ownership_and_corrupt_backups_cannot_be_recovered(self):
        job=self.job();u.write_json(job/'owner.json',{'owner':'other','root':str(self.root)})
        with self.assertRaises(ValueError):u.validate_job(self.root,job)
        self.assertPersonal()
    def test_second_update_cannot_acquire_the_installation_lock(self):
        with u.update_lock(self.root):
            with self.assertRaises((ValueError,BlockingIOError)):
                with u.update_lock(self.root):pass
    def test_owned_stage_cleanup_does_not_remove_previous_version_backup(self):
        job=self.job();stage=self.stage(job);u.deploy(self.root,job,stage,log=lambda s:None);u.cleanup_stage(self.root,job)
        entries=json.loads((job/'transaction.json').read_text())['entries'];entry=next(e for e in entries if e['path']=='RoadRashTrilogyRecomp.exe')
        self.assertFalse(stage.exists());self.assertTrue((job/entry['backup']).exists());self.assertPersonal()

if __name__=='__main__':unittest.main()
