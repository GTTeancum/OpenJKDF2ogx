#include "jkGUIXboxControls.h"
#ifdef TARGET_XBOX
#include "Gui/jkGUI.h"
#include "Gui/jkGUISetup.h"
#include "Gui/jkGUIRend.h"
#include "General/stdBitmap.h"
#include "General/stdFont.h"
#include "Platform/stdControl.h"
#include "Platform/std3D.h"
#include "Platform/wuRegistry.h"
#include "Platform/Xbox/xbox_control_map.h"
#include "Win95/stdDisplay.h"
#include "stdPlatform.h"
#include "jk.h"

#define ROW_FIRST 10
#define VISIBLE_ROWS 6
#define ROW_HEIGHT 40
#define EDIT_ACTION 300
#define DEFAULT_ACTION 301
#define OPTION_FIRST 18
#define ROW_COUNT 23

static void drawPanel(jkGuiElement*, jkGuiMenu*, stdVBuffer*, BOOL);
static void drawRow(jkGuiElement*, jkGuiMenu*, stdVBuffer*, BOOL);
static void drawTab(jkGuiElement*, jkGuiMenu*, stdVBuffer*, BOOL);
static void drawHint(jkGuiElement*, jkGuiMenu*, stdVBuffer*, BOOL);
static void footer(void);
static int clickRow(jkGuiElement*, jkGuiMenu*, int, int, BOOL);
static void smokeTick(jkGuiMenu*);

static jkGuiElement elements[] = {
    {ELEMENT_CUSTOM,0,0,0,3,{48,408,560,24},1,0,0,drawHint},
    {ELEMENT_TEXT,0,6,"GUI_SETUP",3,{20,20,600,40},1},
    {ELEMENT_TEXTBUTTON,100,2,"GUI_GENERAL",3,{20,80,120,40},1,0,0,drawTab},
    {ELEMENT_TEXTBUTTON,101,2,"GUI_GAMEPLAY",3,{140,80,120,40},1,0,0,drawTab},
    {ELEMENT_TEXTBUTTON,102,2,"GUI_DISPLAY",3,{260,80,120,40},1,0,0,drawTab},
    {ELEMENT_TEXTBUTTON,103,2,"GUI_SOUND",3,{380,80,120,40},1,0,0,drawTab},
    {ELEMENT_TEXT,104,3,"GUI_CONTROLS",3,{500,80,120,40},1,0,0,drawTab},
    {ELEMENT_CUSTOM,0,0,0,0,{48,160,564,240},1,0,0,drawPanel},
    {ELEMENT_TEXT,0,2,L"Action",0,{60,130,400,24},1},
    {ELEMENT_TEXT,0,2,L"Button",3,{475,130,100,24},1},
    {ELEMENT_TEXTBUTTON,EDIT_ACTION,2,0,0,{49,160,540,40},1,0,0,drawRow,clickRow},
    {ELEMENT_TEXTBUTTON,EDIT_ACTION,2,0,0,{49,200,540,40},1,0,0,drawRow,clickRow},
    {ELEMENT_TEXTBUTTON,EDIT_ACTION,2,0,0,{49,240,540,40},1,0,0,drawRow,clickRow},
    {ELEMENT_TEXTBUTTON,EDIT_ACTION,2,0,0,{49,280,540,40},1,0,0,drawRow,clickRow},
    {ELEMENT_TEXTBUTTON,EDIT_ACTION,2,0,0,{49,320,540,40},1,0,0,drawRow,clickRow},
    {ELEMENT_TEXTBUTTON,EDIT_ACTION,2,0,0,{49,360,540,40},1,0,0,drawRow,clickRow},
    {ELEMENT_END}
};
static jkGuiMenu controls = {elements,0,0xFF,0xE1,0xF,0,0,jkGui_stdBitmaps,jkGui_stdFonts,0,smokeTick,"thermloop01.wav","thrmlpu2.wav"};

static const wchar_t *names[ROW_COUNT] = {
    L"Fire", L"Alternate fire", L"Jump", L"Use / activate",
    L"Previous weapon", L"Next weapon", L"Previous Force power", L"Next Force power",
    L"Move", L"Look", L"Sprint toggle", L"Crouch toggle", L"Map", L"Pause",
    L"Field light", L"IR goggles", L"Bacta", L"Third-person camera",
    L"Look sensitivity X", L"Look sensitivity Y", L"Stick deadzone", L"Invert look", L"Vibration"
};
static const int logicalButtons[8] = {7,6,0,2,4,5,3,1};
static const wchar_t *physicalNames[8] = {L"A",L"B",L"X",L"Y",L"Black",L"White",L"Left trigger",L"Right trigger"};
static const char *glyphPaths[18] = {
    "ui\\bm\\xbtn_tc_a.bm", "ui\\bm\\xbtn_tc_b.bm", "ui\\bm\\xbtn_tc_x.bm", "ui\\bm\\xbtn_tc_y.bm",
    "ui\\bm\\xbtn_tc_black.bm", "ui\\bm\\xbtn_tc_white.bm", "ui\\bm\\xbtn_tc_lt.bm", "ui\\bm\\xbtn_tc_rt.bm",
    "ui\\bm\\xbtn_tc_back.bm", "ui\\bm\\xbtn_tc_start.bm",
    "ui\\bm\\xbtn_tc_ls.bm", "ui\\bm\\xbtn_tc_rs.bm",
    "ui\\bm\\xbtn_tc_ls_click.bm", "ui\\bm\\xbtn_tc_rs_click.bm",
    "ui\\bm\\xbtn_tc_dpad_up.bm", "ui\\bm\\xbtn_tc_dpad_left.bm",
    "ui\\bm\\xbtn_tc_dpad_right.bm", "ui\\bm\\xbtn_tc_dpad_down.bm"
};
static stdBitmap *glyphs[18];
static int buttonMap[8], options[5];
static int selected, top, editing = -1, savedSelection, savedTop;
static uint8_t dark[256], warm[256], amber, orange, brown;
static unsigned int smokeStart;
static int smokePhase, smokeEnabled;

static uint8_t nearest(int r, int g, int b)
{
    int i, best=0, distance=0x7fffffff;
    for (i=0;i<256;i++) {
        int dr=r-stdDisplay_masterPalette[i].r, dg=g-stdDisplay_masterPalette[i].g, db=b-stdDisplay_masterPalette[i].b;
        int d=dr*dr+dg*dg+db*db;
        if(d<distance) {distance=d;best=i;}
    }
    return (uint8_t)best;
}

static void palette(void)
{
    int i;
    amber=nearest(255,177,32); orange=nearest(213,114,12); brown=nearest(76,36,6);
    for(i=0;i<256;i++) {
        rdColor24 c=stdDisplay_masterPalette[i];
        dark[i]=nearest(c.r*26/100,c.g*26/100,c.b*26/100);
        warm[i]=nearest(c.r*20/100+74,c.g*20/100+33,c.b*20/100+3);
    }
}

static void shade(stdVBuffer *vbuf, rdRect *rect, const uint8_t *map)
{
    int x,y;
    stdDisplay_VBufferLock(vbuf);
    for(y=rect->y;y<rect->y+rect->height;y++) {
        uint8_t *row=(uint8_t*)vbuf->surface_lock_alloc+y*vbuf->format.width_in_bytes;
        for(x=rect->x;x<rect->x+rect->width;x++) row[x]=map[row[x]];
    }
    stdDisplay_VBufferUnlock(vbuf);
}

static int count(void) {return editing>=0 ? 8 : ROW_COUNT;}

static void drawTab(jkGuiElement *element,jkGuiMenu *menu,stdVBuffer *vbuf,BOOL redraw)
{
    if(redraw) jkGuiRend_CopyVBuffer(menu,&element->rect);
    if(element==&elements[6] || menu->lastMouseOverClickable==element) {
        shade(vbuf,&element->rect,warm);
        jkGuiRend_DrawRect(vbuf,&element->rect,amber);
    }
    stdFont_Draw3(vbuf,menu->fonts[element==&elements[6] ? 3 : 2],element->rect.y,&element->rect,3,element->wstr,1);
}

static void drawPanel(jkGuiElement *element,jkGuiMenu *menu,stdVBuffer *vbuf,BOOL redraw)
{
    rdRect border={48,160,542,240}, track={598,164,12,232}, thumb;
    rdRect line={20,123,600,1};
    jkGuiRend_DrawRect(vbuf,&border,orange);
    stdDisplay_VBufferFill(vbuf,orange,&line);
    stdDisplay_VBufferFill(vbuf,brown,&track);
    jkGuiRend_DrawRect(vbuf,&track,orange);
    thumb=track; thumb.x+=2; thumb.width-=4;
    thumb.height=(track.height-4)*VISIBLE_ROWS/count();
    thumb.y+=2+(track.height-4-thumb.height)*top/(count()-VISIBLE_ROWS);
    stdDisplay_VBufferFill(vbuf,orange,&thumb);
    jkGuiRend_DrawRect(vbuf,&thumb,amber);
}

static void drawHint(jkGuiElement *element,jkGuiMenu *menu,stdVBuffer *vbuf,BOOL redraw)
{
    const wchar_t *hint=L"";
    if(redraw) jkGuiRend_CopyVBuffer(menu,&element->rect);
    if(editing<0 && selected>=4 && selected<8) hint=L"Tap to cycle; hold to open the wheel.";
    else if(editing<0 && selected>=8 && selected<OPTION_FIRST) hint=L"Fixed control";
    else if(editing<0 && selected>=OPTION_FIRST) hint=L"Left / right to adjust";
    stdFont_Draw3(vbuf,menu->fonts[1],element->rect.y,&element->rect,2,hint,1);
}

static void drawRow(jkGuiElement *element,jkGuiMenu *menu,stdVBuffer *vbuf,BOOL redraw)
{
    int slot=(int)(element-&elements[ROW_FIRST]), row=top+slot;
    int active=(menu->lastMouseOverClickable==element);
    rdRect rect=element->rect, valueRect;
    wchar_t value[32];
    if(row>=count()) return;
    if(redraw) jkGuiRend_CopyVBuffer(menu,&rect);
    shade(vbuf,&rect,active?warm:dark);
    if(active) jkGuiRend_DrawRect(vbuf,&rect,amber);
    rect.x+=12;rect.width=398;
    stdFont_Draw3(vbuf,menu->fonts[active?3:2],rect.y,&rect,2,editing>=0?physicalNames[row]:names[row],1);
    if(editing>=0 || row<OPTION_FIRST) return;
    valueRect=element->rect;valueRect.x=453;valueRect.width=126;
    {
        int n=row-OPTION_FIRST;
        if(n>=3) jk_snwprintf(value,32,L"%s",options[n]?L"On":L"Off");
        else jk_snwprintf(value,32,n==2?L"< %d%% >":L"< %d >",options[n]);
        stdFont_Draw3(vbuf,menu->fonts[2],valueRect.y,&valueRect,3,value,1);
    }
}

static void drawGlyphs(void *ctx)
{
    int slot;
    for(slot=0;slot<VISIBLE_ROWS;slot++) {
        int row=top+slot, glyph=-1;
        if(row>=count()) break;
        if(editing>=0) glyph=row;
        else if(row<8) glyph=buttonMap[logicalButtons[row]];
        else if(row>=8 && row<=11) glyph=row+2;
        else if(row==12) glyph=8;
        else if(row==13) glyph=9;
        else if(row>=14 && row<OPTION_FIRST) glyph=row;
        if(glyph>=0 && glyphs[glyph]) {
            stdBitmap *bm=glyphs[glyph];
            stdBitmap_EnsureData(bm);
            if(bm->mipSurfaces && bm->mipSurfaces[0]) {
                stdVBuffer *src=bm->mipSurfaces[0];
                std3D_DrawUIBitmapRGBA(bm,0,503.0f,163.0f+slot*ROW_HEIGHT,NULL,
                    50.0f/src->format.width,35.0f/src->format.height,1,255,255,255,255);
            }
        }
    }
}

static int clickRow(jkGuiElement *element,jkGuiMenu *menu,int x,int y,BOOL clicked)
{
    selected=top+(int)(element-&elements[ROW_FIRST]);
    return clicked && (editing>=0 || selected<8 || selected>=OPTION_FIRST) ? EDIT_ACTION : 0;
}

static void clampSelection(void)
{
    if(selected<0) selected=0;
    if(selected>=count()) selected=count()-1;
    if(selected<top) top=selected;
    if(selected>=top+VISIBLE_ROWS) top=selected-VISIBLE_ROWS+1;
    if(top>count()-VISIBLE_ROWS) top=count()-VISIBLE_ROWS;
}

static void focusRow(void)
{
    clampSelection();
    controls.focusedElement=NULL;
    controls.lastMouseOverClickable=&elements[ROW_FIRST+selected-top];
    controls.lastMouseDownClickable=NULL;
}

static void changeOption(int direction)
{
    int n=selected-OPTION_FIRST, max;
    if(n<0 || n>=5) return;
    if(n>=3) options[n]=!options[n];
    else {
        max=n==2?30:100;
        options[n]+=direction*(n==2?1:5);
        if(options[n]<(n==2?0:1)) options[n]=n==2?0:1;
        if(options[n]>max) options[n]=max;
    }
}

int jkGuiXboxControls_Focus(jkGuiMenu *menu,int direction)
{
    int rowFocus;
    if(menu!=&controls) return 0;
    rowFocus=menu->lastMouseOverClickable && menu->lastMouseOverClickable>=&elements[ROW_FIRST]
        && menu->lastMouseOverClickable<&elements[ROW_FIRST+VISIBLE_ROWS];
    if(direction==FOCUS_NONE) return 1;
    if(!rowFocus) {
        if(direction==FOCUS_DOWN) focusRow();
        else if(direction==FOCUS_LEFT || direction==FOCUS_RIGHT) {
            int tab=menu->lastMouseOverClickable?(int)(menu->lastMouseOverClickable-elements):5;
            tab+=direction==FOCUS_LEFT?-1:1;
            if(tab<2) tab=2;
            if(tab>5) tab=5;
            menu->lastMouseOverClickable=&elements[tab];
        }
    } else {
        if(direction==FOCUS_UP && selected==0 && editing<0)
            menu->lastMouseOverClickable=&elements[5];
        else {
            if(direction==FOCUS_UP) --selected;
            if(direction==FOCUS_DOWN) ++selected;
            if(editing<0 && (direction==FOCUS_LEFT || direction==FOCUS_RIGHT))
                changeOption(direction==FOCUS_LEFT?-1:1);
            focusRow();
        }
    }
    footer();
    jkGuiRend_Paint(menu);
    return 1;
}

static void save(void)
{
    stdControl_XboxSetButtonMap(buttonMap);
    stdControl_XboxSetLookOptionsAxesEx(options[0],options[1],options[3],options[4],options[2]);
    wuRegistry_SaveInt("xboxLookSensitivity",options[0]);
    wuRegistry_SaveInt("xboxLookSensitivityX",options[0]);
    wuRegistry_SaveInt("xboxLookSensitivityY",options[1]);
    wuRegistry_SaveInt("xboxDeadzone",options[2]);
    wuRegistry_SaveBool("xboxInvertLook",options[3]);
    wuRegistry_SaveBool("xboxVibration",options[4]);
}

static void defaults(void)
{
    xboxControlMap_Defaults(buttonMap);
    options[0]=50;options[1]=75;options[2]=12;options[3]=0;options[4]=1;
}

static void edit(void)
{
    if(editing>=0) {
        xboxControlMap_Assign(buttonMap,editing,selected);
        editing=-1;selected=savedSelection;top=savedTop;
    } else if(selected<8) {
        savedSelection=selected;savedTop=top;editing=logicalButtons[selected];
        selected=buttonMap[editing];top=0;
    } else changeOption(1);
    focusRow();
}

static void footer(void)
{
    jkGuiElement *focus=controls.lastMouseOverClickable;
    int tab=focus && focus>=&elements[2] && focus<=&elements[6];
    jkGuiRend_XboxFooterBegin(&controls);
    if(editing>=0 || tab || selected<8 || selected>=OPTION_FIRST)
        jkGuiRend_XboxFooterAddAction(&controls,JKGUI_XBOX_BTN_A,0,editing>=0?L"Assign":tab?L"Select":L"Change");
    jkGuiRend_XboxFooterAddAction(&controls,JKGUI_XBOX_BTN_B,-1,editing>=0?L"Cancel":L"Back");
    if(editing<0) {
        jkGuiRend_XboxFooterAddAction(&controls,JKGUI_XBOX_BTN_X,DEFAULT_ACTION,L"Defaults");
        jkGuiRend_XboxFooterAddAction(&controls,JKGUI_XBOX_BTN_START,1,L"Done");
    }
}

int jkGuiXboxControls_IsMenu(jkGuiMenu *menu) { return menu == &controls && editing < 0; }

int jkGuiXboxControls_Show(void)
{
    int i,result;
    FILE *probe;
    stdControl_XboxGetButtonMap(buttonMap);
    options[0]=stdControl_XboxGetLookSensitivityX();options[1]=stdControl_XboxGetLookSensitivityY();
    options[2]=stdControl_XboxGetDeadzone();options[3]=stdControl_XboxGetInvertLook();options[4]=stdControl_XboxGetVibration();
    selected=top=0;editing=-1;
    smokeEnabled=0;smokePhase=-1;smokeStart=0;
    probe=fopen("D:\\xbox_smoke_setup.txt","rb");
    if(probe) {fscanf(probe,"%d",&smokeEnabled);fclose(probe);smokeEnabled=smokeEnabled==2;}
    for(i=0;i<18;i++) if(!glyphs[i]) glyphs[i]=stdBitmap_LoadPartial((char*)glyphPaths[i],1,0);
    palette();
    for(;;) {
        jkGui_sub_412E20(&controls,100,104,104);
        jkGuiSetup_sub_412EF0(&controls,0);
        /* Bitmap fonts and the stock Setup background are shared with the other tabs. */
        elements[8].wstr=editing>=0?L"Choose button (swaps assignments)":L"Action";
        clampSelection();
        jkGuiRend_MenuSetReturnKeyShortcutElement(&controls,NULL);
        jkGuiRend_MenuSetEscapeKeyShortcutElement(&controls,NULL);
        jkGuiRend_XboxSetInitialFocus(&controls,&elements[ROW_FIRST+selected-top]);
        footer();
        stdDisplay_XboxSetPostMenuDrawCallback(drawGlyphs,NULL);
        result=jkGuiRend_DisplayAndReturnClicked(&controls);
        stdDisplay_XboxSetPostMenuDrawCallback(NULL,NULL);
        if(result==302) continue; /* Marker-scoped fixture changed the current view. */
        if(result==EDIT_ACTION) {edit();continue;}
        if(result==DEFAULT_ACTION) {defaults();continue;}
        if(result==-1 && editing>=0) {editing=-1;selected=savedSelection;top=savedTop;continue;}
        save();
        stdPlatform_Printf("ControlsMenu: saved result=%d map=%d,%d,%d,%d,%d,%d,%d,%d look=%d,%d deadzone=%d invert=%d vibration=%d\n",
            result,buttonMap[0],buttonMap[1],buttonMap[2],buttonMap[3],buttonMap[4],buttonMap[5],buttonMap[6],buttonMap[7],options[0],options[1],options[2],options[3],options[4]);
        return result>=100?result:-1;
    }
}

static void smokeTick(jkGuiMenu *menu)
{
    unsigned int now=stdPlatform_GetTimeMsec();
    int phase;
    if(!smokeEnabled) return;
    if(!smokeStart) smokeStart=now;
    phase=(now-smokeStart)/10000;
    if(phase==smokePhase) return;
    smokePhase=phase;
    if(phase==1 || phase==2) {
        int i;
        for(i=0;i<11;i++) jkGuiRend_FocusElementDir(menu,FOCUS_DOWN);
    }
    else if(phase==3) {selected=0;top=0;edit();menu->lastClicked=302;}
    else if(phase==4) {selected=0;edit();menu->lastClicked=302;}
    else if(phase==5) {defaults();menu->lastClicked=302;}
    else if(phase==6) {menu->lastClicked=100;}
    stdPlatform_Printf("ControlsMenuProbe: phase=%d selected=%d top=%d editing=%d fire=%d jump=%d\n",phase,selected,top,editing,buttonMap[7],buttonMap[0]);
    if(!menu->lastClicked) jkGuiRend_Paint(menu);
}

void jkGuiXboxControls_ProbeMenu(jkGuiMenu *menu)
{
    static jkGuiMenu *previous;
    static unsigned int entered;
    jkGuiElement *tab;
    unsigned int now;
    int id;
    if(!smokeEnabled || menu==&controls) return;
    if(!jkGuiRend_MenuGetClickableById(menu,100) || !jkGuiRend_MenuGetClickableById(menu,104)) return;
    now=stdPlatform_GetTimeMsec();
    if(menu!=previous) {previous=menu;entered=now;}
    if(now-entered<10000) return;
    for(id=100;id<=103;id++) {
        tab=jkGuiRend_MenuGetClickableById(menu,id);
        if(tab && tab->type==ELEMENT_TEXT) {
            stdPlatform_Printf("ControlsMenuProbe: active tab=%d boxed=%d\n",id,tab->drawFuncOverride==jkGuiRend_XboxDrawActiveTab);
            menu->lastClicked=id==103?-1:id+1;
            return;
        }
    }
}

void jkGuiXboxControls_Startup(void) {jkGui_InitMenu(&controls,jkGui_stdBitmaps[JKGUI_BM_BK_SETUP]);}
void jkGuiXboxControls_Shutdown(void)
{
    int i;
    for(i=0;i<18;i++) if(glyphs[i]) {stdBitmap_Free(glyphs[i]);glyphs[i]=NULL;}
}
#endif
