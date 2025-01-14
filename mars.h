/*
  Victor Luchits

  The MIT License (MIT)

  Copyright (c) 2021 Victor Luchits

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#ifndef _MARS_H
#define _MARS_H

#ifdef MARS

#include "marshw.h"

enum
{
    MARS_SECCMD_NONE,

    MARS_SECCMD_CLEAR_CACHE,
    MARS_SECCMD_BREAK,

    MARS_SECCMD_R_SETUP,
    MARS_SECCMD_R_WALL_PREP_NODRAW,
    MARS_SECCMD_R_WALL_PREP,
    MARS_SECCMD_R_DRAW_PLANES,
    MARS_SECCMD_R_DRAW_SPRITES,
    MARS_SECCMD_R_DRAW_PSPRITES,

    MARS_SECCMD_M_ANIMATE_FIRE,

    MARS_SECCMD_S_INIT_DMA,
    MARS_SECCMD_S_STOP_MIXER,
    MARS_SECCMD_S_START_MIXER,

    MARS_SECCMD_AM_DRAW,

    MARS_SECCMD_NUMCMDS
};

void Mars_Secondary(void) ATTR_DATA_CACHE_ALIGN;

void Mars_Sec_R_Setup(void) ATTR_DATA_CACHE_ALIGN;
void Mars_Sec_R_WallPrep(void) ATTR_DATA_CACHE_ALIGN;
void Mars_Sec_R_PlanePrep(void) ATTR_DATA_CACHE_ALIGN;
void Mars_Sec_R_SegCommands(void) ATTR_DATA_CACHE_ALIGN;
void Mars_Sec_R_DrawPlanes(void) ATTR_DATA_CACHE_ALIGN;
void Mars_Sec_R_DrawSprites(int sprscreenhalf) ATTR_DATA_CACHE_ALIGN;
void Mars_Sec_R_DrawPSprites(int sprscreenhalf) ATTR_DATA_CACHE_ALIGN;

void Mars_Sec_M_AnimateFire(void) ATTR_OPTIMIZE_EXTREME;
void Mars_Sec_InitSoundDMA(void);
void Mars_Sec_StopSoundMixer(void);
void Mars_Sec_StartSoundMixer(void);
void Mars_Sec_ReadSoundCmds(void) ATTR_DATA_OPTIMIZE_NONE;

void Mars_Sec_AM_Drawer(void);

void sec_dma1_handler(void) ATTR_DATA_OPTIMIZE_NONE;
void pri_cmd_handler(void) ATTR_DATA_OPTIMIZE_NONE;

static inline void Mars_R_SecWait(void)
{
    while(MARS_SYS_COMM4 != MARS_SECCMD_NONE);
}

static inline void Mars_CommSlaveClearCache(void)
{
    Mars_R_SecWait();
    MARS_SYS_COMM4 = MARS_SECCMD_CLEAR_CACHE;
}

static inline void Mars_R_SecSetup(void)
{
    Mars_R_SecWait();
    MARS_SYS_COMM4 = MARS_SECCMD_R_SETUP;
}

// r_phase2
static inline void Mars_R_BeginWallPrep(boolean draw)
{
    Mars_R_SecWait();
    MARS_SYS_COMM6 = 0; // next seg
    MARS_SYS_COMM4 = draw ? MARS_SECCMD_R_WALL_PREP : MARS_SECCMD_R_WALL_PREP_NODRAW;
}

static inline void Mars_R_WallNext(void)
{
    MARS_SYS_COMM6++;
}

static inline void Mars_R_EndWallPrep(void)
{
    MARS_SYS_COMM6 = 0xffff;
}

// r_phase7
static inline void Mars_R_BeginDrawPlanes(void)
{
    MARS_SYS_COMM6 = 0; // next visplane
    MARS_SYS_COMM4 = MARS_SECCMD_R_DRAW_PLANES;
}

static inline void Mars_R_EndDrawPlanes(void)
{
}

// r_phase8
static inline void Mars_R_BeginDrawSprites(int sprscreenhalf)
{
    Mars_R_SecWait();
    MARS_SYS_COMM6 = sprscreenhalf;
    MARS_SYS_COMM4 = MARS_SECCMD_R_DRAW_SPRITES;
}

static inline void Mars_R_EndDrawSprites(void)
{
}

static inline void Mars_R_BeginDrawPSprites(int sprscreenhalf)
{
    Mars_R_SecWait();
    MARS_SYS_COMM6 = sprscreenhalf;
    MARS_SYS_COMM4 = MARS_SECCMD_R_DRAW_PSPRITES;
}

static inline void Mars_R_EndDrawPSprites(void)
{
}

static inline void Mars_M_BeginDrawFire(void)
{
    Mars_R_SecWait();
    MARS_SYS_COMM4 = MARS_SECCMD_M_ANIMATE_FIRE;
}

static inline void Mars_M_EndDrawFire(void)
{
    MARS_SYS_COMM4 = MARS_SECCMD_BREAK;
    Mars_R_SecWait();
}

static inline void Mars_InitSoundDMA(void)
{
    Mars_R_SecWait();
    MARS_SYS_COMM4 = MARS_SECCMD_S_INIT_DMA;
    Mars_R_SecWait();
}

static inline void Mars_StopSoundMixer(void)
{
    Mars_R_SecWait();
    MARS_SYS_COMM4 = MARS_SECCMD_S_STOP_MIXER;
    Mars_R_SecWait();
}

static inline void Mars_StartSoundMixer(void)
{
    Mars_R_SecWait();
    MARS_SYS_COMM4 = MARS_SECCMD_S_START_MIXER;
    Mars_R_SecWait();
}

static inline void Mars_AM_BeginDrawer(void)
{
    Mars_R_SecWait();
    MARS_SYS_COMM4 = MARS_SECCMD_AM_DRAW;
}

static inline void Mars_AM_EndDrawer(void)
{
    Mars_R_SecWait();
}

#endif

#endif // _MARS_H
