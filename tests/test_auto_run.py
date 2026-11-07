#!/usr/bin/env python3
"""Recovery regression tests: serve :1000 and never write mirror sidecars."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def function(source, name):
    start = source.index('static int ' + name + '(')
    end = source.index('\n}', start) + 2
    return source[start:end]


class ServingRecoveryTest(unittest.TestCase):
    def test_proven_serving_entrypoint(self):
        source = (ROOT / 'host/hbinstall/main.c').read_text()
        main = source.split('int main(void) {', 1)[1]
        self.assertIn('make_data_dir();', main)
        self.assertIn('serve();', main)
        self.assertNotIn('is_auto_run_argv0', source)
        self.assertIn('usleep(200000);', source)
        self.assertIn('Never keep .sha256 sidecars', source)

    def test_real_sidecar_helpers_never_open_mirror_sidecar(self):
        source = (ROOT / 'host/hbinstall/update_impl.inc').read_text()
        harness = r"""
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define DATA_ROOT "/data/elf-launcher/"
#define SIDECAR_NO_DIR DATA_ROOT "mirror/"
static int opens, unlinks, hashes;
static FILE *mock_fopen(const char *p, const char *mode) {
  (void)p; (void)mode; ++opens; return NULL;
}
static int mock_unlink(const char *p) {
  assert(strstr(p, ".sha256")); ++unlinks; return 0;
}
static int sha256_path_hex(const char *p, char out[65]) {
  assert(!strcmp(p, DATA_ROOT "mirror/test.elf"));
  ++hashes; memset(out, 'a', 64); out[64] = 0; return 0;
}
#define fopen mock_fopen
#define unlink mock_unlink
"""
        for name in ['sidecar_in_mirror', 'write_sha256_sidecar', 'read_sha256_sidecar']:
            harness += '\n' + function(source, name)
        harness += r"""
int main(void) {
  char digest[65], result[65]; memset(digest, 'a', 64); digest[64] = 0;
  assert(!write_sha256_sidecar(DATA_ROOT "mirror/test.elf", digest));
  assert(opens == 0 && unlinks == 1);
  assert(!read_sha256_sidecar(DATA_ROOT "mirror/test.elf", result, sizeof(result)));
  assert(!strcmp(result, digest)); assert(opens == 0 && hashes == 1 && unlinks == 2);
  assert(write_sha256_sidecar("/outside/test.elf", digest) < 0);
  assert(write_sha256_sidecar(DATA_ROOT "mirror/test.elf", "bad") < 0);
  puts("PASS: mirror checksums are on-demand, no sidecar fopen, stale sidecars removed");
}
"""
        with tempfile.TemporaryDirectory() as tmp:
            c = Path(tmp) / 'test.c'; c.write_text(harness)
            exe = str(Path(tmp) / 'test')
            subprocess.run(['cc', '-Wall', '-Werror', str(c), '-o', exe], check=True)
            subprocess.run([exe], check=True)


if __name__ == '__main__':
    unittest.main()
