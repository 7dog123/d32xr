/* R_data.c */

#include "doomdef.h"
#include "r_local.h"
#include "p_local.h"
#ifdef MARS
#include "mars.h"
#endif

boolean	spr_rotations;

VINT		firstflat, lastflat, numflats;

VINT		numtextures;
texture_t	*textures;

spritedef_t sprites[NUMSPRITES];
spriteframe_t* spriteframes;

VINT			*flattranslation;		/* for global animation */
VINT			*texturetranslation;	/* for global animation */

void			**flatpixels;

texture_t	*skytexturep;

uint8_t		*dc_playpals;

/*============================================================================ */

/*
==================
=
= R_InitTextures
=
= Initializes the texture list with the textures from the world map
=
==================
*/

int			*maptex;

void R_InitTextures(void)
{
    maptexture_t	*mtexture;
    texture_t		*texture;
    int			i, j, c;
    int			offset;
    int			*directory;


    /* */
    /* load the map texture definitions from textures.lmp */
    /* */
    maptex = (void *)I_TempBuffer();
    W_ReadLump(W_GetNumForName("TEXTURE1"), maptex);
    numtextures = LITTLELONG(*maptex);
    directory = maptex + 1;

    textures = Z_Malloc(numtextures * sizeof(*textures), PU_STATIC, 0);

    for(i = 0; i < numtextures; i++, directory++)
    {
        offset = LITTLELONG(*directory);
        mtexture = (maptexture_t*)((byte*)maptex + offset);
        texture = &textures[i];
        texture->width = LITTLESHORT(mtexture->width);
        texture->height = LITTLESHORT(mtexture->height);
        D_memcpy(texture->name, mtexture->name, 8);

        for(j = 0; j < 8; j++)
        {
            c = texture->name[j];

            if(c >= 'a' && c <= 'z')
                texture->name[j] = c - ('a' - 'A');
        }

        texture->data = NULL;		/* not cached yet */
        texture->lumpnum = W_CheckNumForName(texture->name);

        if(texture->lumpnum == -1)
            texture->lumpnum = 0;

#if 0
        uint8_t* data = R_CheckPixels(texture->lumpnum);

        if(data)
        {
            int j;

            for(j = 0; j < texture->width * texture->height; j++)
            {
                if(!D_strncasecmp(texture->name, "SP_HOT1", 7))
                    continue;

                if(!D_strncasecmp(texture->name, "SW1HOT", 6))
                    continue;

                if(!D_strncasecmp(texture->name, "SW2HOT", 6))
                    continue;

                if(data[j] == 248 || data[j] == 249)
                    I_Error("%d %s", data[j] & 0xff, texture->name);
            }
        }

#endif
    }

    /* */
    /* translation table for global animation */
    /* */
    texturetranslation = Z_Malloc((numtextures + 1) * sizeof(*texturetranslation), PU_STATIC, 0);

    for(i = 0 ; i < numtextures ; i++)
        texturetranslation[i] = i;
}


/*
================
=
= R_InitFlats
=
=================
*/

void R_InitFlats(void)
{
    int		i;

    firstflat = W_GetNumForName("F_START") + 1;
    lastflat = W_GetNumForName("F_END") - 1;
    numflats = lastflat - firstflat + 1;

    /* translation table for global animation */
    flattranslation = Z_Malloc((numflats + 1) * sizeof(*flattranslation), PU_STATIC, 0);

    for(i = 0 ; i < numflats ; i++)
        flattranslation[i] = i;

    flatpixels = Z_Malloc(numflats * sizeof(*flatpixels), PU_STATIC, 0);
}

/*
================
=
= R_InitData
=
= Locates all the lumps that will be used by all views
= Must be called after W_Init
=================
*/

void R_InitData(void)
{
    dc_playpals = (uint8_t*)W_POINTLUMPNUM(W_GetNumForName("PLAYPALS"));

    R_InitTextures();
    R_InitFlats();
    R_InitSpriteDefs((const char **)sprnames);
}


/*============================================================================= */

/*
================
=
= R_FlatNumForName
=
================
*/

#ifdef i386
#define HIBIT (1<<7)
#else
#define HIBIT (1<<31)
#endif

int	R_FlatNumForName(const char *name)
{
#if 0
    int		i;
    static char	namet[16];

    i = W_CheckNumForName(name);

    if(i == -1)
    {
        namet[8] = 0;
        D_memcpy(namet, name, 8);
        I_Error("R_FlatNumForName: %s not found", namet);
    }

    i -= firstflat;

    if(i > numflats)
        I_Error("R_FlatNumForName: %s past f_end", namet);

    return i;
#endif
    int			i;
    lumpinfo_t	*lump_p;
    char	name8[12];
    int		v1, v2;
    int		c;

    /* make the name into two integers for easy compares */
    *(int *)&name8[0] = 0;
    *(int *)&name8[4] = 0;

    for(i = 0 ; i < 8 && name[i] ; i++)
    {
        c = name[i];

        if(c >= 'a' && c <= 'z')
            c -= 'a' - 'A';

        name8[i] = c;
    }

    v1 = *(int *)name8;
    v2 = *(int *)&name8[4];

#ifndef MARS
    v1 |= HIBIT;
#endif

    lump_p = &lumpinfo[firstflat];

    for(i = 0 ; i < numflats; i++, lump_p++)
    {
        if(*(int *)&lump_p->name[4] == v2
                && (*(int *)lump_p->name) == v1)
            return i;
    }

    I_Error("R_FlatNumForName: %s not found", name);
    return -1;
}


/*
================
=
= R_CheckTextureNumForName
=
================
*/

int	R_CheckTextureNumForName(const char *name)
{
    int		i, c;
    char	temp[8];
    int		v1, v2;
    texture_t	*texture_p;

    if(name[0] == '-')		/* no texture marker */
        return 0;

    *(int *)&temp[0] = 0;
    *(int *)&temp[4] = 0;

    for(i = 0 ; i < 8 && name[i] ; i++)
    {
        c = name[i];

        if(c >= 'a' && c <= 'z')
            c -= ('a' - 'A');

        temp[i] = c;
    }

    v1 = *(int *)temp;
    v2 = *(int *)&temp[4];

    texture_p = textures;

    for(i = 0 ; i < numtextures ; i++, texture_p++)
        if(*(int *)&texture_p->name[4] == v2
                && (*(int *)texture_p->name) == v1)
            return i;

    return -1;
}


/*
================
=
= R_TextureNumForName
=
================
*/

int	R_TextureNumForName(const char *name)
{
    int		i;

    i = R_CheckTextureNumForName(name);

    if(i == -1)
        return 0;

    //if (i==-1)
    //	I_Error ("R_TextureNumForName: %s not found",name);

    return i;
}

/*
================
=
= R_InitMathTables
=
= Create all the math tables for the current screen size
================
*/
void R_InitMathTables(void)
{
    int i;
    fixed_t focalLength;
    fixed_t stretchWidth;
    int fuzzunit;
    VINT *tempviewangletox;

    I_FreeWorkBuffer();

    viewangletox = (VINT *)I_AllocWorkBuffer(sizeof(*viewangletox) * (FINEANGLES / 2));
    distscale = (fixed_t *)I_AllocWorkBuffer(sizeof(*distscale) * SCREENWIDTH);
    yslope = (fixed_t *)I_AllocWorkBuffer(sizeof(*yslope) * SCREENHEIGHT);
    tempviewangletox = (VINT *)I_WorkBuffer();

    // Use tangent table to generate viewangletox:
    //  viewangletox will give the next greatest x
    //  after the view angle.
    //
    // Calc focallength
    //  so FIELDOFVIEW angles covers viewportWidth.

    focalLength = FixedDiv(centerXFrac, finetangent(FINEANGLES / 4 + FIELDOFVIEW / 2));

    for(i = 0; i < FINEANGLES / 2; i++)
    {
        fixed_t t;

        if(finetangent(i) > FRACUNIT * 2)
        {
            t = -1;
        }
        else if(finetangent(i) < -FRACUNIT * 2)
        {
            t = viewportWidth + 1;
        }
        else
        {
            t = FixedMul(finetangent(i), focalLength);
            t = (centerXFrac - t + FRACUNIT - 1) >> FRACBITS;

            if(t < -1)
            {
                t = -1;
            }
            else if(t > viewportWidth + 1)
            {
                t = viewportWidth + 1;
            }
        }

        tempviewangletox[i] = t;
    }

    // Scan viewangletox[] to generate xtoviewangle[]:
    //  xtoviewangle will give the smallest view angle
    //  that maps to x.
    for(i = 0; i <= viewportWidth; i++)
    {
        int x;

        for(x = 0; tempviewangletox[x] > i; x++);

        xtoviewangle[i] = (x << (ANGLETOFINESHIFT)) - ANG90;
    }

    // Take out the fencepost cases from viewangletox.
    for(i = 0; i < FINEANGLES / 2; i++)
    {
        if(tempviewangletox[i] == -1)
        {
            tempviewangletox[i] = 0;
        }
        else if(tempviewangletox[i] == viewportWidth + 1)
        {
            tempviewangletox[i] = viewportWidth;
        }

        viewangletox[i] = tempviewangletox[i];
    }

    // Make the yslope table for floor and ceiling textures
    stretchWidth = viewportWidth / 2 * stretch;

    for(i = 0; i < viewportHeight; i++)
    {
        fixed_t y = ((i - viewportHeight / 2) << FRACBITS) + FRACUNIT / 2;
        y = D_abs(y);
        y = FixedDiv(stretchWidth, y);
        yslope[i] = y;
    }

    // Create the distance scale table for floor and ceiling textures
    for(i = 0; i < viewportWidth; i++)
    {
        fixed_t cosang = finecosine(xtoviewangle[i] >> ANGLETOFINESHIFT);
        cosang = D_abs(cosang);
        distscale[i] = FixedDiv(FRACUNIT, cosang);
    }

    fuzzunit = lowResMode ? 160 : 320;

    for(i = 0; i < FUZZTABLE; i++)
    {
        fuzzoffset[i] = fuzzoffset[i] < 0 ? -fuzzunit : fuzzunit;
    }
}


// variables used to look up
//  and range check thing_t sprites patches

typedef struct
{
    short		rotate;		/* if false use 0 for any position */
    short		lump[8];	/* lump to use for view angles 0-7 */
} tempspriteframe_t;

//
// R_InstallSpriteLump
// Local function for R_InitSprites.
//
void
R_InstallSpriteLump(const char* spritename, char letter, tempspriteframe_t* frame,
                    int lump, int rotation, boolean flipped)
{
    int r;

    if(rotation == 0)
    {
        // the lump should be used for all rotations
        if(frame->rotate == 0)
            I_Error("R_InitSprites: Sprite %s frame %c has "
                    "multip rot=0 lump", spritename, letter);

        if(frame->rotate == 1)
            I_Error("R_InitSprites: Sprite %s frame %c has rotations "
                    "and a rot=0 lump", spritename, letter);

        frame->rotate = 0;

        for(r = 0; r < 8; r++)
        {
            frame->lump[r] = flipped ? (lump + 1) * -1 : lump;
        }

        return;
    }

    // the lump is only used for one rotation
    if(frame->rotate == 0)
        I_Error("R_InitSprites: Sprite %s frame %c has rotations "
                "and a rot=0 lump", spritename, letter);

    frame->rotate = 1;

    // make 0 based
    rotation--;

    if(frame->lump[rotation] != -1)
        I_Error("R_InitSprites: Sprite %s : %c : %c "
                "has two lumps mapped to it",
                spritename, letter, '1' + rotation);

    frame->lump[rotation] = flipped ? (lump + 1) * -1 : lump;
}




//
// R_InitSpriteDefs
// Pass a null terminated list of sprite names
//  (4 chars exactly) to be used.
// Builds the sprite rotation matrixes to account
//  for horizontally flipped sprites.
// Will report an error if the lumps are inconsistant.
// Only called at startup.
//
// Sprite lump names are 4 characters for the actor,
//  a letter for the frame, and a number for the rotation.
// A sprite that is flippable will have an additional
//  letter/number appended.
// The rotation character can be 0 to signify no rotations.
//
void R_InitSpriteDefs(const char** namelist)
{
    int		i;
    int		l;
    int		frame;
    int		rotation;
    int		start;
    int		end;
    int		numsprites = NUMSPRITES;
    tempspriteframe_t* sprtemp;
    int		maxframe;
    byte	*tempbuf;
    int		totalframes;
    int 	totallumps;
    VINT 	*lumps;

    spr_rotations = false;

    start = W_CheckNumForName("S_START");

    if(start < 0)
    {
        start = 0;
        end = W_GetNumForName("T_START");
        l = W_CheckNumForName("DS_START");

        if(l != -1 && l < end)
            end = l;
    }
    else
    {
        start += 1;
        end = W_GetNumForName("S_END");
    }

    // scan all the lump names for each of the names,
    //  noting the highest frame letter.
    maxframe = 29;
    totalframes = 0;
    totallumps = 0;

    tempbuf = I_WorkBuffer();
    sprtemp = (void*)tempbuf;

    for(i = 0; i < numsprites; i++)
    {
        const char* spritename = namelist[i];

        D_memset(sprtemp, -1, sizeof(tempspriteframe_t) * 29);
        maxframe = -1;

        // scan the lumps,
        //  filling in the frames for whatever is found
        for(l = start; l < end; l += 2)
        {
            int flip;
            const char* framename;

            framename = W_GetNameForNum(l);

            if(D_strncasecmp(framename, spritename, 4))
                continue;

            framename += 4;

            for(flip = 0; flip < 2; flip++)
            {
                if(!framename[0])
                    break;

                frame = framename[0] - 'A';
                rotation = framename[1] - '0';

                if(frame > maxframe)
                    maxframe = frame;

                if(frame >= 29 || rotation > 8)
                    I_Error("Bad frame characters in lump %i", l);

                R_InstallSpriteLump(spritename, framename[0], sprtemp + frame,
                                    l, rotation, flip);
                framename += 2;
            }
        }

        // check the frames that were found for completeness
        if(maxframe == -1)
        {
            sprites[i].numframes = 0;
            continue;
        }

        maxframe++;

        for(frame = 0; frame < maxframe; frame++)
        {
            switch((int)sprtemp[frame].rotate)
            {
            case -1:
                sprtemp[frame].rotate = 0;

            case 0:
                // only the first rotation is needed
                totallumps += 2;
                break;

            case 1:

                // must have all 8 frames
                for(rotation = 0; rotation < 8; rotation++)
                    if(sprtemp[frame].lump[rotation] == -1)
                        I_Error("Sprite %s frame %c "
                                "is missing rotations",
                                spritename, frame + 'A');

                totallumps += 8;
                spr_rotations = true;
                break;
            }
        }

        // allocate space for the frames present and copy sprtemp to it
        sprites[i].numframes = maxframe;

        sprtemp += maxframe;
        totalframes += maxframe;
    }

    spriteframes = Z_Malloc(totalframes * sizeof(spriteframe_t), PU_STATIC, NULL);
    sprtemp = (void*)tempbuf;
    lumps = Z_Malloc(totallumps * sizeof(*lumps), PU_STATIC, NULL);

    for(i = 0; i < totalframes; i++)
    {
        spriteframes[i].lump = lumps;

        if(!sprtemp[i].rotate)
        {
            lumps[0] = sprtemp[i].lump[0];
            lumps[1] = -1;
            lumps += 2;
        }
        else
        {
            for(l = 0; l < 8; l++)
                lumps[l] = sprtemp[i].lump[l];

            lumps += 8;
        }

#if 0
        int j;

        for(j = 0; j < 8; j++)
        {
            int x;
            int lump = spriteframes[i].lump[j];

            if(lump < 0)
                break;

            lump = lump < 0 ? -(lump + 1) : lump;

            if(!D_strncasecmp(W_GetNameForNum(lump) + 1, "KEY", 3))
                continue;

            if(!D_strncasecmp(W_GetNameForNum(lump), "CANDA0", 6))
                continue;

            if(!D_strncasecmp(W_GetNameForNum(lump), "CBRAA0", 6))
                continue;

            patch_t *patch = (patch_t*)W_POINTLUMPNUM(lump);
            uint8_t *pixels = R_CheckPixels(lump + 1);

            for(x = 0; x < patch->width; x++)
            {
                column_t* column = (column_t*)((byte*)patch +
                                               BIGSHORT(patch->columnofs[x]));

                /* */
                /* draw a masked column */
                /* */
                for(; column->topdelta != 0xff; column++)
                {
                    int count = column->length;
                    uint8_t* src = pixels + column->dataofs;

                    while(count--)
                    {
                        uint8_t pix = *src++;

                        if(pix == 248 || pix == 249)
                            I_Error("%d %d", pix & 0xff, lump);
                    }
                }
            }
        }

#endif
    }

    l = 0;

    for(i = 0; i < numsprites; i++)
    {
        int numframes;

        numframes = sprites[i].numframes;
        sprites[i].firstframe = l;

        l += numframes;
    }
}

void R_InitColormap(boolean doublepix)
{
    int l;

    l = W_CheckNumForName("COLORMAP");
    l -= colormapopt * 2;
    l -= (int)!doublepix;

    doomcolormap = (int8_t *)W_GetLumpData(l);

    if(doublepix)
        dc_colormaps = (void *)((short *)doomcolormap + 128);
    else
        dc_colormaps = (void *)(doomcolormap + 128);

#ifdef MARS
    Mars_CommSlaveClearCache();
#endif
}
