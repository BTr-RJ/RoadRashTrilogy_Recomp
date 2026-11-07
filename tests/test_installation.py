"""Public installer boundaries, using synthetic ROMs only."""
from pathlib import Path
import hashlib
import importlib.util
import json
import shutil
import tempfile
import unittest
import zipfile

ROOT=Path(__file__).resolve().parents[1]
def module(name):
    spec=importlib.util.spec_from_file_location(name,ROOT/'tools'/f'{name}.py')
    result=importlib.util.module_from_spec(spec);spec.loader.exec_module(result);return result
roms=module('install_roms');sdk=module('bootstrap_public');release=module('package_release')

class InstallationTests(unittest.TestCase):
    def fixture(self,root):
        data=b'SYNTHETIC FIXTURE ONLY'*100;digest=hashlib.sha256(data).hexdigest()
        (root/'games/rr1').mkdir(parents=True)
        (root/'games/supported-roms.json').write_text(json.dumps({'rr1':dict(title='Synthetic fixture',sha256=digest)}))
        for name in ('runtime.toml','runtime-observed.toml'):(root/'games/rr1'/name).write_text('# synthetic metadata\n')
        return data,digest
    def test_zip_uses_contents_and_does_not_extract_entry_paths(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);data,digest=self.fixture(root);source=root/'user originals.zip'
            with zipfile.ZipFile(source,'w') as z:
                z.writestr('../../escaped.bin',data);z.writestr('extra/unsupported.md',b'unknown'*100)
            before=source.read_bytes()
            self.assertEqual(roms.stage(root,[source]),['rr1'])
            self.assertEqual(source.read_bytes(),before)
            self.assertEqual((root/'.local/roms/rr1'/f'{digest}.bin').read_bytes(),data)
            self.assertFalse((root/'escaped.bin').exists())
            self.assertFalse((root/'.local/games/rr2.json').exists())
    def test_unknown_revision_leaves_no_private_inputs(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);self.fixture(root);source=root/'unsupported.bin';source.write_bytes(b'unknown'*100)
            with self.assertRaisesRegex(ValueError,'No supported'):roms.stage(root,[source])
            self.assertFalse((root/'.local').exists())
    def test_duplicate_folders_stage_each_game_once(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);data,digest=self.fixture(root);source=root/'user';source.mkdir();(source/'game.GEN').write_bytes(data)
            self.assertEqual(roms.stage(root,[source,source]),['rr1'])
            state=json.loads((root/'.local/games/rr1.json').read_text());self.assertEqual(state['report']['sha256'],digest)
    def test_sdk_archive_cannot_escape_destination(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);archive=root/'sdk.zip'
            with zipfile.ZipFile(archive,'w') as z:z.writestr('../escape.txt','bad')
            with self.assertRaisesRegex(ValueError,'Unsafe'):sdk.extract(archive,root/'sdk')
            self.assertFalse((root/'escape.txt').exists())
    def test_package_excludes_private_inputs_and_generated_code(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)/'project';root.mkdir()
            for name in ('.local','build','.tools','src','tools/__pycache__','roms'):(root/name).mkdir(parents=True)
            for name in ('.local/game.bin','build/game.c','.tools/private.exe','roms/game.bin','tools/__pycache__/secret.pyc'):(root/name).write_text('PRIVATE')
            (root/'src/main.cpp').write_text('// authored source')
            dest=Path(tmp)/'package';manifest=release.stage(root,dest)
            self.assertEqual([x['path'] for x in manifest['files']],['src/main.cpp'])
            (dest/'src/leaked.bin').write_bytes(b'bad')
            with self.assertRaisesRegex(ValueError,'Private/generated'):release.audit(dest)
    def test_offline_package_rejects_corrupt_dependency(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)/'project';(root/'tools').mkdir(parents=True);(root/'.tools/downloads').mkdir(parents=True)
            (root/'tools/public-tools.lock.json').write_text(json.dumps(dict(downloads=[dict(archive='sdk.zip',sha256='0'*64)])))
            (root/'.tools/downloads/sdk.zip').write_bytes(b'corrupt')
            with self.assertRaisesRegex(ValueError,'checksum mismatch'):release.stage(root,Path(tmp)/'package',offline=True)
    def test_archive_preserves_full_dotted_release_name(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory=Path(tmp)/'RoadRashTrilogyRecomp-0.50-Windows-x64';directory.mkdir()
            (directory/'README.md').write_text('public fixture')
            target=release.archive(directory)
            self.assertEqual(target.name,directory.name+'.zip')
            with zipfile.ZipFile(target) as z:self.assertEqual(z.namelist(),[directory.name+'/README.md'])

    def music_fixture(self,root):
        pack=root/'private music';(pack/'source').mkdir(parents=True)
        data=b'SYNTHETIC AUDIO FIXTURE';(pack/'source/test.m4a').write_bytes(data)
        (pack/'source/unmapped.m4a').write_bytes(b'not selected')
        (pack/'source/test.info.json').write_text('{"private_path":"do not ship"}')
        rows=[]
        for game in ('rr1','rr2','rr3'):
            (pack/(game+'.tsv')).write_text('1\ttest.m4a\tSynthetic track\n')
            rows.append(dict(game=game,cue=1,artist='Synthetic artist',url='https://example.com/track',duration=180,
                             sha256=hashlib.sha256(data).hexdigest(),private_path='do not ship'))
        (pack/'sources.json').write_text(json.dumps(rows))
        return pack

    def test_music_package_copies_only_verified_mapped_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);pack=self.music_fixture(root);project=root/'project';project.mkdir()
            dest=root/'package';manifest=release.stage(project,dest,music_pack=pack)
            self.assertEqual(manifest['music_pack']['tracks'],1)
            self.assertEqual(manifest['music_pack']['cues'],3)
            payload=dest/'music/dean-harris'
            self.assertEqual((payload/'source/test.m4a').read_bytes(),b'SYNTHETIC AUDIO FIXTURE')
            self.assertFalse((payload/'source/unmapped.m4a').exists())
            self.assertFalse((payload/'source/test.info.json').exists())
            self.assertNotIn('private_path',(payload/'sources.json').read_text())
            # Source/default packages never scoop up an unselected public audio folder.
            (project/'music').mkdir();shutil.copytree(payload,project/'music/dean-harris')
            source_manifest=release.stage(project,root/'source')
            self.assertFalse(any(f['path'].startswith('music/dean-harris/') for f in source_manifest['files']))

    def test_music_package_rejects_corruption_before_staging(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);pack=self.music_fixture(root)
            (pack/'source/test.m4a').write_bytes(b'corrupt')
            with self.assertRaisesRegex(ValueError,'Music checksum mismatch'):
                release.stage(root,root/'package',music_pack=pack)
            self.assertFalse((root/'package').exists())

    def test_music_package_rejects_escaping_track_path(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);pack=self.music_fixture(root)
            (pack/'rr1.tsv').write_text('1\t../../test.m4a\tSynthetic track\n')
            with self.assertRaisesRegex(ValueError,'Unsafe music filename'):release.music_files(pack)

if __name__=='__main__':unittest.main()
