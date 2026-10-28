"""The performance smoke build must retain authoritative CTF callbacks."""
from pathlib import Path
import shutil
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[3] / 'src/Platform/Xbox/xbox_debug.c').read_text()
start = source.index('static int xbox_debug_ShouldLogText(')
end = source.index('static long xdbg_NtCreate(', start)
code = '#include <string.h>\n#define XBOX_PERF_SMOKE 1\n' + source[start:end] + r'''
int main(void) {
 if(!xbox_debug_ShouldLogText("BotCTFEvent: event=22 redScore=0 goldScore=1")) return 1;
 if(!xbox_debug_ShouldLogText("BotMatch: route-stalled slot=1")) return 2;
 if(xbox_debug_ShouldLogText("ordinary verbose renderer message")) return 3;
 if(xbox_debug_ShouldLogText(0)) return 4;
 if(!xbox_debug_ShouldLogText("BotLiftProbe: reached phase=2")) return 5;
 return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
    c = Path(directory) / 'test.c'
    executable = Path(directory) / 'test.exe'
    c.write_text(code)
    subprocess.run([shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe',
                    str(c), '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
print('PASS: smoke filter retains CTF callbacks and route diagnostics without enabling verbose logs')
