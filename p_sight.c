/*
  CALICO

  Line-of-sight checking

  The MIT License (MIT)

  Copyright (c) 2015 James Haley, Olde Skuul, id Software and ZeniMax Media

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

#include "doomdef.h"
#include "p_local.h"
#include "mars.h"

typedef struct
{
    fixed_t sightzstart;           // eye z of looker
    fixed_t topslope, bottomslope; // slopes to top and bottom of target

    divline_t strace;  // from t1 to t2
    fixed_t t2x, t2y;
} sightWork_t;

static int P_DivlineSide(fixed_t x, fixed_t y, divline_t* node) ATTR_DATA_CACHE_ALIGN;
static fixed_t P_InterceptVector2(divline_t* v2, divline_t* v1) ATTR_DATA_CACHE_ALIGN;
static boolean PS_CrossSubsector(sightWork_t* sw, int num) ATTR_DATA_CACHE_ALIGN;
static boolean PS_CrossBSPNode(sightWork_t* sw, int bspnum) ATTR_DATA_CACHE_ALIGN;
static boolean PS_CheckSight(mobj_t* t1, mobj_t* t2) ATTR_DATA_CACHE_ALIGN;
void P_CheckSights2(void) ATTR_DATA_CACHE_ALIGN;

//
// Returns side 0 (front), 1 (back), or 2 (on).
//
static int P_DivlineSide(fixed_t x, fixed_t y, divline_t *node)
{
    fixed_t dx;
    fixed_t dy;
    fixed_t left;
    fixed_t right;

    dx = x - node->x;
    dy = y - node->y;

#ifdef MARS
    left = ((int64_t)node->dy * dx) >> 32;
    right = ((int64_t)dy * node->dx) >> 32;
#else
    left  = (node->dy >> FRACBITS) * (dx >> FRACBITS);
    right = (dy >> FRACBITS) * (node->dx >> FRACBITS);
#endif

    return (left <= right) + (left == right);
}

//
// Returns the fractional intercept point
// along the first divline.
// This is only called by the addthings and addlines traversers.
//
static fixed_t P_InterceptVector2(divline_t *v2, divline_t *v1)
{
    fixed_t frac;
#ifdef MARS
#if 1
    union
    {
        int64_t i64;
        uint32_t i32[2];
    } den, num;

    den.i64 = (int64_t)v1->dy * v2->dx;
    den.i64 -= (int64_t)v1->dx * v2->dy;

    if(den.i32[0] == 0)
        return 0;

    num.i64 = (int64_t)(v1->x - v2->x) * v1->dy;
    num.i64 -= (int64_t)(v1->y - v2->y) * v1->dx;
    num.i64 >>= 16;

    do
    {
        int32_t t;
        __asm volatile(
            "mov #-128, %0\n\t"
            "add %0, %0 /* %0 is now 0xFFFFFF00 */ \n\t"
            "mov.l %4, @(0,%0) /* set 32-bit divisor */ \n\t"
            "mov.l %2, @(16,%0)\n\t"
            "mov.l %3, @(20,%0) /* start divide */\n\t"
            "mov.l @(20,%0), %1 /* get 32-bit quotient */ \n\t"
            : "=&r"(t), "=r"(frac)
            : "r"(num.i32[0]), "r"(num.i32[1]), "r"(den.i32[0])
        );
    }
    while(0);

#else
    fixed_t num;
    fixed_t den, temp;

    FixedMul2(temp, v1->dy, FRACUNIT / 256);
    FixedMul2(den, temp, v2->dx);

    FixedMul2(temp, v1->dx, FRACUNIT / 256);
    FixedMul2(temp, temp, v2->dy);

    den = den - temp;

    if(den == 0)
        return 0;

    FixedMul2(temp, (v2->y - v1->y), FRACUNIT / 256);
    FixedMul2(temp, temp, v1->dx);

    FixedMul2(num, (v1->x - v2->x), FRACUNIT / 256);
    FixedMul2(num, num, v1->dy);

    num  = num + temp;
    frac = FixedDiv(num, den);
#endif
#else
    fixed_t num;
    fixed_t den, temp;

    FixedMul2(den, v1->dy >> 8, v2->dx);
    FixedMul2(temp, v1->dx >> 8, v2->dy);

    den = den - temp;

    if(den == 0)
        return 0;

    FixedMul2(temp, (v2->y - v1->y) >> 8, v1->dx);
    FixedMul2(num, (v1->x - v2->x) >> 8, v1->dy);

    num  = num + temp;
    frac = FixedDiv(num, den);
#endif

    return frac;
}

/*
=================
=
= PS_CrossSubsector
=
= Returns true if strace crosses the given subsector successfuly
=================
*/

static boolean PS_CrossSubsector(sightWork_t *sw, int num)
{
    seg_t       *seg;
    line_t      *line;
    int          s1;
    int          s2;
    int          count;
    subsector_t *sub;
    sector_t    *front;
    sector_t    *back;
    fixed_t      opentop;
    fixed_t      openbottom;
    divline_t    divl;
    vertex_t    *v1;
    vertex_t    *v2;
    fixed_t      frac;
    fixed_t      slope;
    int          side;
    divline_t    *strace = &sw->strace;
    fixed_t      t2x = sw->t2x, t2y = sw->t2y;
    fixed_t      sightzstart = sw->sightzstart;

    sub = &subsectors[num];

    // check lines
    count = sub->numlines;
    seg   = &segs[sub->firstline];

    for(; count; seg++, count--)
    {
        line = &lines[seg->linedef];

        // allready checked other side?
        if(line->validcount == validcount)
            continue;

        line->validcount = validcount;

        v1 = line->v1;
        v2 = line->v2;
        s1 = P_DivlineSide(v1->x, v1->y, strace);
        s2 = P_DivlineSide(v2->x, v2->y, strace);

        // line isn't crossed?
        if(s1 == s2)
            continue;

        divl.x = v1->x;
        divl.y = v1->y;
        divl.dx = v2->x - v1->x;
        divl.dy = v2->y - v1->y;
        s1 = P_DivlineSide(strace->x, strace->y, &divl);
        s2 = P_DivlineSide(t2x, t2y, &divl);

        // line isn't crossed?
        if(s1 == s2)
            continue;

        // stop because it is not two sided anyway
        if(!(line->flags & ML_TWOSIDED))
            return false;

        // crosses a two sided line
        side = seg->side;
        front = &sectors[sides[line->sidenum[side]].sector];
        back = &sectors[sides[line->sidenum[side ^ 1]].sector];

        // no wall to block sight with?
        if(front->floorheight == back->floorheight && front->ceilingheight == back->ceilingheight)
            continue;

        // possible occluder
        // because of ceiling height differences
        if(front->ceilingheight < back->ceilingheight)
            opentop = front->ceilingheight;
        else
            opentop = back->ceilingheight;

        // because of ceiling height differences
        if(front->floorheight > back->floorheight)
            openbottom = front->floorheight;
        else
            openbottom = back->floorheight;

        // quick test for totally closed doors
        if(openbottom >= opentop)
            return false; // stop

        frac = P_InterceptVector2(strace, &divl);

        if(front->floorheight != back->floorheight)
        {
            slope = FixedDiv(openbottom - sightzstart, frac);

            if(slope > sw->bottomslope)
                sw->bottomslope = slope;
        }

        if(front->ceilingheight != back->ceilingheight)
        {
            slope = FixedDiv(opentop - sightzstart, frac);

            if(slope < sw->topslope)
                sw->topslope = slope;
        }

        if(sw->topslope <= sw->bottomslope)
            return false;    // stop
    }

    // passed the subsector ok
    return true;
}

//
// Returns true if strace crosses the given node successfuly
//
static boolean PS_CrossBSPNode(sightWork_t* sw, int bspnum)
{
    node_t *bsp;
    int side;
    divline_t* strace = &sw->strace;

#ifdef MARS

    while((int16_t)bspnum >= 0)
#else
    while(!(bspnum & NF_SUBSECTOR))
#endif
    {
        bsp = &nodes[bspnum];

        // decide which side the start point is on
        side = P_DivlineSide(strace->x, strace->y, (divline_t*)bsp) & 1;

        // the partition plane is crossed here
        if(side == P_DivlineSide(sw->t2x, sw->t2y, (divline_t*)bsp))
            bspnum = bsp->children[side]; // the line doesn't touch the other side
        else if(!PS_CrossBSPNode(sw, bsp->children[side]))
            return false; // cross the starting side
        else
            bspnum = bsp->children[side ^ 1]; // cross the ending side
    }

    return PS_CrossSubsector(sw, bspnum == -1 ? 0 : bspnum & ~NF_SUBSECTOR);
}

//
// Returns true if a straight line between t1 and t2 is unobstructed
//
static boolean PS_CheckSight(mobj_t *t1, mobj_t *t2)
{
    int s1, s2;
    unsigned pnum, bytenum, bitnum;
    sightWork_t sw;

    // First check for trivial rejection
    s1 = (int)(t1->subsector->sector - sectors);
    s2 = (int)(t2->subsector->sector - sectors);
    pnum = s1 * numsectors + s2;
    bytenum = pnum >> 3;

    bitnum = 1;

    switch(pnum & 7)
    {
    case 7:
        do
        {
            bitnum <<= 1;

        case 6:
            bitnum <<= 1;

        case 5:
            bitnum <<= 1;

        case 4:
            bitnum <<= 1;

        case 3:
            bitnum <<= 1;

        case 2:
            bitnum <<= 1;

        case 1:
            bitnum <<= 1;

        case 0:
            break;
        }
        while(0);
    }

    if(rejectmatrix[bytenum] & bitnum)
    {
        return false; // can't possibly be connected
    }

    // look from eyes of t1 to any part of t2
    ++validcount;

    sw.sightzstart = t1->z + t1->height - (t1->height >> 2);
    sw.topslope    = (t2->z + t2->height) - sw.sightzstart;
    sw.bottomslope = (t2->z) - sw.sightzstart;

    // make sure it never lies exactly on a vertex coordinate
    sw.strace.x = (t1->x & ~0x1ffff) | 0x10000;
    sw.strace.y = (t1->y & ~0x1ffff) | 0x10000;
    sw.t2x = (t2->x & ~0x1ffff) | 0x10000;
    sw.t2y = (t2->y & ~0x1ffff) | 0x10000;
    sw.strace.dx = sw.t2x - sw.strace.x;
    sw.strace.dy = sw.t2y - sw.strace.y;

    return PS_CrossBSPNode(&sw, numnodes - 1);
}

//
// Optimal mobj sight checking that checks sights in the main tick loop rather
// than from multiple mobj action routines.
//
void P_CheckSights2(void)
{
    mobj_t *mobj;

    for(mobj = mobjhead.next; mobj != (void *)&mobjhead; mobj = mobj->next)
    {
        // must be killable
        if(!(mobj->flags & MF_COUNTKILL))
            continue;

        // must be about to change states
        if(mobj->tics != 1)
            continue;

        mobj->flags &= ~MF_SEETARGET;

        // must have a target
        if(!mobj->target)
            continue;

        if(PS_CheckSight(mobj, mobj->target))
            mobj->flags |= MF_SEETARGET;
    }
}

// EOF

