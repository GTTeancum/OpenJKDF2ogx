"""Exercise the production catch-up logger with a deterministic host clock."""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
compiler = shutil.which("clang") or r"C:\Program Files\LLVM\bin\clang.exe"
source = (ROOT / "src/Main/sithMain.c").read_text()
function = source.split("static void sithMain_XboxLogCatchup", 1)[1].split(
    "static void sithMain_XboxLogMotsInventoryBin", 1
)[0]
program = r'''
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
static uint32_t now, writes, count, peakFrames, peakDelta;
static uint32_t stdPlatform_GetTimeMsec(void) { return now; }
static void record(const char *format, uint32_t n, uint32_t f, uint32_t d) {
    ++writes; count = n; peakFrames = f; peakDelta = d;
}
#define XPERF record
static void sithMain_XboxLogCatchupFUNCTION
int main(void) {
    unsigned int i;
    /* First overrun is visible immediately, including at clock zero. */
    sithMain_XboxLogCatchup(15, 100);
    assert(writes == 1 && count == 1);
    for (i = 1; i < 50; ++i) {
        now = i * 100;
        sithMain_XboxLogCatchup(16, 110);
    }
    assert(writes == 1);
    now = 5000;
    sithMain_XboxLogCatchup(20, 130);
    assert(writes == 2 && count == 50 && peakFrames == 20 && peakDelta == 130);
    /* Unsigned elapsed time remains correct across the platform clock wrap. */
    now = UINT32_MAX - 1000;
    sithMain_XboxLogCatchup(9, 60);
    assert(writes == 3);
    now = 1000;
    sithMain_XboxLogCatchup(10, 70);
    assert(writes == 3);
    now = 4000;
    sithMain_XboxLogCatchup(11, 80);
    assert(writes == 4 && count == 2 && peakFrames == 11 && peakDelta == 80);
    puts("PASS: catch-up logging is bounded, aggregates overruns, and survives clock wrap");
    return 0;
}
'''.replace("FUNCTION", function)
with tempfile.TemporaryDirectory(prefix="jk-catchup-") as temp:
    path = Path(temp) / "test.c"
    binary = Path(temp) / "test.exe"
    path.write_text(program)
    subprocess.run([compiler, str(path), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
