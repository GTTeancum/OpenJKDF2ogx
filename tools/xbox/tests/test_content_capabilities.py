"""Execute production registration and lighting accessors with minimal engine mocks."""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]


def function(path, name):
    text = (root / path).read_text()
    start = text.index(name + '(')
    start = text.rfind('\n', 0, start) + 1
    brace = text.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


registration = function('src/Cog/sithCog.c', 'sithCog_RegisterXboxContentVerbs')
verbs = re.findall(r'RegisterVerb\(ctx, (\w+), "([^"]+)"', registration)
assert len(verbs) == 19
program = r'''
#include <cassert>
#include <map>
#include <string>
#include <cstdint>
typedef double cog_flex_t;
struct sithCog {};
struct sithCogSymboltable {};
static int Main_bMotsCompat;
static std::map<std::string, void(*)()> registered;
void sithCogScript_RegisterVerb(sithCogSymboltable*, void(*f)(), const char* name) {
    assert(registered.count(name)==0); registered[name]=f;
}
'''
program += '\n'.join(f'void {f}() {{}}' for f, _ in verbs) + '\n' + registration
program += r'''
struct sithSector { double ambientLight; } sector;
static sithSector* selectedSector;
static double result;
sithSector* sithCogExec_PopSector(sithCog*) { return selectedSector; }
void sithCogExec_PushFlex(sithCog*, double v) { result=v; }
'''
program += function('src/Cog/sithCogFunctionSector.c', 'sithCogFunctionSector_GetSectorAmbientLight')
program += r'''
int main() {
    sithCogSymboltable table;
    Main_bMotsCompat=0;
    sithCog_RegisterXboxContentVerbs(&table);
    assert(registered.size()==19);
    assert(registered.count("getsurfacevertexlightrgb"));
    assert(registered.count("setsurfacevertexlightrgb"));
    assert(registered.count("findsectoratpos"));
    assert(!registered.count("selectweapon"));
    assert(!registered.count("setinv"));
    registered.clear(); Main_bMotsCompat=1;
    sithCog_RegisterXboxContentVerbs(&table);
    assert(registered.empty());
    sithCog cog;
    selectedSector=&sector; sector.ambientLight=0.375;
    sithCogFunctionSector_GetSectorAmbientLight(&cog);
    assert(result==0.375);
    selectedSector=0;
    sithCogFunctionSector_GetSectorAmbientLight(&cog);
    assert(result==0.0);
}
'''
# Registration mocks and the real accessor share a name; give only the mock
# a distinct identifier so calls below exercise the actual accessor.
program = program.replace('void sithCogFunctionSector_GetSectorAmbientLight() {}',
                          'void MockAmbientRegistration() {}')
program = program.replace('ctx, sithCogFunctionSector_GetSectorAmbientLight,',
                          'ctx, MockAmbientRegistration,')
with tempfile.TemporaryDirectory(prefix='jk-content-') as tmp:
    source = Path(tmp) / 'test.cpp'
    binary = Path(tmp) / 'test.exe'
    source.write_text(program)
    subprocess.run([shutil.which('clang++') or 'C:/Program Files/LLVM/bin/clang++.exe',
                    str(source), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print('PASS: additive JK registration, MotS duplicate avoidance, ambient read/null handling')
