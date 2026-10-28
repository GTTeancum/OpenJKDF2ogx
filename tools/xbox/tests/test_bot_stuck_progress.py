"""Normal authored-speed progress must not discard a committed route."""
from pathlib import Path
import shutil
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[3] / 'src/AI/sithBot.c').read_text()
start = source.index('static void sithBot_CheckStuck(SithBotState *state, sithThing *thing, const rdVector3 *target)\n{')
end = source.index('static void sithBot_SyncPositionIfNeeded(', start)
code = r'''
#include <math.h>
#include <stdio.h>
#include <string.h>
#define SITHBOT_GOAL_CTF 5
#define SITHBOT_CTF_DEFEND_BASE 5
#define SITH_ATTACH_NO_MOVE 8
#define SITH_DAMAGE_SABER 1
#define SITHBOT_STUCK_MS 600
typedef double flex_t;
typedef struct {double x,y,z;} rdVector3;
typedef struct {
 int enemyVisibleCached,goalNode,goalMode,ctfObjective,hardStuckSinceMs,hardStuckFailures;
 int ridingLiftThingIdx,liftExitNode,interactionWaitUntilMs,lastMoveCheckMs;
 int stuckTicks,nextUseMs,interactionRepeatUntilMs,nextNode,routeGoalNode,routeCommitUntilMs;
 int steeringUntilMs,steeringTargetNode,steeringCombat,nextGoalMs,playerIdx;
 rdVector3 hardStuckPos,lastMovePos,steeringDir;
} SithBotState;
typedef struct {rdVector3 position;int attach_flags;struct {double health;} actorParams;} sithThing;
static struct {rdVector3 pos;} sithBot_nodes[2];
static int sithBot_numNodes=2,sithTime_curMs=1600,sithBot_debugJumpsLogged=0;
static int escapes,recoveries,jumps;
static double sithBot_DistSq(const rdVector3 *a,const rdVector3 *b) {
 return (a->x-b->x)*(a->x-b->x)+(a->y-b->y)*(a->y-b->y)+(a->z-b->z)*(a->z-b->z);
}
static double rdVector_Dist3(const rdVector3 *a,const rdVector3 *b) {return sqrt(sithBot_DistSq(a,b));}
static void rdVector_Copy3(rdVector3 *a,const rdVector3 *b) {*a=*b;}
static void rdVector_Sub3(rdVector3 *a,const rdVector3 *b,const rdVector3 *c) {a->x=b->x-c->x;a->y=b->y-c->y;a->z=b->z-c->z;}
static double rdVector_Normalize3Acc(rdVector3 *a) {double n=sqrt(a->x*a->x+a->y*a->y+a->z*a->z);if(n){a->x/=n;a->y/=n;a->z/=n;}return n;}
static int sithBot_TryRecoverHardStuck(SithBotState *s,sithThing *t) {recoveries++;return 1;}
static void sithBot_Logf(const char *fmt,...) {}
static void sithThing_Damage(sithThing *a,sithThing *b,double damage,int flags) {}
static void sithBot_FaceToward(SithBotState *s,sithThing *t,const rdVector3 *p,int combat) {}
static void sithPlayerActions_JumpWithVel(sithThing *t,double v) {jumps++;}
static void sithPlayerActions_Activate(sithThing *t) {}
static int sithBot_TryActivateNearbyInteraction(SithBotState *s,sithThing *t,const rdVector3 *p,int a,int b) {return 0;}
static int sithBot_FindSafeMoveDir(sithThing *t,rdVector3 *p,double speed,double dz,double *out) {escapes++;return 1;}
#define stdMath_Sqrt sqrt
#define CHECK(x) do {if (!(x)) {fprintf(stderr,"failure at %d\n",__LINE__);return 1;}} while(0)
BODY
static SithBotState initial(void) {
 SithBotState s;memset(&s,0,sizeof(s));
 s.goalNode=1;s.nextNode=327;s.routeGoalNode=1;s.routeCommitUntilMs=4000;
 s.lastMoveCheckMs=1000;s.hardStuckSinceMs=1000;s.nextUseMs=9999;
 s.ridingLiftThingIdx=-1;s.liftExitNode=-1;
 s.lastMovePos=(rdVector3){.25,1.49,.62};s.hardStuckPos=s.lastMovePos;
 escapes=recoveries=jumps=0;sithTime_curMs=1600;
 return s;
}
int main(void) {
 sithBot_nodes[1].pos=(rdVector3){-3.4,0,.34};
 rdVector3 target={-.26,1.50,.54};
 sithThing thing={{.02,1.51,.62},1,{100}};
 SithBotState s=initial();
 /* c1 measured approach: .23 units in .6 seconds, still moving toward 327. */
 sithBot_CheckStuck(&s,&thing,&target);
 CHECK(s.nextNode==327 && s.routeCommitUntilMs==4000 && escapes==0 && s.hardStuckFailures==0);
 /* Slower steady progress remains valid; drift at a wall still recovers. */
 s=initial();thing.position=(rdVector3){.17,1.49,.62};
 sithBot_CheckStuck(&s,&thing,&target);CHECK(s.nextNode==327 && escapes==0);
 s=initial();thing.position=(rdVector3){.24,1.49,.62};
 sithBot_CheckStuck(&s,&thing,&target);CHECK(s.nextNode==-1 && escapes==1 && s.hardStuckFailures==1);
 /* Keep the original time gate and destination hold. */
 s=initial();sithTime_curMs=1599;
 sithBot_CheckStuck(&s,&thing,&target);CHECK(s.nextNode==327 && escapes==0);
 s=initial();target=thing.position;
 sithBot_CheckStuck(&s,&thing,&target);CHECK(s.nextNode==327 && escapes==0);
 /* Long-term recovery remains available after repeated genuine failures. */
 s=initial();sithTime_curMs=20000;s.hardStuckFailures=4;
 target=(rdVector3){-.26,1.50,.54};
 sithBot_CheckStuck(&s,&thing,&target);CHECK(recoveries==1);
 return 0;
}
'''.replace('BODY', source[start:end])
with tempfile.TemporaryDirectory() as directory:
    c = Path(directory) / 'test.c'
    executable = Path(directory) / 'test.exe'
    c.write_text(code)
    subprocess.run([shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe',
                    str(c), '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
print('PASS: measured walking retains its route; stationary, timing and long-term recovery cases preserved')
