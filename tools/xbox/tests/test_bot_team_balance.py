"""Run production bot team selection against human/bot roster combinations."""
from pathlib import Path
import tempfile,subprocess,shutil
root=Path(__file__).resolve().parents[3]
s=(root/'src/AI/sithBot.c').read_text(); a=s.index('static int sithBot_ChooseTeam('); b=s.index('static void sithBot_ActivateSlot',a)
code=r"""
#include <assert.h>
#include <string.h>
#include <stdio.h>
typedef struct { int flags,teamNum; } sithPlayerInfo;
sithPlayerInfo jkPlayer_playerInfos[32];
int jkPlayer_maxPlayers=32,teamMode=1;
int sithBot_IsTeamMode(void) { return teamMode; }
float _frand(void) { return .25f; }
SOURCE
int main(void) {
 int locals,n,i,c1,c2;
 for(locals=1;locals<=4;locals++) for(n=0;n<=8;n++) {
  memset(jkPlayer_playerInfos,0,sizeof(jkPlayer_playerInfos));
  for(i=0;i<locals;i++) { jkPlayer_playerInfos[i].flags=1; jkPlayer_playerInfos[i].teamNum=(i&1)+1; }
  for(i=locals;i<locals+n;i++) { jkPlayer_playerInfos[i].flags=1; jkPlayer_playerInfos[i].teamNum=sithBot_ChooseTeam(i); }
  c1=c2=0;
  for(i=0;i<locals+n;i++) { c1+=jkPlayer_playerInfos[i].teamNum==1; c2+=jkPlayer_playerInfos[i].teamNum==2; }
  assert(c1-c2<=1 && c2-c1<=1);
 }
 teamMode=0; assert(sithBot_ChooseTeam(4)==0);
 puts("PASS: 1-4 locals plus 0-8 bots balance within one player; free-for-all stays unassigned");
}
""".replace('SOURCE',s[a:b])
with tempfile.TemporaryDirectory() as tmp:
 c=Path(tmp)/'test.c';exe=Path(tmp)/'test.exe';c.write_text(code)
 subprocess.run([shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe',str(c),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
