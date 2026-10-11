/* Visual-only adaptation of OpenJPB's jpb_saber_fan_sector and its relative
 * blade history. Collision continues to use JK's authored live blade. */
#if defined(QOL_IMPROVEMENTS) && defined(TARGET_XBOX)
typedef struct jkModernHistory {
    sithThing *owner;
    uint32_t signature, stamp, motionStamp;
    rdVector3 base, blade, previous;
    int valid, sweep;
    rdKeyframe *attackKey;
    int attackWindow, sampleCount;
    rdVector3 samples[8];
    uint32_t sampleTimes[8];
} jkModernHistory;
static jkModernHistory jkModernHistories[4][64][2][2];
static rdMaterial *jkModernMaterial;

/* Boss keys use the same 4/5 markers as the collision callback. Inspect the
 * live track as well as the flag: an interrupted key may never deliver 5.
 * Each marked swing of a multi-swing key gets a separate visual history. */
static int jkModern_BossAttack(sithThing *actor, rdKeyframe **key)
{
    int i, j, window;
    *key=NULL;
    if(!actor->rdthing.puppet || (actor->thingflags&SITH_TF_DEAD)
       || actor->type==SITH_THING_CORPSE || (actor->jkFlags&JKFLAG_SABERRETRACT)) return 0;
    for(i=0;i<4;i++) {
        rdPuppetTrack *t=&actor->rdthing.puppet->tracks[i];
        if(!t->status || (t->status&0x218) || !t->keyframe) continue;
        window=0;
        for(j=0;j<(int)t->keyframe->numMarkers && j<8;j++) {
            if(t->keyframe->markers.marker_float[j]>t->field_120) break;
            if(t->keyframe->markers.marker_int[j]==4) window=1+j+i*8;
            if(t->keyframe->markers.marker_int[j]==5) window=0;
        }
        if(window) { *key=t->keyframe; return window; }
    }
    return 0;
}

/* A 50 ms shutter keeps slower boss sweeps visible at high render rates.
 * Interpolate real poses; never extrapolate beyond the authored motion.
 * The sector geometry/color/feather and player trail remain unchanged. */
static rdVector3 jkModern_BossPrevious(jkModernHistory *h, const rdVector3 *blade, uint32_t now)
{
    rdVector3 result=*blade;
    int i, n=h->sampleCount;
    if(n) {
        result=h->samples[0];
        for(i=0;i<n;i++) {
            uint32_t age=now-h->sampleTimes[i];
            if(age>=50) result=h->samples[i];
            else {
                if(i>0) {
                    uint32_t older=now-h->sampleTimes[i-1];
                    float t=(older-50.0f)/(older-age);
                    result.x=h->samples[i-1].x+t*(h->samples[i].x-h->samples[i-1].x);
                    result.y=h->samples[i-1].y+t*(h->samples[i].y-h->samples[i-1].y);
                    result.z=h->samples[i-1].z+t*(h->samples[i].z-h->samples[i-1].z);
                }
                break;
            }
        }
        /* If a slow frame straddles the window, interpolate to today's pose. */
        if(now-h->sampleTimes[n-1]>50) {
            float t=1.0f-50.0f/(now-h->sampleTimes[n-1]);
            result.x+=t*(blade->x-result.x);
            result.y+=t*(blade->y-result.y);
            result.z+=t*(blade->z-result.z);
        }
    }
    /* Eight samples spaced at least 8 ms cover the shutter at 120+ FPS. */
    if(!n || now-h->sampleTimes[n-1]>=8) {
        if(n==8) {
            for(i=1;i<n;i++) { h->samples[i-1]=h->samples[i]; h->sampleTimes[i-1]=h->sampleTimes[i]; }
            --n;
        }
        h->samples[n]=*blade; h->sampleTimes[n]=now; h->sampleCount=n+1;
    }
    return result;
}

typedef struct jkModernVertex { rdVector3 p; float c[4]; } jkModernVertex;

/* Clip RGBA together with position at the near plane. Side clipping is done
 * by the viewport; no invalid behind-camera coordinates reach projection. */
static void jkModern_Triangle(const rdVector3 *a, const rdVector3 *b,
    const rdVector3 *c, uint32_t ca, uint32_t cb, uint32_t cc)
{
    jkModernVertex in[3], out[4];
    const rdVector3 *points[3] = {a,b,c};
    uint32_t colors[3] = {ca,cb,cc};
    float nearY = rdCamera_pCurCamera->pClipFrustum->zNear;
    int i, k, n = 0;
    rdProcEntry *face;
    rdVector3 positions[4];
    if (nearY < .0001f) nearY = .0001f;
    for (i=0;i<3;i++) {
        in[i].p = *points[i];
        for (k=0;k<4;k++) in[i].c[k]=(float)((colors[i]>>(k*8))&255);
    }
    for (i=0;i<3;i++) {
        jkModernVertex *v=&in[i], *w=&in[(i+1)%3];
        int inside=v->p.y>=nearY, next=w->p.y>=nearY;
        if (inside) out[n++]=*v;
        if (inside!=next) {
            float t=(nearY-v->p.y)/(w->p.y-v->p.y);
            out[n].p.x=v->p.x+t*(w->p.x-v->p.x);
            out[n].p.y=nearY;
            out[n].p.z=v->p.z+t*(w->p.z-v->p.z);
            for(k=0;k<4;k++) out[n].c[k]=v->c[k]+t*(w->c[k]-v->c[k]);
            n++;
        }
    }
    if(n<3) return;
    face=rdCache_GetProcEntry();
    if(!face) return;
    face->geometryMode=RD_GEOMODE_SOLIDCOLOR;
    face->lightingMode=RD_LIGHTMODE_FULLYLIT;
    face->textureMode=0;
    face->material=jkModernMaterial;
    face->wallCel=0;
    face->type=RD_FF_DOUBLE_SIDED|RD_FF_TEX_TRANSLUCENT|RD_FF_ZWRITE_DISABLED;
    face->light_flags=0;
    face->extralight=0;
    face->ambientLight=1;
    face->light_level_static=1;
    face->hasEffectColors=1;
    for(i=0;i<n;i++) {
        positions[i]=out[i].p;
        face->effectColors[i]=0;
        for(k=0;k<4;k++) face->effectColors[i]|=(uint32_t)(out[i].c[k]+.5f)<<(k*8);
    }
    rdCamera_pCurCamera->fnProjectLst(face->vertices,positions,n);
    rdCache_AddProcFace(0,n,1);
}

static uint32_t jkModern_Color(uint32_t rgb, float alpha)
{
    int shift;
    uint32_t result=(uint32_t)(alpha*255)<<24;
    for(shift=0;shift<24;shift+=8)
        result|=(uint32_t)(255*alpha+((rgb>>shift)&255)*(1-alpha))<<shift;
    return result;
}

static void jkModern_Fan(const rdVector3 *base, const rdVector3 *blade,
    const rdVector3 *previous, uint32_t rgb)
{
    static const float angles[8]={0,.06f,.18f,.38f,.60f,.78f,.91f,1};
    static const float aa[8]={1,1,1,1,1,.75f,.22f,0};
    static const float radii[6]={0,.16f,1,1.0125f,1.03f,1.05f};
    static const float ra[6]={0,1,1,.65f,.18f,0};
    rdVector3 mesh[8][6];
    uint32_t colors[8][6];
    float len=rdVector_Len3(blade), oldLen=rdVector_Len3(previous);
    int a,r;
    for(a=0;a<8;a++) {
        rdVector3 dir;
        float t=angles[a], magnitude;
        dir.x=(1-t)*blade->x+t*previous->x;
        dir.y=(1-t)*blade->y+t*previous->y;
        dir.z=(1-t)*blade->z+t*previous->z;
        magnitude=rdVector_Len3(&dir);
        if(magnitude<.00001f) return;
        for(r=0;r<6;r++) {
            rdVector3 world;
            float scale=radii[r]*((1-t)*len+t*oldLen)/magnitude;
            world.x=base->x+dir.x*scale;
            world.y=base->y+dir.y*scale;
            world.z=base->z+dir.z*scale;
            rdMatrix_TransformPoint34(&mesh[a][r],&world,&rdCamera_pCurCamera->view_matrix);
            colors[a][r]=jkModern_Color(rgb,aa[a]*ra[r]);
        }
    }
    for(a=0;a<7;a++) for(r=0;r<5;r++) {
        if(r) jkModern_Triangle(&mesh[a][r],&mesh[a+1][r],&mesh[a+1][r+1],colors[a][r],colors[a+1][r],colors[a+1][r+1]);
        jkModern_Triangle(&mesh[a][r],&mesh[a+1][r+1],&mesh[a][r+1],colors[a][r],colors[a+1][r+1],colors[a][r+1]);
    }
}

/* Camera-facing tapered white blade with a colored feather. Uses the same
 * unlit RGBA path as the fan, so level lighting cannot stain its core. */
/* Match the blade's white core and feather at its seam, then round the end.
 * Camera-space geometry also gives an end-on blade a circular silhouette. */
static void jkModern_Tip(const rdVector3 *tip,float radius,float nx,float nz,
    float tx,float tz,uint32_t rgb,int endOn)
{
    static const float along[9]={0,.38268343f,.70710678f,.92387953f,1,.92387953f,.70710678f,.38268343f,0};
    static const float across[9]={-1,-.92387953f,-.70710678f,-.38268343f,0,.38268343f,.70710678f,.92387953f,1};
    static const float widths[4]={0,.23f,.90f,1.75f};
    static const float alpha[4]={1,1,.52f,0};
    rdVector3 mesh[9][4];
    uint32_t colors[4];
    int half,a,r;
    for(r=0;r<4;r++) colors[r]=jkModern_Color(rgb,alpha[r]);
    for(half=0;half<(endOn?2:1);half++) {
        float sign=half?-1.0f:1.0f;
        for(a=0;a<9;a++) for(r=0;r<4;r++) {
            float distance=radius*widths[r];
            mesh[a][r]=*tip;
            mesh[a][r].x+=distance*(nx*across[a]+sign*tx*along[a]);
            mesh[a][r].z+=distance*(nz*across[a]+sign*tz*along[a]);
        }
        for(a=0;a<8;a++) for(r=0;r<3;r++) {
            if(r) jkModern_Triangle(&mesh[a][r],&mesh[a+1][r],&mesh[a+1][r+1],colors[r],colors[r],colors[r+1]);
            jkModern_Triangle(&mesh[a][r],&mesh[a+1][r+1],&mesh[a][r+1],colors[r],colors[r+1],colors[r+1]);
        }
    }
}

static void jkModern_Blade(const rdVector3 *base,const rdVector3 *tip,float radius,uint32_t rgb)
{
    rdVector3 b,t,points[2][6];
    static const float widths[6]={-1.75f,-.90f,-.23f,.23f,.90f,1.75f};
    static const float alpha[6]={0,.52f,1,1,.52f,0};
    float dx,dz,mag,nx,nz;
    int i,j;
    rdMatrix_TransformPoint34(&b,base,&rdCamera_pCurCamera->view_matrix);
    rdMatrix_TransformPoint34(&t,tip,&rdCamera_pCurCamera->view_matrix);
    dx=t.x-b.x; dz=t.z-b.z; mag=sqrtf(dx*dx+dz*dz);
    nx=mag>.000001f?dz/mag:1; nz=mag>.000001f?-dx/mag:0;
    for(i=0;i<2;i++) for(j=0;j<6;j++) {
        float w=radius*widths[j]*(i?.55f:1);
        points[i][j]=i?t:b;
        points[i][j].x+=nx*w; points[i][j].z+=nz*w;
    }
    for(j=0;j<5;j++) {
        uint32_t ca=jkModern_Color(rgb,alpha[j]), cb=jkModern_Color(rgb,alpha[j+1]);
        jkModern_Triangle(&points[0][j],&points[1][j],&points[1][j+1],ca,ca,cb);
        jkModern_Triangle(&points[0][j],&points[1][j+1],&points[0][j+1],ca,cb,cb);
    }
    jkModern_Tip(&t,radius*.55f,nx,nz,mag>.000001f?dx/mag:0,
        mag>.000001f?dz/mag:1,rgb,mag<=.000001f);
}
#endif

void jkSaber_DrawBlade(sithThing *actor,rdThing *line,rdMatrix34 *matrix,int slot,int pov)
{
#if defined(QOL_IMPROVEMENTS) && defined(TARGET_XBOX)
    jkModernHistory *h=&jkModernHistories[playerThingIdx&3][actor->thingIdx&63][slot&1][pov&1];
    if(jkPlayer_bModernSaber && line->polyline && line->polyline->length>0) {
        rdVector3 local={0},tip,blade,previous,renderBase,renderBlade;
        uint32_t now=sithTime_curMs;
        uint32_t rgb=actor->actorParams.field_1A8;
        int boss=(actor->actorParams.typeflags&SITH_AF_BOSS)!=0;
        int attacking=(actor->jkFlags&JKFLAG_SABERDAMAGE)!=0;
        int fan=0, window=0;
        rdKeyframe *attackKey=NULL;
        float len=line->polyline->length, delta, renderOffset;
        jkModernMaterial=line->polyline->edgeFace.material;
        local.y=len;
        rdMatrix_TransformPoint34(&tip,&local,matrix);
        rdVector_Sub3(&blade,&tip,&matrix->scale);
        renderBase=matrix->scale;
        renderOffset=line->polyline->baseRadius*2.6f;
        if(renderOffset<len*.08f) renderOffset=len*.08f;
        if(renderOffset>len*.16f) renderOffset=len*.16f;
        if(!pov && len>.00001f) {
            renderBase.x+=blade.x*(renderOffset/len);
            renderBase.y+=blade.y*(renderOffset/len);
            renderBase.z+=blade.z*(renderOffset/len);
        }
        rdVector_Sub3(&renderBlade,&tip,&renderBase);
        previous=h->blade;
        if(h->owner!=actor || h->signature!=actor->signature || now-h->stamp>100 || rdVector_Dist3(&renderBase,&h->base)>.5f) h->valid=0;
        if(boss) {
            window=jkModern_BossAttack(actor,&attackKey);
            attacking=attacking && window;
            if(h->attackKey!=attackKey || h->attackWindow!=window) h->valid=0;
            if(!h->valid || !attacking) h->sampleCount=0;
            if(attacking) previous=jkModern_BossPrevious(h,&renderBlade,now);
            h->attackKey=attackKey; h->attackWindow=window;
        }
        delta=rdVector_Dist3(&renderBlade,&previous);
        if(!attacking) h->valid=0;
        if(h->valid) {
            if(!boss) {
                if(delta>.00001f) { h->previous=previous; h->motionStamp=now; h->sweep=delta<len*1.9f; }
                else if(h->sweep && now-h->motionStamp<=17) { previous=h->previous; delta=rdVector_Dist3(&renderBlade,&previous); }
            }
            if(delta>.00001f && delta<len*1.9f) { jkModern_Fan(&renderBase,&renderBlade,&previous,rgb); fan=1; }
        } else h->sweep=0;
#ifndef JKSABER_UNIT_TEST
        {
            static int trace=-1, remaining=1200;
            if(trace<0) {
                FILE *f=fopen("D:\\xbox_smoke_boss_saber.txt","rb");
                trace=f!=NULL;
                if(f) fclose(f);
            }
            if(trace && remaining>0 && (actor->actorParams.typeflags&SITH_AF_BOSS) && (actor->jkFlags&JKFLAG_SABERDAMAGE)) {
                int i;
                --remaining;
                XPERF("BossSaber: t=%u actor=%d slot=%d flags=%x valid=%d gap=%u delta=%.5f len=%.4f frame=%d fan=%d window=%d\n",now,actor->thingIdx,slot,actor->jkFlags,h->valid,now-h->stamp,delta,len,rdroid_frameTrue,fan,window);
                if(actor->rdthing.puppet) for(i=0;i<4;i++) {
                    rdPuppetTrack *t=&actor->rdthing.puppet->tracks[i];
                    if(t->status && t->keyframe) XPERF("BossTrack: %d key=%s status=%x frame=%.2f\n",i,t->keyframe->name,t->status,t->field_120);
                }
            }
        }
#endif
        h->owner=actor; h->signature=actor->signature; h->stamp=now;
        h->blade=renderBlade; h->base=renderBase;
        h->valid=attacking;
        jkModern_Blade(&renderBase,&tip,line->polyline->baseRadius,rgb);
        return;
    }
    h->valid=0;
#endif
    rdThing_Draw(line,matrix);
}
