/* w_tool.c
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
/* ---- TIPC port starts here ---- */
#include "sim.h"

int ConnecTile(short x, short y, short *TileAdrPtr, short Command);
short tally(short tileValue);
short checkSize(short temp);
short checkBigZone(short id, short *deltaHPtr, short *deltaVPtr);
void check3x3border(short xMap, short yMap);
void check4x4border(short xMap, short yMap);
void check6x6border(short xMap, short yMap);
int put3x3Rubble(short x, short y);
int put4x4Rubble(short x, short y);
int put6x6Rubble(short x, short y);
#define DidTool(v, n, x, y)
#define UpdateFunds() 0

int Players = 1;
int Expensive = 1000;

QUAD CostOf[] = {
    100,    100,    100,    500,
      0,    500,      5,      1,
     20,     10,      0,      0,
   5000,     10,   3000,   3000,
   5000,  10000,    100,      0,
};

int
putDownPark(SimView *view, short mapH, short mapV)
{
  short value, tile;

  if (TotalFunds - CostOf[parkState] >= 0) {
    value = Rand(4);

    if (value == 4)
      tile = FOUNTAIN | BURNBIT | BULLBIT | ANIMBIT;
    else
      tile = (value + WOODS2) | BURNBIT | BULLBIT;

    if (Map[mapH][mapV] == 0) {
      Spend(CostOf[parkState]);
      UpdateFunds();
      Map[mapH][mapV] = tile;
      return 1;
    }
    return -1;
  }
  return -2;
}

int
putDownNetwork(SimView *view, short mapH, short mapV)
{
  int tile = Map[mapH][mapV] & LOMASK;

  if ((TotalFunds > 0) && tally(tile)) {
    Map[mapH][mapV] = tile = 0;
    Spend(1);
  }

  if (tile == 0) {
    if ((TotalFunds - CostOf[view->tool_state]) >= 0) {
      Map[mapH][mapV] = TELEBASE | CONDBIT | BURNBIT | BULLBIT | ANIMBIT;
      Spend(CostOf[view->tool_state]);
      return 1;
    } else {
      return -2;
    }
  } else {
    return -1;
  }
}

short
checkBigZone(short id, short *deltaHPtr, short *deltaVPtr)
{
  switch (id) {
  case POWERPLANT:	/* check coal plant */
  case PORT:		/* check sea port */
  case NUCLEAR:		/* check nuc plant */
  case STADIUM:		/* check stadium */
    *deltaHPtr = 0;	*deltaVPtr = 0;		return (4);

  case POWERPLANT + 1:	/* check coal plant */
  case COALSMOKE3:	/* check coal plant, smoke */
  case COALSMOKE3 + 1:	/* check coal plant, smoke */
  case COALSMOKE3 + 2:	/* check coal plant, smoke */
  case PORT + 1:	/* check sea port */
  case NUCLEAR + 1:	/* check nuc plant */
  case STADIUM + 1:	/* check stadium */
    *deltaHPtr = -1;	*deltaVPtr = 0;		return (4);

  case POWERPLANT + 4:	/* check coal plant */
  case PORT + 4:	/* check sea port */
  case NUCLEAR + 4:	/* check nuc plant */
  case STADIUM + 4:	/* check stadium */
    *deltaHPtr = 0;	*deltaVPtr = -1;	return (4);

  case POWERPLANT + 5:	/* check coal plant */
  case PORT + 5:	/* check sea port */
  case NUCLEAR + 5:	/* check nuc plant */
  case STADIUM + 5:	/* check stadium */
    *deltaHPtr = -1;	*deltaVPtr = -1;	return (4);

    /* check airport */
    /*** first row ***/
  case AIRPORT:
    *deltaHPtr = 0;	*deltaVPtr = 0;		return (6);
  case AIRPORT + 1:
    *deltaHPtr = -1;	*deltaVPtr = 0;		return (6);
  case AIRPORT + 2:
    *deltaHPtr = -2;	*deltaVPtr = 0;		return (6);
  case AIRPORT + 3:
    *deltaHPtr = -3;	*deltaVPtr = 0;		return (6);

    /*** second row ***/
  case AIRPORT + 6:
    *deltaHPtr = 0;	*deltaVPtr = -1;	return (6);
  case AIRPORT + 7:
    *deltaHPtr = -1;	*deltaVPtr = -1;	return (6);
  case AIRPORT + 8:
    *deltaHPtr = -2;	*deltaVPtr = -1;	return (6);
  case AIRPORT + 9:
    *deltaHPtr = -3;	*deltaVPtr = -1;	return (6);

    /*** third row ***/
  case AIRPORT + 12:
    *deltaHPtr = 0;	*deltaVPtr = -2;	return (6);
  case AIRPORT + 13:
    *deltaHPtr = -1;	*deltaVPtr = -2;	return (6);
  case AIRPORT + 14:
    *deltaHPtr = -2;	*deltaVPtr = -2;	return (6);
  case AIRPORT + 15:
    *deltaHPtr = -3;	*deltaVPtr = -2;	return (6);

    /*** fourth row ***/
  case AIRPORT + 18:
    *deltaHPtr = 0;	*deltaVPtr = -3;	return (6);
  case AIRPORT + 19:
    *deltaHPtr = -1;	*deltaVPtr = -3;	return (6);
  case AIRPORT + 20:
    *deltaHPtr = -2;	*deltaVPtr = -3;	return (6);
  case AIRPORT + 21:
    *deltaHPtr = -3;	*deltaVPtr = -3;	return (6);

  default:
    *deltaHPtr = 0;	*deltaVPtr = 0;		return (0);
  }
}

short
tally(short tileValue)
{
  /* can we autobulldoze this tile? */
  if (((tileValue >= FIRSTRIVEDGE) &&
       (tileValue <= LASTRUBBLE)) || 
      ((tileValue >= (POWERBASE + 2)) &&
       (tileValue <= (POWERBASE + 12))) ||
      ((tileValue >= TINYEXP) &&
       (tileValue <= (LASTTINYEXP + 2)))) { /* ??? */
    return (1);
  } else {
    return (0);
  }
}

short
checkSize(short temp)
{
  /* check for the normal com, resl, ind 3x3 zones & the fireDept & PoliceDept */
  if (((temp >= (RESBASE - 1)) && (temp  <= (PORTBASE - 1))) ||
      ((temp >= (LASTPOWERPLANT + 1)) && (temp <= (POLICESTATION + 4)))) {
    return (3);
  } else if (((temp >= PORTBASE) && (temp <= LASTPORT)) ||
	     ((temp >= COALBASE) && (temp <= LASTPOWERPLANT)) ||
	     ((temp >= STADIUMBASE) && (temp <= LASTZONE))) {
    return (4);
  }
  return (0);
}

void
check3x3border(short xMap, short yMap)
{
  short xPos, yPos;
  short cnt;

  xPos = xMap; yPos = yMap - 1;
  for (cnt = 0; cnt < 3; cnt++) {
    /*** this will do the upper bordering row ***/
    ConnecTile(xPos, yPos, &Map[xPos][yPos], 0);
    xPos++;
  }

  xPos = xMap - 1; yPos = yMap;
  for (cnt = 0; cnt < 3; cnt++) {
    /*** this will do the left bordering row ***/
    ConnecTile(xPos, yPos, &Map[xPos][yPos], 0);
    yPos++;
  }

  xPos = xMap; yPos = yMap + 3;
  for (cnt = 0; cnt < 3; cnt++) {
    /*** this will do the bottom bordering row ***/
    ConnecTile(xPos, yPos, &Map[xPos][yPos], 0);
    xPos++;
  }
	
  xPos = xMap + 3; yPos = yMap;
  for (cnt = 0; cnt < 3; cnt++) {
    /*** this will do the right bordering row ***/
    ConnecTile(xPos, yPos, &Map[xPos][yPos], 0);
    yPos++;
  }
}

int
check3x3(SimView *view, short mapH, short mapV, short base, short tool)
{
  register short rowNum, columnNum;
  register short holdMapH, holdMapV;
  short xPos, yPos;
  short cost = 0;
  short tileValue;
  short flag;

  mapH--; mapV--;
  if ((mapH < 0) || (mapH > (WORLD_X - 3)) ||
      (mapV < 0) || (mapV > (WORLD_Y - 3))) {
    return -1;
  }

  xPos = holdMapH = mapH;
  yPos = holdMapV = mapV;

  flag = 1;

  for (rowNum = 0; rowNum <= 2; rowNum++) {
    mapH = holdMapH;

    for (columnNum = 0; columnNum <= 2; columnNum++) {
      tileValue = Map[mapH++][mapV] & LOMASK;

      if (autoBulldoze) {
	/* if autoDoze is enabled, add up the cost of bulldozed tiles */
	if (tileValue != 0) {
	  if (tally(tileValue)) {
	    cost++;
	  } else {
	    flag = 0;
	  }
	}
      } else {
	/* check and see if the tile is clear or not  */
	if (tileValue != 0) {
	  flag = 0;
	}
      }
    }
    mapV++;
  }

  if (flag == 0) {
    return -1;
  }

  cost += CostOf[tool];

  if ((TotalFunds - cost) < 0) {
    return -2;
  }


  /* take care of the money situtation here */
  Spend(cost);
  UpdateFunds();

  mapV = holdMapV;

  for (rowNum = 0; rowNum <= 2; rowNum++) {
    mapH = holdMapH;

    for (columnNum = 0; columnNum <= 2; columnNum++) {
      if (columnNum == 1 && rowNum == 1) {
	Map[mapH++][mapV] = base + BNCNBIT + ZONEBIT;
      } else {
	Map[mapH++][mapV] = base + BNCNBIT;
      }
      base++;
    }
    mapV++;
  }
  check3x3border(xPos, yPos);
  return 1;
}

void
check4x4border(short xMap, short yMap)
{
  short *tilePtr;
  short xPos, yPos;
  short cnt;

  xPos = xMap; yPos = yMap - 1;
  for (cnt = 0; cnt < 4; cnt++) {
    /* this will do the upper bordering row */
    tilePtr = &Map[xPos][yPos];
    ConnecTile(xPos, yPos, tilePtr, 0);
    xPos++;
  }

  xPos = xMap - 1; yPos = yMap;
  for (cnt = 0; cnt < 4; cnt++) {
    /* this will do the left bordering row */
    tilePtr = &Map[xPos][yPos];
    ConnecTile(xPos, yPos, tilePtr, 0);
    yPos++;
  }

  xPos = xMap; yPos = yMap + 4;
  for (cnt = 0; cnt < 4;cnt++) {
    /* this will do the bottom bordering row */
    tilePtr = &Map[xPos][yPos];
    ConnecTile(xPos, yPos, tilePtr, 0);
    xPos++;
  }
	
  xPos = xMap + 4; yPos = yMap;
  for (cnt = 0; cnt < 4; cnt++) {
    /* this will do the right bordering row */
    tilePtr = &Map[xPos][yPos];
    ConnecTile(xPos, yPos, tilePtr, 0);
    yPos++;
  }
}

short
check4x4(SimView *view, short mapH, short mapV,
	 short base, short aniFlag, short tool)
{
  register short rowNum, columnNum;
  short h, v;
  short holdMapH;
  short xMap, yMap;
  short tileValue;
  short flag;
  short cost = 0;

  mapH--; mapV--;
  if ((mapH < 0) || (mapH > (WORLD_X - 4)) ||
      (mapV < 0) || (mapV > (WORLD_Y - 4))) {
    return -1;
  }

  h = xMap = holdMapH = mapH;
  v = yMap = mapV;

  flag = 1;

  for (rowNum = 0; rowNum <= 3; rowNum++) {
    mapH = holdMapH;

    for (columnNum = 0; columnNum <= 3; columnNum++) {
      tileValue = Map[mapH++][mapV] & LOMASK;

      if (autoBulldoze) {
	/* if autoDoze is enabled, add up the cost of bulldozed tiles */
	if (tileValue != 0) {
	  if (tally(tileValue)) {
	    cost++;
	  } else {
	    flag = 0;
	  }
	}
      } else {
	/* check and see if the tile is clear or not  */
	if (tileValue != 0) {
	  flag = 0;
	}
      }
    }
    mapV++;
  }

  if (flag == 0) {
    return -1;
  }

  cost += CostOf[tool];

  if ((TotalFunds - cost) < 0) {
    return -2;
  }


  /* take care of the money situtation here */
  Spend(cost);
  UpdateFunds();

  mapV = v; holdMapH = h;

  for (rowNum = 0; rowNum <= 3; rowNum++) {
    mapH = holdMapH;

    for (columnNum = 0; columnNum <= 3; columnNum++) {
      if (columnNum == 1 && rowNum == 1)
	Map[mapH++][mapV] = base + BNCNBIT + ZONEBIT;
      else if (columnNum == 1 && rowNum == 2 && aniFlag)
	Map[mapH++][mapV] = base + BNCNBIT + ANIMBIT;
      else
	Map[mapH++][mapV] = base + BNCNBIT;
      base++;
    }
    mapV++;
  }
  check4x4border(xMap, yMap);
  return 1;
}

void
check6x6border(short xMap, short yMap)
{
  short xPos, yPos;
  short cnt;

  xPos = xMap; yPos = yMap - 1;
  for (cnt = 0; cnt < 6; cnt++) {
    /* this will do the upper bordering row */
    ConnecTile(xPos, yPos, &Map[xPos][yPos], 0);
    xPos++;
  }

  xPos = xMap - 1; yPos = yMap;
  for (cnt=0; cnt < 6; cnt++) {
    /* this will do the left bordering row */
    ConnecTile(xPos, yPos, &Map[xPos][yPos], 0);
    yPos++;
  }

  xPos = xMap; yPos = yMap + 6;
  for (cnt = 0; cnt < 6; cnt++) {
    /* this will do the bottom bordering row */
    ConnecTile(xPos, yPos, &Map[xPos][yPos], 0);
    xPos++;
  }
	
  xPos = xMap + 6; yPos = yMap;
  for (cnt = 0; cnt < 6; cnt++) {
    /* this will do the right bordering row */
    ConnecTile(xPos, yPos, &Map[xPos][yPos], 0);
    yPos++;
  }
}

short
check6x6(SimView *view, short mapH, short mapV, short base, short tool)
{
  register short rowNum, columnNum;
  short h, v;
  short holdMapH;
  short xMap, yMap;
  short flag;
  short tileValue;
  short cost = 0;

  mapH--; mapV--;
  if ((mapH < 0) || (mapH > (WORLD_X - 6)) ||
      (mapV < 0) || (mapV > (WORLD_Y - 6)))
    return -1;

  h = xMap = holdMapH = mapH;
  v = yMap = mapV;

  flag = 1;

  for (rowNum = 0; rowNum <= 5; rowNum++) {
    mapH = holdMapH;

    for (columnNum = 0; columnNum <= 5; columnNum++) {
      tileValue = Map[mapH++][mapV] & LOMASK;

      if (autoBulldoze) {
	/* if autoDoze is enabled, add up the cost of bulldozed tiles */
	if (tileValue != 0)
	  if (tally(tileValue)) {
	    cost++;
	  } else {
	    flag = 0;
	  }
      } else {
	/* check and see if the tile is clear or not  */
	if (tileValue != 0) {
	  flag = 0;
	}
      }
    }
    mapV++;
  }

  if (flag == 0) {
    return -1;
  }

  cost += CostOf[tool];

  if ((TotalFunds - cost) < 0) {
    return -2;
  }


  /* take care of the money situtation here */
  Spend(cost);
  UpdateFunds();

  mapV = v; holdMapH = h;

  for (rowNum = 0; rowNum <= 5; rowNum++) {
    mapH = holdMapH;

    for (columnNum = 0; columnNum <= 5; columnNum++) {
      if (columnNum == 1 && rowNum == 1) {
	Map[mapH++][mapV] = base + BNCNBIT + ZONEBIT;
      } else {
	Map[mapH++][mapV] = base + BNCNBIT;
      }
      base++;
    }
    mapV++;
  }
  check6x6border(xMap, yMap);
  return 1;
}

put3x3Rubble(short x, short y)
{
  register xx, yy, zz;
	
  for (xx = x - 1; xx < x + 2; xx++) {
    for (yy = y - 1; yy < y + 2; yy++)  {
      if (TestBounds(xx, yy)) {
	zz = Map[xx][yy] & LOMASK;
	if ((zz != RADTILE) && (zz != 0)) {
	  Map[xx][yy] =
	    (DoAnimation
	     ? (TINYEXP + Rand(2))
	     : SOMETINYEXP)
	    | ANIMBIT | BULLBIT;
	}
      }
    }
  }
}

put4x4Rubble(short x, short y)
{
  register xx, yy, zz;
	
  for (xx = x - 1; xx < x + 3; xx++) {
    for (yy = y - 1; yy < y + 3; yy++) {
      if (TestBounds(xx, yy)) {
	zz = Map[xx][yy] & LOMASK;
	if ((zz != RADTILE) && (zz != 0)) {
	  Map[xx][yy] =
	    (DoAnimation
	     ? (TINYEXP + Rand(2))
	     : SOMETINYEXP)
	    | ANIMBIT | BULLBIT;
	}
      }
    }
  }
}

put6x6Rubble(short x, short y)
{
  register xx, yy, zz;

  for (xx = x - 1; xx < x + 5; xx++) {
    for (yy = y - 1; yy < y + 5; yy++)  {
      if (TestBounds(xx, yy)) {
	zz = Map[xx][yy] & LOMASK;
	if ((zz != RADTILE) && (zz != 0)) {
	  Map[xx][yy] =
	    (DoAnimation
	     ? (TINYEXP + Rand(2))
	     : SOMETINYEXP)
	    | ANIMBIT | BULLBIT;
	}
      }
    }
  }
}	


int
bulldozer_tool(SimView *view, short x, short y)
{
  unsigned short currTile, temp;
  short zoneSize, deltaH, deltaV;
  int result = 1;

  if ((x < 0) || (x > (WORLD_X - 1)) ||
      (y < 0) || (y > (WORLD_Y - 1))) {
    return -1;
  }

  currTile = Map[x][y];
  temp = currTile & LOMASK;

  if (currTile & ZONEBIT) { /* zone center bit is set */
    if (TotalFunds > 0) {
      Spend(1);
      switch (checkSize(temp)) {
      case 3:
	MakeSound("city", "Explosion-High");
	put3x3Rubble(x, y);
	break;

      case 4:
	put4x4Rubble(x, y);
	MakeSound("city", "Explosion-Low");
	break;

      case 6: 
	MakeSound("city", "Explosion-High");
	MakeSound("city", "Explosion-Low");
	put6x6Rubble(x, y);
	break;

      default:
	break;
      }
    }
  } else if ((zoneSize = checkBigZone(temp, &deltaH, &deltaV))) {
    if (TotalFunds > 0) {
      Spend(1);
      switch (zoneSize) {
      case 3:
	MakeSound("city", "Explosion-High");
	break;

      case 4:
	MakeSound("city", "Explosion-Low");
	put4x4Rubble(x + deltaH, y + deltaV);
	break;

      case 6: 
	MakeSound("city", "Explosion-High");
	MakeSound("city", "Explosion-Low");
	put6x6Rubble(x + deltaH, y + deltaV);
	break;
      }
    }
  } else {
    if (temp == RIVER || temp == REDGE || temp == CHANNEL) {
      if (TotalFunds >= 6) {
	result = ConnecTile(x, y, &Map[x][y], 1);
	if (temp != (Map[x][y] & LOMASK)) {
	  Spend(5);
	}
      } else {
	result = 0;
      }
    } else {
      result = ConnecTile(x, y, &Map[x][y], 1);
    }
  }
  UpdateFunds();
  if (result == 1) {
    DidTool(view, "Dozr", x, y);
  }
  return result;
}

int
road_tool(SimView *view, short x, short y)
{
  int result;

  if ((x < 0) || (x > (WORLD_X - 1)) ||
      (y < 0) || (y > (WORLD_Y - 1))) {
    return -1;
  }

  result = ConnecTile(x, y, &Map[x][y], 2);
  UpdateFunds();
  if (result == 1) {
    DidTool(view, "Road", x, y);
  }
  return result;
}

int
rail_tool(SimView *view, short x, short y)
{
  int result;

  if ((x < 0) || (x > (WORLD_X - 1)) ||
      (y < 0) || (y > (WORLD_Y - 1))) {
    return -1;
  }

  result = ConnecTile(x, y, &Map[x][y], 3);
  UpdateFunds();
  if (result == 1) {
    DidTool(view, "Rail", x, y);
  }
  return result;
}

int
wire_tool(SimView *view, short x, short y)
{
  int result;

  if ((x < 0) || (x > (WORLD_X - 1)) ||
      (y < 0) || (y > (WORLD_Y - 1))) {
    return -1;
  }

  result = ConnecTile(x, y, &Map[x][y], 4);
  UpdateFunds();
  if (result == 1) {
    DidTool(view, "Wire", x, y);
  }
  return result;
}

int
park_tool(SimView *view, short x, short y)
{
  int result;

  if ((x < 0) || (x > (WORLD_X - 1)) ||
      (y < 0) || (y > (WORLD_Y - 1)))
    return -1;

  result = putDownPark(view, x, y);
  if (result == 1) {
    DidTool(view, "Park", x, y);
  }
  return result;
}

int
residential_tool(SimView *view, short x, short y)
{
  int result;

  if ((x < 0) || (x > (WORLD_X - 1)) ||
      (y < 0) || (y > (WORLD_Y - 1))) {
    return -1;
  }

  result = check3x3(view, x, y, RESBASE, residentialState);
  if (result == 1) {
    DidTool(view, "Res", x, y);
  }
  return result;
}

int
commercial_tool(SimView *view, short x, short y)
{
  int result;

  if ((x < 0) || (x > (WORLD_X - 1)) ||
      (y < 0) || (y > (WORLD_Y - 1))) {
    return -1;
  }

  result = check3x3(view, x, y, COMBASE, commercialState);
  if (result == 1) {
    DidTool(view, "Com", x, y);
  }
  return result;
}

int
industrial_tool(SimView *view, short x, short y)
{
  int result;

  if ((x < 0) || (x > (WORLD_X - 1)) ||
      (y < 0) || (y > (WORLD_Y - 1))) {
    return -1;
  }

  result = check3x3(view, x, y, INDBASE, industrialState);
  if (result == 1) {
    DidTool(view, "Ind", x, y);
  }
  return result;
}

int
police_dept_tool(SimView *view, short x, short y)
{
  int result;

  if ((x < 0) || (x > (WORLD_X - 1)) ||
      (y < 0) || (y > (WORLD_Y - 1))) {
    return -1;
  }

  result = check3x3(view, x, y, POLICESTBASE, policeState);
  if (result == 1) {
    DidTool(view, "Pol", x, y);
  }
  return result;
}

int
fire_dept_tool(SimView *view, short x, short y)
{
  int result;

  if ((x < 0) || (x > (WORLD_X - 1)) ||
      (y < 0) || (y > (WORLD_Y - 1))) {
    return -1;
  }

  result = check3x3(view, x, y, FIRESTBASE, fireState);
  if (result == 1) {
    DidTool(view, "Fire", x, y);
  }
  return result;
}

int
stadium_tool(SimView *view, short x, short y)
{
  int result;

  if ((x < 0) || (x > (WORLD_X - 1)) ||
      (y < 0) || (y > (WORLD_Y - 1))) {
    return -1;
  }

  result = check4x4(view, x, y, STADIUMBASE, 0, stadiumState);
  if (result == 1) {
    DidTool(view, "Stad", x, y);
  }
  return result;
}

int
coal_power_plant_tool(SimView *view, short x, short y)
{
  int result;

  if ((x < 0) || (x > (WORLD_X - 1)) ||
      (y < 0) || (y > (WORLD_Y - 1))) {
    return -1;
  }

  result = check4x4(view, x, y, COALBASE, 1, powerState);
  if (result == 1) {
    DidTool(view, "Coal", x, y);
  }
  return result;
}

int
nuclear_power_plant_tool(SimView *view, short x, short y)
{
  int result;

  if ((x < 0) || (x > (WORLD_X - 1)) ||
      (y < 0) || (y > (WORLD_Y - 1))) {
    return -1;
  }

  result = check4x4(view, x, y, NUCLEARBASE, 1, nuclearState);
  if (result == 1) {
    DidTool(view, "Nuc", x, y);
  }
  return result;
}

int
seaport_tool(SimView *view, short x, short y)
{
  int result;

  if ((x < 0) || (x > (WORLD_X - 1)) ||
      (y < 0) || (y > (WORLD_Y - 1))) {
    return -1;
  }

  result = check4x4(view, x, y, PORTBASE, 0, seaportState);
  if (result == 1) {
    DidTool(view, "Seap", x, y);
  }
  return result;
}

int
airport_tool(SimView *view, short x, short y)
{
  int result;

  if ((x < 0) || (x > (WORLD_X - 1)) ||
      (y < 0) || (y > (WORLD_Y - 1))) {
    return -1;
  }

  result = check6x6(view, x, y, AIRPORTBASE, airportState);
  if (result == 1) {
    DidTool(view, "Airp", x, y);
  }
  return result;
}

int
network_tool(SimView *view, short x, short y)
{
  int result;

  if ((x < 0) || (x > (WORLD_X - 1)) ||
      (y < 0) || (y > (WORLD_Y - 1))) {
    return -1;
  }

  result = putDownNetwork(view, x, y);
  if (result == 1) {
    DidTool(view, "Net", x, y);
  }
  return result;
}

/* TIPC: one entry point, tile coordinates. Result 1 ok, -1 bad location, -2 no money, 0 nothing. */
int do_tool_xy(short state, short x, short y)
{
  switch (state) {
  case residentialState: return residential_tool(NULL, x, y);
  case commercialState:  return commercial_tool(NULL, x, y);
  case industrialState:  return industrial_tool(NULL, x, y);
  case fireState:        return fire_dept_tool(NULL, x, y);
  case policeState:      return police_dept_tool(NULL, x, y);
  case wireState:        return wire_tool(NULL, x, y);
  case dozeState:        return bulldozer_tool(NULL, x, y);
  case rrState:          return rail_tool(NULL, x, y);
  case roadState:        return road_tool(NULL, x, y);
  case stadiumState:     return stadium_tool(NULL, x, y);
  case parkState:        return park_tool(NULL, x, y);
  case seaportState:     return seaport_tool(NULL, x, y);
  case powerState:       return coal_power_plant_tool(NULL, x, y);
  case nuclearState:     return nuclear_power_plant_tool(NULL, x, y);
  case airportState:     return airport_tool(NULL, x, y);
  case networkState:     return network_tool(NULL, x, y);
  }
  return 0;
}
