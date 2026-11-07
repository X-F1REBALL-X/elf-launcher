#!/usr/bin/env python3
"""Host-side regression test of the actual auto-run main branch with PS5 stubs."""
from pathlib import Path
import subprocess
import tempfile
import textwrap
import unittest

ROOT = Path(__file__).resolve().parents[1]

class AutoRunTest(unittest.TestCase):
    def test_payload_first_then_missing_tile_repair(self):
        source = (ROOT / "host/hbinstall/main.c").read_text()
        branch = source.split("  if (is_auto_run_argv0(argc, argv)) {", 1)[1].split("\n  }", 1)[0]
        for forbidden in ("serve(", "start_fresh_browser(", "spawn_elfldr("):
            self.assertNotIn(forbidden, branch)
        harness = r"""
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#define SYS_thr_set_name 0
static long long ev_started;
static int current, fail_install;
static char calls[64];
static void record(char c) { size_t n = strlen(calls); calls[n] = c; calls[n+1] = 0; }
static int syscall(int n, ...) { (void)n; return 0; }
static void make_data_dir(void) { record('D'); }
static void evlog(const char *level, const char *fmt, ...) { (void)level; (void)fmt; }
static const char *self_app_ver(void) { return "2.0.21"; }
static void run_headless_auto(void) { record('A'); }
static void write_auto_run_done(void) { record('S'); }
static int home_icon_up_to_date(void) { record('C'); return current; }
static int install_home_icon(void) { record('I'); return fail_install ? -1 : 0; }
static void tile_report(int error) { record(error ? 'F' : 'R'); }
static int auto_run(void) {
""" + branch + r"""
}
int main(void) {
  current = 1; assert(auto_run() == 0); assert(!strcmp(calls, "DASC"));
  calls[0] = 0; current = 0;
  assert(auto_run() == 0); assert(!strcmp(calls, "DASCIR"));
  calls[0] = 0; fail_install = 1;
  assert(auto_run() == 0); assert(!strcmp(calls, "DASCIF"));
  puts("auto-run: current tile, missing/stale tile, install failure PASS");
}
"""
        with tempfile.TemporaryDirectory() as tmp:
            c = Path(tmp) / "test.c"
            c.write_text(textwrap.dedent(harness))
            binary = str(Path(tmp) / "test")
            subprocess.run(["cc", "-Wall", "-Werror", str(c), "-o", binary], check=True)
            subprocess.run([binary], check=True)

if __name__ == "__main__":
    unittest.main()
