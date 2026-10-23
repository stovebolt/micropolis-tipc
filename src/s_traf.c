/* s_traf.c
 *
 * Micropolis, Unix Version.  This game was released for the Unix platform
 * in or about 1990 and has been modified for inclusion in the One Laptop
 * Per Child program.  Copyright (C) 1989 - 2007 Electronic Arts Inc.  If
 * you need assistance with this program, you may contact:
 *   http://wiki.laptop.org/go/Micropolis  or email  micropolis@laptop.org.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.  You should have received a
 * copy of the GNU General Public License along with this program.  If
 * not, see <http://www.gnu.org/licenses/>.
 * 
 *             ADDITIONAL TERMS per GNU GPL Section 7
 * 
 * No trademark or publicity rights are granted.  This license does NOT
 * give you any right, title or interest in the trademark SimCity or any
 * other Electronic Arts trademark.  You may not distribute any
 * modification of this program using the trademark SimCity or claim any
 * affliation or association with Electronic Arts Inc. or its employees.
 * 
 * Any propagation or conveyance of this program must include this
 * copyright notice and these terms.
 * 
 * If you convey this program (or any modifications of it) and assume
 * contractual liability for the program to recipients of it, you agree
 * to indemnify Electronic Arts for any liability that those contractual
 * assumptions impose on Electronic Arts.
 * 
 * You may not misrepresent the origins of this program; modified
 * versions of the program must be marked as such and not identified as
 * the original program.
 * 
 * This disclaimer supplements the one included in the General Public
 * License.  TO THE FULLEST EXTENT PERMISSIBLE UNDER APPLICABLE LAW, THIS
 * PROGRAM IS PROVIDED TO YOU "AS IS," WITH ALL FAULTS, WITHOUT WARRANTY
 * OF ANY KIND, AND YOUR USE IS AT YOUR SOLE RISK.  THE ENTIRE RISK OF
 * SATISFACTORY QUALITY AND PERFORMANCE RESIDES WITH YOU.  ELECTRONIC ARTS
 * DISCLAIMS ANY AND ALL EXPRESS, IMPLIED OR STATUTORY WARRANTIES,
 * INCLUDING IMPLIED WARRANTIES OF MERCHANTABILITY, SATISFACTORY QUALITY,
 * FITNESS FOR A PARTICULAR PURPOSE, NONINFRINGEMENT OF THIRD PARTY
 * RIGHTS, AND WARRANTIES (IF ANY) ARISING FROM A COURSE OF DEALING,
 * USAGE, OR TRADE PRACTICE.  ELECTRONIC ARTS DOES NOT WARRANT AGAINST
 * INTERFERENCE WITH YOUR ENJOYMENT OF THE PROGRAM; THAT THE PROGRAM WILL
 * MEET YOUR REQUIREMENTS; THAT OPERATION OF THE PROGRAM WILL BE
 * UNINTERRUPTED OR ERROR-FREE, OR THAT THE PROGRAM WILL BE COMPATIBLE
 * WITH THIRD PARTY SOFTWARE OR THAT ANY ERRORS IN THE PROGRAM WILL BE
 * CORRECTED.  NO ORAL OR WRITTEN ADVICE PROVIDED BY ELECTRONIC ARTS OR
 * ANY AUTHORIZED REPRESENTATIVE SHALL CREATE A WARRANTY.  SOME
 * JURISDICTIONS DO NOT ALLOW THE EXCLUSION OF OR LIMITATIONS ON IMPLIED
 * WARRANTIES OR THE LIMITATIONS ON THE APPLICABLE STATUTORY RIGHTS OF A
 * CONSUMER, SO SOME OR ALL OF THE ABOVE EXCLUSIONS AND LIMITATIONS MAY
 * NOT APPLY TO YOU.
 */
#include "sim.h"


/* Traffic Generation */


#define MAXDIS 30

short PosStackN, SMapXStack[MAXDIS+1], SMapYStack[MAXDIS+1];
short LDir;
short Zsource;
short TrafMaxX, TrafMaxY;


/* comefrom: DoIndustrial DoCommercial DoResidential */
MakeTraf(int Zt)
{
  short xtem, ytem;

  xtem = SMapX;
  ytem = SMapY;
  Zsource = Zt;
  PosStackN = 0;

#if 0
  if ((!Rand(2)) && FindPTele()) {
/* printf("Telecommute!\n"); */
    return (TRUE);
  }
#endif

  if (FindPRoad()) {		/* look for road on zone perimeter */
    if (TryDrive()) {		/* attempt to drive somewhere */
      SetTrafMem();		/* if sucessful, inc trafdensity */
      SMapX = xtem;
      SMapY = ytem;
      return (TRUE);		/* traffic passed */
    }
    SMapX = xtem;
    SMapY = ytem;
    return (FALSE);		/* traffic failed */
  }
  else return (-1);		/* no road found */
}


/* comefrom: MakeTraf */
SetTrafMem(void)
{
  register short x, z;

  for (x = PosStackN; x > 0; x--) {
    PullPos();
    if (TestBounds(SMapX, SMapY)) {
      z = MAPXY(SMapX, SMapY) & LOMASK;
      if ((z >= ROADBASE) && (z < POWERBASE)) {
	SimSprite SPRNEAR *sprite;

	/* check for rail */
	z = TrfDensity[SMapX >>1][SMapY >>1];
	z += 50;
	if ((z > 240) &&
	    (!Rand(5))) {
	  z = 240;
	  TrafMaxX = SMapX <<4;
	  TrafMaxY = SMapY <<4;
	  if (((sprite = GetSprite(COP)) != NULL) &&
	      (sprite->control == -1)) {
	    sprite->dest_x = TrafMaxX;
	    sprite->dest_y = TrafMaxY;
	  }
	}
	TrfDensity[SMapX >>1][SMapY >>1] = z;
      }
    }
  }
}


/* comefrom: TryGo */
PushPos(void)
{
  PosStackN++;
  SMapXStack[PosStackN] = SMapX;
  SMapYStack[PosStackN] = SMapY;
}


/* comefrom: SetTrafMem */
PullPos(void)
{
  SMapX = SMapXStack[PosStackN];
  SMapY = SMapYStack[PosStackN];
  PosStackN--;
}


/* comefrom: DoSPZone MakeTraf */
FindPRoad(void)		/* look for road on edges of zone   */
{
  static short PerimX[12] = {-1, 0, 1, 2, 2, 2, 1, 0,-1,-2,-2,-2};
  static short PerimY[12] = {-2,-2,-2,-1, 0, 1, 2, 2, 2, 1, 0,-1};
  register short tx, ty, z;

  for (z = 0; z < 12; z++) {
	  tx = SMapX + PerimX[z];
	  ty = SMapY + PerimY[z];
	  if (TestBounds(tx, ty)) {
		  if (RoadTest(MAPXY(tx, ty))) {
			  SMapX = tx;
			  SMapY = ty;
			  return (TRUE);
		  }
	  }
  }
  return (FALSE);
}


FindPTele(void)		/* look for telecommunication on edges of zone */
{
  static short PerimX[12] = {-1, 0, 1, 2, 2, 2, 1, 0,-1,-2,-2,-2};
  static short PerimY[12] = {-2,-2,-2,-1, 0, 1, 2, 2, 2, 1, 0,-1};
  register short tx, ty, z, tile;

  for (z = 0; z < 12; z++) {
	  tx = SMapX + PerimX[z];
	  ty = SMapY + PerimY[z];
	  if (TestBounds(tx, ty)) {
	  	  tile = MAPXY(tx, ty) & LOMASK;
		  if ((tile >= TELEBASE) && (tile <= TELELAST)) {
			  return (TRUE);
		  }
	  }
  }
  return (FALSE);
}


/* comefrom: MakeTraf */
TryDrive(void)
{
  short z;

  LDir = 5;
  for (z = 0; z < MAXDIS; z++) {	/* Maximum distance to try */
    if (TryGo(z)) {			/* if it got a road */
      if (DriveDone())			/* if destination is reached */
	return (TRUE);			/* pass */
    } else {
      if (PosStackN) {			/* deadend , backup */
	PosStackN--;
	z += 3;
      }
      else return (FALSE);		/* give up at start  */	
    }
  }
  return (FALSE);			/* gone maxdis */
}


/* TIPC speed pass: RoadTest as a macro on a masked tile */
#define ROADTILE(t) (((t) >= ROADBASE) && ((t) <= LASTRAIL) && !(((t) >= POWERBASE) && ((t) < RAILHPOWERV)))

/* comefrom: TryDrive */
/* TIPC speed pass: GetFromMap/RoadTest/MoveMapSim inlined; same order of
   directions and the same single Rand16() draw per call. */
TryGo(int z)
{
  register short x, realdir;
  short rdir, t, sx = SMapX, sy = SMapY;
  int pi = MapCol[sx] + sy;

  rdir = Rand16() & 3;
  for (x = rdir; x < (rdir + 4); x++) {	/* for the 4 directions */
    realdir = x & 3;
    if (realdir == LDir) continue;	/* skip last direction */
    switch (realdir) {
    case 0: if (sy <= 0) continue; t = MapData[pi + (-1)] & LOMASK; break;
    case 1: if (sx >= WORLD_X - 1) continue; t = MapData[pi + (WORLD_Y)] & LOMASK; break;
    case 2: if (sy >= WORLD_Y - 1) continue; t = MapData[pi + (1)] & LOMASK; break;
    default: if (sx <= 0) continue; t = MapData[pi + (-WORLD_Y)] & LOMASK; break;
    }
    if (ROADTILE(t)) {
      switch (realdir) {
      case 0: SMapY = sy - 1; break;
      case 1: SMapX = sx + 1; break;
      case 2: SMapY = sy + 1; break;
      default: SMapX = sx - 1; break;
      }
      LDir = (realdir + 2) & 3;
      if (z & 1)			/* save pos every other move */
	PushPos();
      return (TRUE);
    }
  }
  return (FALSE);
}


/* comefrom: TryGo DriveDone */
GetFromMap(int x)
{
  int pi = MapCol[SMapX] + SMapY;
  switch (x) {
  case 0: return (SMapY > 0) ? (MapData[pi + (-1)] & LOMASK) : FALSE;
  case 1: return (SMapX < (WORLD_X - 1)) ? (MapData[pi + (WORLD_Y)] & LOMASK) : FALSE;
  case 2: return (SMapY < (WORLD_Y - 1)) ? (MapData[pi + (1)] & LOMASK) : FALSE;
  case 3: return (SMapX > 0) ? (MapData[pi + (-WORLD_Y)] & LOMASK) : FALSE;
  default: return (FALSE);
  }
}


/* comefrom: TryDrive */
DriveDone(void)
{
  static short TARGL[3] = {COMBASE, LHTHR, LHTHR};
  static short TARGH[3] = {NUCLEAR, PORT, COMBASE};	/* for destinations */
  register short z, l, h;
  int pi = MapCol[SMapX] + SMapY;	/* TIPC speed pass */

  l = TARGL[Zsource];
  h = TARGH[Zsource];
  if (SMapY > 0) { z = MapData[pi + (-1)] & LOMASK; if ((z >= l) && (z <= h)) return (TRUE); }
  if (SMapX < (WORLD_X - 1)) { z = MapData[pi + (WORLD_Y)] & LOMASK; if ((z >= l) && (z <= h)) return (TRUE); }
  if (SMapY < (WORLD_Y - 1)) { z = MapData[pi + (1)] & LOMASK; if ((z >= l) && (z <= h)) return (TRUE); }
  if (SMapX > 0) { z = MapData[pi + (-WORLD_Y)] & LOMASK; if ((z >= l) && (z <= h)) return (TRUE); }
  return (FALSE);
}


/* comefrom: TryGo FindPRoad */
RoadTest(int x)
{
  x = x & LOMASK;
  if (x < ROADBASE)
    return (FALSE);
  if (x > LASTRAIL)
    return (FALSE);
  if ((x >= POWERBASE) && (x < RAILHPOWERV))
    return (FALSE);
  return (TRUE);
}

