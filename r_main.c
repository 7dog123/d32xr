/* r_main.c */

#include "doomdef.h"
#include "r_local.h"
#ifdef MARS
#include "mars.h"
#include "marshw.h"
#endif

int16_t viewportWidth, viewportHeight;
int16_t centerX, centerY;
fixed_t centerXFrac, centerYFrac;
fixed_t stretch;
fixed_t stretchX;
VINT weaponYpos;
fixed_t weaponXScale;

detailmode_t detailmode = detmode_high;
VINT anamorphicview = 0;
VINT initmathtables = 2;

drawcol_t drawcol;
drawcol_t drawfuzzcol;
drawcol_t drawcolnpo2;
drawcol_t drawcollow;
drawspan_t drawspan;

short fuzzoffset[FUZZTABLE] =
{
    1, -1, 1, -1, 1, 1, -1,
    1, 1, -1, 1, 1, 1, -1,
    1, 1, 1, -1, -1, -1, -1,
    1, -1, -1, 1, 1, 1, 1, -1,
    1, -1, 1, 1, -1, -1, 1,
    1, -1, -1, -1, -1, 1, 1,
    1, 1, -1, 1, 1, -1, 1,
    -1, -1, 1, -1, 1, 1, 1,
    1, -1, 1, -1, -1, 1, 1
};

/*===================================== */

/* */
/* walls */
/* */
viswall_t	*viswalls/*[MAXWALLCMDS]*/, *lastwallcmd;

/* */
/* planes */
/* */
visplane_t	*visplanes/*[MAXVISPLANES]*/, *lastvisplane;
#ifdef MARS
uint16_t 		*sortedvisplanes;
#endif

#define NUM_VISPLANES_BUCKETS 32
static visplane_t **visplanes_hash;

/* */
/* sprites */
/* */
vissprite_t	*vissprites/*[MAXVISSPRITES]*/, * lastsprite_p, * vissprite_p;

/* */
/* openings / misc refresh memory */
/* */
unsigned short	*openings/*[MAXOPENINGS]*/, * lastopening;

unsigned short	*segclip, *lastsegclip;

/*===================================== */

#ifndef MARS
boolean		phase1completed;

pixel_t		*workingscreen;
#endif

#ifdef MARS
static int16_t	curpalette = -1;

__attribute__((aligned(16)))
pixel_t* viewportbuffer;

__attribute__((aligned(16)))
#endif
viewdef_t       vd;
player_t	*viewplayer;

VINT			validcount = 1;		/* increment every time a check is made */
VINT			framecount;		/* incremented every frame */

VINT		extralight;			/* bumped light from gun blasts */

/* */
/* precalculated math */
/* */
angle_t		clipangle, doubleclipangle;
#ifndef MARS
fixed_t	*finecosine_ = &finesine_[FINEANGLES / 4];
#endif

fixed_t *yslope/*[SCREENHEIGHT]*/;
fixed_t *distscale/*[SCREENWIDTH]*/;

VINT *viewangletox/*[FINEANGLES/2]*/;

angle_t xtoviewangle[SCREENWIDTH + 1];

/* */
/* performance counters */
/* */
VINT t_ref_cnt = 0;
int t_ref_bsp[4], t_ref_prep[4], t_ref_segs[4], t_ref_planes[4], t_ref_sprites[4], t_ref_total[4];

r_texcache_t r_texcache;

/*
===============================================================================
=
= R_PointToAngle
=
===============================================================================
*/

static int SlopeDiv(unsigned int num, unsigned int den) ATTR_DATA_CACHE_ALIGN;

static int SlopeDiv(unsigned num, unsigned den)
{
    unsigned ans;
    den >>= 8;

    if(den < 2)
        return SLOPERANGE;

    ans = (num << 3) / den;
    return ans <= SLOPERANGE ? ans : SLOPERANGE;
}

angle_t R_PointToAngle2(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2)
{
    int		x;
    int		y;
    int 	base = 0;
    int 	num = 0, den = 0, n = 1;

    x = x2 - x1;
    y = y2 - y1;

    if((!x) && (!y))
        return 0;

    if(x >= 0)
    {
        /* x >=0 */
        if(y >= 0)
        {
            /* y>= 0 */
            if(x > y)
            {
                /* octant 0 */
                num = y, den = x;
            }
            else
            {
                /* octant 1 */
                base = ANG90 - 1, n = -1,	num = x, den = y;
            }
        }
        else
        {
            /* y<0 */
            y = -y;

            if(x > y)
            {
                n = -1, num = y, den = x; /* octant 8 */
            }
            else
            {
                base = ANG270, n = 1, num = x, den = y; /* octant 7 */
            }
        }
    }
    else
    {
        /* x<0 */
        x = -x;

        if(y >= 0)
        {
            /* y>= 0 */
            if(x > y)
            {
                base = ANG180 - 1, n = -1, num = y, den = x; /* octant 3 */
            }
            else
            {
                base = ANG90, num = x, den = y; /* octant 2 */
            }
        }
        else
        {
            /* y<0 */
            y = -y;

            if(x > y)
            {
                base = ANG180, num = y, den = x; /* octant 4 */
            }
            else
            {
                base = ANG270 - 1, n = -1, num = x, den = y; /* octant 5 */
            }
        }
    }

    return base + n * tantoangle[SlopeDiv(num, den)];
}

/*
==============
=
= R_PointInSubsector
=
==============
*/

struct subsector_s *R_PointInSubsector(fixed_t x, fixed_t y)
{
    node_t	*node;
    int		side, nodenum;

    if(!numnodes)				/* single subsector is a special case */
        return subsectors;

    nodenum = numnodes - 1;

#ifdef MARS

    while((int16_t)nodenum >= 0)
#else
    while(!(nodenum & NF_SUBSECTOR))
#endif
    {
        node = &nodes[nodenum];
        side = R_PointOnSide(x, y, node);
        nodenum = node->children[side];
    }

    return &subsectors[nodenum & ~NF_SUBSECTOR];

}

/*============================================================================= */

const VINT viewports[][2][3] =
{
    { { 128, 144, true  }, {  80, 100, true  } },
    { { 128, 160, true  }, {  80, 128, true  } },
    { { 160, 180, true  }, {  80, 144, true  } },
    { { 224, 128, false }, { 160, 100, false } },
    { { 256, 144, false }, { 160, 128, false } },
    { { 320, 180, false }, { 160, 144, false } },
};

VINT viewportNum;
boolean lowResMode;
const VINT numViewports = sizeof(viewports) / sizeof(viewports[0]);

/*
================
=
= R_SetViewportSize
=
================
*/
void R_SetViewportSize(int num)
{
    int width, height;

    while(!I_RefreshCompleted())
        ;

    num %= numViewports;

    width = viewports[num][splitscreen][0];
    height = viewports[num][splitscreen][1];
    lowResMode = viewports[num][splitscreen][2];

    viewportNum = num;
    viewportWidth = width;
    viewportHeight = height;

    centerX = viewportWidth / 2;
    centerY = viewportHeight / 2;

    centerXFrac = centerX * FRACUNIT;
    centerYFrac = centerY * FRACUNIT;

    if(anamorphicview)
    {
        stretch = ((FRACUNIT * 16 * height) / 180 * 28) / width;
        weaponXScale = 1000 * (lowResMode ? 1 : 2) * FRACUNIT / 1100;
    }
    else
    {
        /* proper screen size would be 160*100, stretched to 224 is 2.2 scale */
        //stretch = (fixed_t)((160.0f / width) * ((float)height / 180.0f) * 2.2f * FRACUNIT);
        stretch = ((FRACUNIT * 16 * height) / 180 * 22) / width;
        weaponXScale = FRACUNIT * (lowResMode ? 1 : 2);
    }

    stretchX = stretch * centerX;

    weaponYpos = 180;

    if(viewportHeight <= 128)
    {
        weaponYpos = 144;
    }

    weaponYpos = (viewportHeight - weaponYpos) / 2;

    initmathtables = 2;

    // refresh func pointers
    R_SetDetailMode(detailmode);

    R_InitColormap(lowResMode);

    clearscreen = 2;

#ifdef MARS
    Mars_CommSlaveClearCache();
#endif
}

void R_SetDetailMode(int mode)
{
    if(mode < detmode_potato)
        return;

    if(mode >= MAXDETAILMODES)
        return;

    detailmode = mode;

    if(debugmode == DEBUGMODE_NODRAW)
    {
        drawcol = I_DrawColumnNoDraw;
        drawcolnpo2 = I_DrawColumnNoDraw;
        drawfuzzcol = I_DrawColumnNoDraw;
        drawcollow = I_DrawColumnNoDraw;
        drawspan = I_DrawSpanNoDraw;
        return;
    }

    if(lowResMode)
    {
        drawcol = I_DrawColumnLow;
        drawcolnpo2 = I_DrawColumnNPo2Low;
        drawfuzzcol = I_DrawFuzzColumnLow;
        drawcollow = I_DrawColumnLow;
        drawspan = detailmode == detmode_potato ? I_DrawSpanPotatoLow : I_DrawSpanLow;
    }
    else
    {
        drawcol = I_DrawColumn;
        drawcolnpo2 = I_DrawColumnNPo2;
        drawfuzzcol = I_DrawFuzzColumn;
        drawspan = I_DrawSpan;
        drawcollow = I_DrawColumnLow;
        drawspan = detailmode == detmode_potato ? I_DrawSpanPotato : I_DrawSpan;
    }

#ifdef MARS
    Mars_CommSlaveClearCache();
#endif
}

int R_DefaultViewportSize(void)
{
    int i;

    for(i = 0; i < numViewports; i++)
    {
        const VINT* vp = viewports[i][0];

        if(vp[0] == 160 && vp[2] == true)
            return i;
    }

    return 0;
}

/*
==============
=
= R_Init
=
==============
*/

void R_Init(void)
{
    D_printf("R_InitData\n");
    R_InitData();
    D_printf("Done\n");

    R_SetViewportSize(viewportNum);

    framecount = 0;
    viewplayer = &players[0];

    R_SetDetailMode(detailmode);

    R_InitTexCache(&r_texcache, numflats + numtextures);
}

/*
==============
=
= R_SetupTextureCaches
=
==============
*/
void R_SetupTextureCaches(void)
{
    int i;
    int zonefree;
    int cachezonesize;
    void *margin;
    const int zonemargin = 12 * 1024;
    const int flatblocksize = sizeof(memblock_t) + ((sizeof(texcacheblock_t) + 15) & ~15) + 64 * 64 + 32;

    // reset pointers from previous level
    for(i = 0; i < numtextures; i++)
        textures[i].data = R_CheckPixels(textures[i].lumpnum);

    for(i = 0 ; i < numflats ; i++)
        flatpixels[i] = R_CheckPixels(firstflat + i);

    // functioning texture cache requires at least 8kb of ram
    zonefree = Z_LargestFreeBlock(mainzone);

    if(zonefree < zonemargin + flatblocksize)
        goto nocache;

    cachezonesize = zonefree - zonemargin - 128; // give the main zone some slack

    if(cachezonesize < flatblocksize)
        goto nocache;

    margin = Z_Malloc(zonemargin, PU_LEVEL, 0);

    R_InitTexCacheZone(&r_texcache, cachezonesize);

    Z_Free(margin);
    return;

nocache:
    R_InitTexCacheZone(&r_texcache, 0);
}

void R_SetupLevel(void)
{
    /* we used the framebuffer as temporary memory, so it */
    /* needs to be cleared from potential garbage */
    I_ClearFrameBuffer();

    R_SetupTextureCaches();

    R_SetViewportSize(viewportNum);

#ifdef MARS
    curpalette = -1;
#endif
}

/*============================================================================= */

#ifndef MARS
int shadepixel;
extern	int	workpage;
extern	pixel_t	*screens[2];	/* [viewportWidth*viewportHeight];  */
#endif

/*
==================
=
= R_Setup
=
==================
*/

static void R_Setup(int displayplayer, visplane_t *visplanes_, vissprite_t *vissprites_,
                    visplane_t **visplanes_hash_, uint16_t *sortedvisplanes_)
{
    int 		i;
    int		damagecount, bonuscount;
    player_t *player;
#ifdef JAGUAR
    int		shadex, shadey, shadei;
#endif
    unsigned short  *tempbuf;
#ifdef MARS
    int		palette = 0;
#endif

    /* */
    /* set up globals for new frame */
    /* */
#ifndef MARS
    workingscreen = screens[workpage];

    *(pixel_t  **)0xf02224 = workingscreen;	/* a2 base pointer */
    *(int *)0xf02234 = 0x10000;				/* a2 outer loop add (+1 y) */
    *(int *)0xf0226c = *(int *)0xf02268 = 0;		/* pattern compare */
#else

    if(debugmode == DEBUGMODE_NODRAW)
        I_ClearFrameBuffer();

#endif

    framecount++;
    validcount++;

    player = &players[displayplayer];

    vd.viewplayer = player;
    vd.viewx = player->mo->x;
    vd.viewy = player->mo->y;
    vd.viewz = player->viewz;
    vd.viewangle = player->mo->angle;

    vd.viewsin = finesine(vd.viewangle >> ANGLETOFINESHIFT);
    vd.viewcos = finecosine(vd.viewangle >> ANGLETOFINESHIFT);

    vd.displayplayer = displayplayer;
    vd.lightlevel = player->mo->subsector->sector->lightlevel;
    vd.fixedcolormap = 0;

    damagecount = player->damagecount;
    bonuscount = player->bonuscount;

    clipangle = xtoviewangle[0];
    doubleclipangle = clipangle * 2;

#ifdef JAGUAR
    vd.extralight = player->extralight << 6;

    /* */
    /* calc shadepixel */
    /* */
    if(damagecount)
        damagecount += 10;

    if(bonuscount)
        bonuscount += 2;

    damagecount >>= 2;
    shadex = (bonuscount >> 1) + damagecount;
    shadey = (bonuscount >> 1) - damagecount;
    shadei = (bonuscount + damagecount) << 2;

    shadei += player->extralight << 3;

    /* */
    /* pwerups */
    /* */
    if(player->powers[pw_invulnerability] > 60
            || (player->powers[pw_invulnerability] & 4))
    {
        shadex -= 8;
        shadei += 32;
    }

    if(player->powers[pw_ironfeet] > 60
            || (player->powers[pw_ironfeet] & 4))
        shadey += 7;

    if(player->powers[pw_strength]
            && (player->powers[pw_strength] < 64))
        shadex += (8 - (player->powers[pw_strength] >> 3));


    /* */
    /* bound and store shades */
    /* */
    if(shadex > 7)
        shadex = 7;
    else if(shadex < -8)
        shadex = -8;

    if(shadey > 7)
        shadey = 7;
    else if(shadey < -8)
        shadey = -8;

    if(shadei > 127)
        shadei = 127;
    else if(shadei < -128)
        shadei = -128;

    shadepixel = ((shadex << 12) & 0xf000) + ((shadey << 8) & 0xf00) + (shadei & 0xff);
#endif

#ifdef MARS

    if(detailmode == detmode_high)
        vd.extralight = player->extralight << 4;
    else
        vd.extralight = 0;

    if(player->powers[pw_invulnerability] > 60
            || (player->powers[pw_invulnerability] & 4))
        vd.fixedcolormap = INVERSECOLORMAP;

    viewportbuffer = (pixel_t*)I_ViewportBuffer();

    palette = 0;

    i = 0;

    if(player->powers[pw_strength] > 0)
        i = 12 - player->powers[pw_strength] / 64;

    if(i < damagecount)
        i = damagecount;

    if(gamepaused)
        palette = 14;
    else if(!splitscreen)
    {
        if(i)
        {
            palette = (i + 7) / 8;

            if(palette > 7)
                palette = 7;

            palette += 1;
        }
        else if(bonuscount)
        {
            palette = (bonuscount + 7) / 8;

            if(palette > 3)
                palette = 3;

            palette += 9;
        }
        else if(player->powers[pw_ironfeet] > 60
                || (player->powers[pw_ironfeet] & 4))
            palette = 13;
    }

    if(palette != curpalette)
    {
        curpalette = palette;
        I_SetPalette(dc_playpals + palette * 768);
    }

    if(vd.fixedcolormap == INVERSECOLORMAP)
        vd.fuzzcolormap = INVERSECOLORMAP;
    else
        vd.fuzzcolormap = (colormapopt ? 12 : 6) * 256;

#endif

    tempbuf = (unsigned short *)I_WorkBuffer();

    tempbuf = (unsigned short*)(((intptr_t)tempbuf + 3) & ~3);
    viswalls = (void*)tempbuf;
    tempbuf += sizeof(*viswalls) * MAXWALLCMDS / sizeof(*tempbuf);
    lastwallcmd = viswalls;			/* no walls added yet */

    segclip = tempbuf;
    lastsegclip = segclip;
    tempbuf += MAXOPENINGS;

    tempbuf = (unsigned short*)(((intptr_t)tempbuf + 3) & ~3);
    openings = tempbuf;
    tempbuf += MAXOPENINGS;

    sortedvisplanes = sortedvisplanes_;

    vissprites = vissprites_;

    visplanes = visplanes_;
    visplanes_hash = visplanes_hash_;
    lastvisplane = visplanes + 1;		/* visplanes[0] is left empty */

    /* */
    /* plane filling */
    /*	 */
    tempbuf = (unsigned short *)(((intptr_t)tempbuf + 1) & ~1);
    tempbuf++; // padding

    for(i = 0; i < MAXVISPLANES; i++)
    {
        visplanes[i].open = tempbuf;
        tempbuf += SCREENWIDTH + 2;
    }

    //I_Error("%d", ((uint16_t *)I_FrameBuffer() + 64*1024-0x100 - tempbuf) * 2);

    /* */
    /* clear sprites */
    /* */
    vissprite_p = vissprites;
    lastsprite_p = vissprite_p;

    lastopening = openings;

    for(i = 0; i < NUM_VISPLANES_BUCKETS; i++)
        visplanes_hash[i] = NULL;

#ifndef MARS
    phasetime[0] = samplecount;
#endif

    R_SetupTexCacheFrame(&r_texcache);
}

#ifdef MARS

void Mars_Sec_R_Setup(void)
{
    int i;

    Mars_ClearCacheLines(&vd, (sizeof(vd) + 31) / 16);
    Mars_ClearCacheLine(&viewportbuffer);

    Mars_ClearCacheLine(&viswalls);
    Mars_ClearCacheLine(&vissprites);
    Mars_ClearCacheLine(&visplanes);
    Mars_ClearCacheLine(&lastvisplane);
    Mars_ClearCacheLine(&visplanes_hash);

    Mars_ClearCacheLine(&segclip);
    Mars_ClearCacheLine(&lastsegclip);

    Mars_ClearCacheLine(&openings);
    Mars_ClearCacheLine(&lastopening);

    Mars_ClearCacheLines(visplanes, (sizeof(visplane_t)*MAXVISPLANES + 31) / 16);

    Mars_ClearCacheLine(&sortedvisplanes);

    for(i = 0; i < NUM_VISPLANES_BUCKETS; i++)
        visplanes_hash[i] = NULL;
}

#endif

//
// Check for a matching visplane in the visplanes array, or set up a new one
// if no compatible match can be found.
//
int R_PlaneHash(fixed_t height, unsigned flatnum, unsigned lightlevel)
{
    return ((((unsigned)height >> 8) + lightlevel) ^ flatnum) & (NUM_VISPLANES_BUCKETS - 1);
}

void R_MarkOpenPlane(visplane_t* pl)
{
    int i;
    unsigned short* open = pl->open;

    for(i = 0; i < viewportWidth / 4; i++)
    {
        *open++ = OPENMARK;
        *open++ = OPENMARK;
        *open++ = OPENMARK;
        *open++ = OPENMARK;
    }
}

void R_InitClipBounds(unsigned *clipbounds)
{
    // initialize the clipbounds array
    int i;
    int longs = (viewportWidth + 1) / 2;
    unsigned* clip = clipbounds;
    unsigned clipval = (unsigned)viewportHeight << 16 | viewportHeight;

    for(i = 0; i < longs; i++)
        *clip++ = clipval;
}

visplane_t* R_FindPlane(int hash, fixed_t height,
                        int flatnum, int lightlevel, int start, int stop)
{
    visplane_t *check, *tail, *next;

    tail = visplanes_hash[hash];

    for(check = tail; check; check = next)
    {
        next = check->next ? &visplanes[check->next - 1] : NULL;

        if(height == check->height &&  // same plane as before?
                flatnum == check->flatnum &&
                lightlevel == check->lightlevel)
        {
            if(MARKEDOPEN(check->open[start]))
            {
                // found a plane, so adjust bounds and return it
                if(start < check->minx)
                    check->minx = start; // mark the new edge

                if(stop > check->maxx)
                    check->maxx = stop;  // mark the new edge

                return check; // use the same one as before
            }
        }
    }

    if(lastvisplane == visplanes + MAXVISPLANES)
        return visplanes;

    // make a new plane
    check = lastvisplane;
    ++lastvisplane;

    check->height = height;
    check->flatnum = flatnum;
    check->lightlevel = lightlevel;
    check->minx = start;
    check->maxx = stop;

    R_MarkOpenPlane(check);

    check->next = tail ? tail - visplanes + 1 : 0;
    visplanes_hash[hash] = check;

    return check;
}

void R_BSP(void);
void R_WallPrep(void);
void R_SpritePrep(void);
boolean R_LatePrep(void);
void R_Cache(void);
void R_SegCommands(void);
void R_DrawPlanes(void);
void R_Sprites(void);
void R_Update(void);

/*
==============
=
= R_RenderView
=
==============
*/

extern	boolean	debugscreenactive;

#ifndef MARS
int		phasetime[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};

extern	ref1_start;
extern	ref2_start;
extern	ref3_start;
extern	ref4_start;
extern	ref5_start;
extern	ref6_start;
extern	ref7_start;
extern	ref8_start;

void R_RenderPlayerView(int displayplayer)
{
    visplane_t visplanes_[MAXVISPLANES], *visplanes_hash_[NUM_VISPLANES_BUCKETS];
    vissprite_t vissprites_[MAXVISSPRITES];
    uint32_t sortedvisplanes_[MAXVISPLANES];

    /* make sure its done now */
#if defined(JAGUAR)

    while(!I_RefreshCompleted())
        ;

#endif

    /* */
    /* initial setup */
    /* */
    if(debugscreenactive)
        I_DebugScreen();

    R_Setup(displayplayer, visplanes_, vissprites_, visplanes_hash_, (uint16_t *)sortedvisplanes_);

#ifndef JAGUAR
    R_BSP();

    R_WallPrep();
    R_SpritePrep();

    /* the rest of the refresh can be run in parallel with the next game tic */
    if(R_LatePrep())
        R_Cache();

    R_SegCommands();

    R_DrawPlanes();

    R_Sprites();

    R_Update();
#else

    /* start the gpu running the refresh */
    phasetime[1] = 0;
    phasetime[2] = 0;
    phasetime[3] = 0;
    phasetime[4] = 0;
    phasetime[5] = 0;
    phasetime[6] = 0;
    phasetime[7] = 0;
    phasetime[8] = 0;
    gpufinished = zero;
    gpucodestart = (int)&ref1_start;

#endif
}

#else

void R_RenderPlayerView(int displayplayer)
{
    int t_bsp, t_prep, t_segs, t_planes, t_sprites, t_total;
    boolean drawworld = !(players[consoleplayer].automapflags & AF_ACTIVE);
    __attribute__((aligned(16)))
    visplane_t visplanes_[MAXVISPLANES], *visplanes_hash_[NUM_VISPLANES_BUCKETS];
    __attribute__((aligned(16)))
    vissprite_t vissprites_[MAXVISSPRITES];
    __attribute__((aligned(16)))
    uint32_t sortedvisplanes_[MAXVISPLANES];

    while(!I_RefreshCompleted())
        ;

    t_total = I_GetFRTCounter();

    Mars_R_SecWait();

    R_Setup(displayplayer, visplanes_, vissprites_, visplanes_hash_, (uint16_t *)sortedvisplanes_);

    Mars_R_SecSetup();

    Mars_R_BeginWallPrep(drawworld);

    t_bsp = I_GetFRTCounter();
    R_BSP();
    t_bsp = I_GetFRTCounter() - t_bsp;

    Mars_R_EndWallPrep();

    if(!drawworld)
        return;

    t_prep = I_GetFRTCounter();
    R_SpritePrep();
    R_Cache();
    t_prep = I_GetFRTCounter() - t_prep;

    t_segs = I_GetFRTCounter();
    R_SegCommands();
    t_segs = I_GetFRTCounter() - t_segs;

    Mars_R_SecWait();

    Mars_ClearCacheLine(&lastsegclip);
    Mars_ClearCacheLine(&lastopening);
    Mars_ClearCacheLine(&lastvisplane);

    if(lastsegclip - segclip > MAXOPENINGS)
        I_Error("lastsegclip > MAXOPENINGS: %d", lastsegclip - segclip);

    if(lastopening - openings > MAXOPENINGS)
        I_Error("lastopening > MAXOPENINGS: %d", lastopening - openings);

    Mars_ClearCacheLines(openings, ((lastopening - openings) * sizeof(*openings) + 31) / 16);

    Mars_ClearCacheLines(visplanes, ((lastvisplane - visplanes) * sizeof(visplane_t) + 31) / 16);
    Mars_ClearCacheLines(sortedvisplanes, ((lastvisplane - visplanes - 1) * sizeof(int) + 31) / 16);

    t_planes = I_GetFRTCounter();
    R_DrawPlanes();
    t_planes = I_GetFRTCounter() - t_planes;

    t_sprites = I_GetFRTCounter();
    R_Sprites();
    t_sprites = I_GetFRTCounter() - t_sprites;

    R_Update();

    Mars_R_SecWait();

    t_total = I_GetFRTCounter() - t_total;

    t_ref_cnt = (t_ref_cnt + 1) & 3;
    t_ref_bsp[t_ref_cnt] = t_bsp;
    t_ref_prep[t_ref_cnt] = t_prep;
    t_ref_segs[t_ref_cnt] = t_segs;
    t_ref_planes[t_ref_cnt] = t_planes;
    t_ref_sprites[t_ref_cnt] = t_sprites;
    t_ref_total[t_ref_cnt] = t_total;
}

#endif
