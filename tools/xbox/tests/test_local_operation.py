from pathlib import Path
import subprocess, tempfile, shutil
root=Path(__file__).resolve().parents[3]
s=(root/'src/Platform/Xbox/xbox_splitscreen.c').read_text()
a=s.index('int xboxSplitScreen_BeginLocalOperation(')
b=s.index('void xboxSplitScreen_SetContextForControllerPort(',a)
code=r'''
#include <assert.h>
#include <stdio.h>
static int g_xboxSplitScreenCurrentSlot, liveWeapon=10, saved[4]={10,20,30,40};
static void xboxSplitScreen_SaveTransientStateForSlot(int n){saved[n]=liveWeapon;}
static void xboxSplitScreen_ApplyTransientStateForSlot(int n){liveWeapon=saved[n];}
static void xboxSplitScreen_SetContextForLocalSlot(int n){g_xboxSplitScreenCurrentSlot=n;}
SOURCE
int main(void){
 int old,nested;
 liveWeapon=11;
 old=xboxSplitScreen_BeginLocalOperation(2);
 assert(liveWeapon==30 && g_xboxSplitScreenCurrentSlot==2);
 liveWeapon=31;
 nested=xboxSplitScreen_BeginLocalOperation(1);
 liveWeapon=21;
 xboxSplitScreen_EndLocalOperation(nested);
 assert(liveWeapon==31 && g_xboxSplitScreenCurrentSlot==2);
 xboxSplitScreen_EndLocalOperation(old);
 assert(liveWeapon==11 && saved[1]==21 && saved[2]==31 && g_xboxSplitScreenCurrentSlot==0);
 old=xboxSplitScreen_BeginLocalOperation(0);liveWeapon=12;
 xboxSplitScreen_EndLocalOperation(old);assert(liveWeapon==12 && saved[0]==12);
 puts("PASS: callbacks preserve other players' weapon state, including nested and same-slot operations");
}
'''.replace('SOURCE',s[a:b])
with tempfile.TemporaryDirectory() as tmp:
 c=Path(tmp)/'test.c';exe=Path(tmp)/'test.exe';c.write_text(code)
 subprocess.run([shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe',str(c),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
