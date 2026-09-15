"""Check floor/footprint rejection equivalence for every segment mode."""
from pathlib import Path
import shutil
import subprocess
import tempfile

s = (Path(__file__).resolve().parents[3] / 'src/AI/sithBot.c').read_text()
a = s.index('        if ((assistedVertical || !probeThing)')
b = s.index('\n        rdVector_Copy3(&prev', a)
block = s[a:b]
f = s.index('static int sithBot_PositionHasWalkableFootprint(', s.index('static int sithBot_GetWalkableFloorDrop'))
center = s[s.index('    if (!sithBot_PositionHasWalkableFloorWithRise', f):]
center = center[:center.index('\n\n')]
code = r'''
#include <assert.h>
#include <stdio.h>
int assistedVertical, probeThing, sector, sample, flatDir;
int floorOK, edgeOK, upward, queries;
int sithBot_IsUpwardThrustSector(int s) { return upward; }
int sithBot_PositionHasWalkableFloor(int t,int s,void *p) { ++queries; return floorOK; }
int sithBot_PositionHasWalkableFloorWithRise(int t,int s,void *p,double rise) { ++queries;return floorOK; }
int sithBot_PositionHasWalkableFootprint(int probeThing,int sector,void *pos,void *d,double stepHeight) {
CENTER
 return edgeOK;
}
int check(void) {
BLOCK
 return 1;
}
int main(void) {
 int mask;
 for(mask=0;mask<32;++mask) {
  int expected;
  assistedVertical=!!(mask&1);probeThing=!!(mask&2);
  upward=!!(mask&4);floorOK=!!(mask&8);edgeOK=!!(mask&16);
  expected=((assistedVertical && upward) || floorOK)
    && (assistedVertical || !probeThing || (floorOK && edgeOK));
  queries=0;
  assert(check()==expected);
  assert(queries<=1);
 }
 puts("PASS: all 32 segment modes retain floor/footprint rejection with at most one center query");
}
'''.replace('CENTER', center).replace('BLOCK', block)
with tempfile.TemporaryDirectory() as d:
    c=Path(d)/'test.c'; exe=Path(d)/'test.exe'; c.write_text(code)
    subprocess.run([shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe',str(c),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
