/* o_main.c -- options menu */

#include "doomdef.h"
#include "p_local.h"
#include "st_main.h"

#define MOVEWAIT	5
#define ITEMSPACE	40
#define SLIDEWIDTH 90

extern 	int	cx, cy;
extern	int		sfxvolume;		/* range from 0 to 255 */
extern	int		controltype;				/* 0 to 5 */

extern void print (int x, int y, char *string);
extern void IN_DrawValue(int x,int y,int value);

/* action buttons can be set to BT_A, BT_B, or BT_C */
/* strafe and use should be set to the same thing */

extern	unsigned	BT_ATTACK;
extern	unsigned	BT_USE;
extern	unsigned	BT_STRAFE;
extern	unsigned	BT_SPEED;

typedef enum
{
	SFU,
	SUF,
	FSU,
	FUS,
	USF,
	UFS,
	NUMCONTROLOPTIONS
}	control_t;

typedef enum
{
	soundvol,
	screensize,
#ifndef MARS
	controls,
#endif
	NUMMENUITEMS
} menupos_t;

menupos_t	cursorpos;

typedef struct
{
	int		x;
	int		y;
	boolean	hasslider;
	char 		name[20];
} menuitem_t;

menuitem_t menuitem[NUMMENUITEMS];
 
typedef struct
{
	int	curval;
	int	maxval;
} slider_t;

slider_t slider[2];

int		cursorframe, cursorcount;
int		movecount;

int		o_screensize;
extern int clearscreen;

short	uchar;

short	o_cursor1, o_cursor2;
short	o_slider, o_slidertrack;

char buttona[NUMCONTROLOPTIONS][8] =
		{"Speed","Speed","Fire","Fire","Use","Use"};
char buttonb[NUMCONTROLOPTIONS][8] = 
		{"Fire","Use ","Speed","Use","Speed","Fire"};
char buttonc[NUMCONTROLOPTIONS][8] =
		{"Use","Fire","Use","Speed","Fire","Speed"};

void DrawJagobjLump(int lumpnum, int x, int y, int* ow, int* oh);

unsigned configuration[NUMCONTROLOPTIONS][3] =
{
	{BT_A, BT_B, BT_C},
	{BT_A, BT_C, BT_B},
	{BT_B, BT_A, BT_C},
	{BT_C, BT_A, BT_B},
	{BT_B, BT_C, BT_A},
	{BT_C, BT_B, BT_A} 
};


void O_SetButtonsFromControltype (void)
{
	BT_SPEED = configuration[controltype][0];
	BT_ATTACK =	configuration[controltype][1];
	BT_USE = configuration[controltype][2];	
	BT_STRAFE = configuration[controltype][2];	
}


/* */
/* Draw control value */
/* */
void O_DrawControl(void)
{
#ifndef MARS
	//EraseBlock(menuitem[controls].x + 40, menuitem[controls].y + 20, 90, 80);
	print(menuitem[controls].x + 40, menuitem[controls].y + 20, buttona[controltype]);
	print(menuitem[controls].x + 40, menuitem[controls].y + 40, buttonb[controltype]);
	print(menuitem[controls].x + 40, menuitem[controls].y + 60, buttonc[controltype]);
/*	IN_DrawValue(30, 20, controltype); */
	
	O_SetButtonsFromControltype ();
#endif
}

/*
===============
=
= O_Init
=
===============
*/

void O_Init (void)
{
/* the eeprom has set controltype, so set buttons from that */
	O_SetButtonsFromControltype ();

/* cache all needed graphics */
	o_cursor1 = W_GetNumForName("M_SKULL1");
	o_cursor2 = W_GetNumForName("M_SKULL2");
	o_slider = W_GetNumForName("O_SLIDER");
	o_slidertrack = W_GetNumForName("O_STRACK");

	uchar = W_GetNumForName ("CHAR_065");

/*	initialize variables */

	cursorcount = 0;
	cursorframe = 0;
	cursorpos = 0;	
	o_screensize = 0;

/*    strcpy(menuitem[0].name, "Volume"); */
    D_strncpy(menuitem[0].name, "Volume", 6);
	menuitem[0].x = 95;
	menuitem[0].y = 46;
	menuitem[0].hasslider = true;

 	slider[0].maxval = 4;
	slider[0].curval = 4*sfxvolume/64;

	D_strncpy(menuitem[1].name, "Screen size", 11);
	menuitem[1].x = 95;
	menuitem[1].y = 80;
	menuitem[1].hasslider = true;

	slider[1].maxval = 3;
	slider[1].curval = 0;

#ifndef MARS
/*    strcpy(menuitem[2].name, "Controls"); */
    D_strncpy(menuitem[2].name, "Controls", 8); /* Fixed CEF */
	menuitem[2].x = 95;
	menuitem[2].y = 100;
	menuitem[2].hasslider = false;
#endif
}

/*
==================
=
= O_Control
=
= Button bits can be eaten by clearing them in ticbuttons[playernum]
==================
*/

void O_Control (player_t *player)
{
	int		buttons, oldbuttons;
	char	newframe = false;
	
	buttons = ticbuttons[playernum];
	oldbuttons = oldticbuttons[playernum];
	
	if ( (buttons & BT_OPTION) && !(oldbuttons & BT_OPTION) )
	{
		cursorpos = 0;	
		player->automapflags ^= AF_OPTIONSACTIVE;
#ifndef MARS
		if (player->automapflags & AF_OPTIONSACTIVE)
			DoubleBufferSetup ();
		else
			WriteEEProm ();		/* save new settings */
#endif
	}
	if ( !(player->automapflags & AF_OPTIONSACTIVE) )
		return;

/* clear buttons so game player isn't moving aroung */
	ticbuttons[playernum] &= BT_OPTION;	/* leave option status alone */

	if (playernum != consoleplayer)
		return;

/* animate skull */
	if (++cursorcount == ticrate)
	{
		cursorframe ^= 1;
		cursorcount = 0;
		newframe = true;
	}

	if (!newframe)
		return;

/* check for movement */
	if (! (buttons & (JP_UP|JP_DOWN|JP_LEFT|JP_RIGHT) ) )
		movecount = 0;		/* move immediately on next press */
	else
	{
			if (buttons & JP_RIGHT)
			{
				if (menuitem[cursorpos].hasslider)
				{
					slider[cursorpos].curval++;
					if (slider[cursorpos].curval > slider[cursorpos].maxval)
						slider[cursorpos].curval = slider[cursorpos].maxval;
					switch (cursorpos)
					{
					case 0:
						sfxvolume = 64*slider[0].curval / slider[0].maxval;
						S_StartSound (NULL, sfx_pistol);
						break;
					case 1:
						o_screensize = slider[cursorpos].curval;
						R_SetScreenSize(o_screensize);
						clearscreen = 2;
						break;
					default:
						break;
					}
				}
			}
			if (buttons & JP_LEFT)
			{
				if (menuitem[cursorpos].hasslider)
				{
					slider[cursorpos].curval--;
					if (slider[cursorpos].curval < 0)
						slider[cursorpos].curval = 0;
					switch (cursorpos)
					{
					case 0:
						sfxvolume = 64*slider[0].curval / slider[0].maxval;
						S_StartSound (NULL, sfx_pistol);
						break;
					case 1:
						o_screensize = slider[cursorpos].curval;
						R_SetScreenSize(o_screensize);
						clearscreen = 2;
						break;
					default:
						break;
					}
				}
			}

		if (movecount == MOVEWAIT)
			movecount = 0;		/* repeat move */
		if (++movecount == 1)
		{
			if (buttons & JP_DOWN)
			{
				cursorpos++;
				if (cursorpos == NUMMENUITEMS)
					cursorpos = 0;
			}
		
			if (buttons & JP_UP)
			{
				cursorpos--;
				if (cursorpos == -1)
					cursorpos = NUMMENUITEMS-1;
			}
#ifndef MARS
			if (buttons & JP_RIGHT)
			{
				if (cursorpos == controls)
				{
					controltype++;
					if(controltype == NUMCONTROLOPTIONS)
						controltype = (NUMCONTROLOPTIONS - 1); 
				}			
			}
			if (buttons & JP_LEFT)
			{
				if (cursorpos == controls)
				{
					controltype--;
					if(controltype == -1)
						controltype = 0; 
				}
			}
#endif
		}
	}
}

void O_Drawer (void)
{
	int		i;
	int		offset;

/* Erase old and Draw new cursor frame */
	//EraseBlock(56, 40, o_cursor1->width, 200);
	if(cursorframe)
		DrawJagobjLump(o_cursor1, 60, menuitem[cursorpos].y - 2, NULL, NULL);
	else
		DrawJagobjLump(o_cursor2, 60, menuitem[cursorpos].y - 2, NULL, NULL);

/* Draw menu */

	print(104, 10, "Options");
	
	for (i = 0; i < NUMMENUITEMS; i++)
	{
		print(menuitem[i].x, menuitem[i].y, menuitem[i].name);		

		if(menuitem[i].hasslider == true)
		{
			DrawJagobjLump(o_slidertrack , menuitem[i].x + 2, menuitem[i].y + 20, NULL, NULL);
			offset = (slider[i].curval * SLIDEWIDTH) / slider[i].maxval;
			DrawJagobjLump(o_slider, menuitem[i].x + 7 + offset, menuitem[i].y + 20, NULL, NULL);
/*			ST_Num(menuitem[i].x + o_slider->width + 10,	 */
/*			menuitem[i].y + 20,slider[i].curval);  */
		}			 
	}	
	
/* Draw control info */
#ifndef MARS
	print(menuitem[controls].x + 10, menuitem[controls].y + 20, "A");
	print(menuitem[controls].x + 10, menuitem[controls].y + 40, "B");
	print(menuitem[controls].x + 10, menuitem[controls].y + 60, "C");

	O_DrawControl();
#endif

/* debug stuff */
#if 0
	cx = 30;
	cy = 40;
	D_printf("Speed = %d", BT_SPEED);
	cy = 60;
	D_printf("Use/Strafe = %d", BT_SPEED);
	cy = 80;
	D_printf("Fire = %d", BT_SPEED);
#endif
/* end of debug stuff */

#ifndef MARS
	UpdateBuffer();
#endif
}

