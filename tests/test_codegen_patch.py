"""Regression for empty level_modes emitted by the pinned SDK."""
import importlib.util
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('patch_codegen', ROOT/'tools/patch_codegen.py')
codegen = importlib.util.module_from_spec(spec)
spec.loader.exec_module(codegen)


class CodegenPatchTests(unittest.TestCase):
    def test_patch_preserves_populated_emission_and_is_idempotent(self):
        source = ('    /* populated modes must retain their values and count */\n'
                  + codegen.LOOP
                  + '        fprintf(f, "%s 0x%02Xu", i ? "," : "", L->level_modes[i]);\n')
        result = codegen.patch_source(source)
        self.assertIn('if (L->level_mode_count == 0)', result)
        self.assertIn('fprintf(f, " 0");', result)
        self.assertEqual(result.replace(codegen.GUARD, ''), source)
        self.assertEqual(codegen.patch_source(result), result)

    def test_unknown_or_ambiguous_sdk_context_is_rejected(self):
        for source in ('', codegen.LOOP * 2, '/* RR_C11_LAYOUT */\n' + codegen.LOOP):
            with self.subTest(source=source), self.assertRaises(ValueError):
                codegen.patch_source(source)


if __name__ == '__main__':
    unittest.main()
