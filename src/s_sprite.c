/* s_sprite.c: the original sprite logic (trains, ships, planes, helicopter, monster, tornado,
 * explosions) for Micropolis for TIPC.
 *
 * Derived from w_sprite.c of the GPL v3 Micropolis (SimHacker/micropolis, micropolis-activity/src/sim),
 * Copyright (C) 1989 - 2007 Electronic Arts Inc.  The movement, collision, disaster and generation
 * code is unchanged except as tagged TIPC.  Removed: the Tcl/Tk sprite commands, the X11 drawing and
 * the unused bus sprite.  Added: a sprite switch (SpritesOn), a per-type art mask (SpriteArt, so a build
 * without room for a sprite's art never creates it), and sprites allocated one at a time.
 * Code by Shurr Nuff (AI assistant), produced by Will (stovebolt), Flying Horse Labs. GPL v3.
 */
#include "sim.h"

int absDist;
short Cycle;

SimSprite SPRNEAR *GlobalSprites[OBJN];
SimSprite SPRNEAR *FreeSprites = NULL;

/* TIPC: sprites live in a small static pool in DGROUP so that every sprite pointer is a near pointer
   (the large model's far pointers made the sprite code about 30% bigger). 6 types plus explosions. */
#define NSPRITES 12
static SimSprite SpritePool[NSPRITES];
static int PoolUsed = 0;

static int DoTrainSprite(SimSprite SPRNEAR *sprite), DoCopterSprite(SimSprite SPRNEAR *sprite), DoAirplaneSprite(SimSprite SPRNEAR *sprite),
  DoShipSprite(SimSprite SPRNEAR *sprite), DoMonsterSprite(SimSprite SPRNEAR *sprite), DoTornadoSprite(SimSprite SPRNEAR *sprite),
  DoExplosionSprite(SimSprite SPRNEAR *sprite);
int ExplodeSprite(SimSprite SPRNEAR *sprite);
int CheckSpriteCollision(SimSprite SPRNEAR *s1, SimSprite SPRNEAR *s2);
short SpriteNotInBounds(SimSprite SPRNEAR *sprite);

#define TRA_GROOVE_X -39
#define TRA_GROOVE_Y 6

/* TIPC: SpritesOn switches the traffic sprites (train, ship, plane, helicopter, and the explosions
   they cause); the monster, tornado and explosions run when sprites are on or disasters are on (DisasterSprites).  SpriteArt is a
   bit mask (1<<type) of the sprite types that can be shown: a type without art is never created. */
short SpritesOn = 0;
unsigned short SpriteMade[OBJN];   /* how many of each type were started (test statistics) */
unsigned short SpriteArt = 0xFFFF;
short DisasterSprites = 0;   /* set with the disasters switch (SetDisasters), not by the load's startup value of NoDisasters */

/* TIPC: the disasters switch: random disasters and the monster/tornado/explosion sprites they need. */
void SetDisasters(int on)
{
  NoDisasters = on ? 0 : 1;
  DisasterSprites = on ? 1 : 0;
}

static int SpriteAllowed(int type)
{
  if (!(SpriteArt & (1U << type))) return 0;
  switch (type) {
  case TRA: case COP: case AIR: case SHI: return SpritesOn;
  case GOD: case TOR: case EXP: return SpritesOn || DisasterSprites;   /* also the scenario monster */
  }
  return 0;
}

/* TIPC: switch the traffic sprites on or off; the live ones disappear at the next MoveObjects. */
void SpritesSet(int on)
{
  SimSprite SPRNEAR *s;
  SpritesOn = on ? 1 : 0;
  if (!on)
    for (s = sim->sprite; s != NULL; s = s->next)
      if (s->type == TRA || s->type == COP || s->type == AIR || s->type == SHI) s->frame = 0;
}

SimSprite SPRNEAR *
NewSprite(int type, int x, int y)
{
  SimSprite SPRNEAR *sprite;

  if (!SpriteAllowed(type)) return NULL;
  if (FreeSprites) {
    sprite = FreeSprites;
    FreeSprites = sprite->next;
  } else if (PoolUsed < NSPRITES) {
    sprite = (SimSprite SPRNEAR *)&SpritePool[PoolUsed++];
  } else {
    return NULL;
  }
  sprite->type = type;
  InitSprite(sprite, x, y);
  sprite->next = sim->sprite; sim->sprite = sprite;
  return sprite;
}

InitSprite(SimSprite SPRNEAR *sprite, int x, int y)
{
  SpriteMade[sprite->type]++;
  sprite->x = x; sprite->y = y;
  sprite->frame = 0;
  sprite->orig_x = sprite->orig_y = 0;
  sprite->dest_x = sprite->dest_y = 0;
  sprite->count = sprite->sound_count = 0;
  sprite->dir = sprite->new_dir = 0;
  sprite->step = sprite->flag = 0;
  sprite->control = -1;

  if (GlobalSprites[sprite->type] == NULL) {
    GlobalSprites[sprite->type] = sprite;
  }

  switch (sprite->type) {

  case TRA:
    sprite->x_offset = 32; sprite->y_offset = -16;
    sprite->x_hot = 40; sprite->y_hot = -8;
    sprite->frame = 1;
    sprite->dir = 4;
    break;

  case SHI:
    sprite->x_offset = 32; sprite->y_offset = -16;
    sprite->x_hot = 48; sprite->y_hot = 0;
    if (x < (4 <<4)) sprite->frame = 3;
    else if (x >= ((WORLD_X - 4) <<4)) sprite->frame = 7;
    else if (y < (4 <<4)) sprite->frame = 5;
    else if (y >= ((WORLD_Y - 4) <<4)) sprite->frame = 1;
    else sprite->frame = 3;
    sprite->new_dir = sprite->frame;
    sprite->dir = 10;
    sprite->count = 1;
    break;

  case GOD:
    sprite->x_offset = 24; sprite->y_offset = 0;
    sprite->x_hot = 40; sprite->y_hot = 16;
    if (x > ((WORLD_X <<4) / 2)) {
      if (y > ((WORLD_Y <<4) / 2)) sprite->frame = 10;
      else sprite->frame = 7;
    } else if (y > ((WORLD_Y <<4) / 2)) sprite->frame = 1;
    else sprite->frame = 4;
    sprite->count = 1000;
    sprite->dest_x = PolMaxX <<4;
    sprite->dest_y = PolMaxY <<4;
    sprite->orig_x = sprite->x;
    sprite->orig_y = sprite->y;
    break;

  case COP:
    sprite->x_offset = 32; sprite->y_offset = -16;
    sprite->x_hot = 40; sprite->y_hot = -8;
    sprite->frame = 5;
    sprite->count = 1500;
    sprite->dest_x = Rand((WORLD_X <<4) - 1);
    sprite->dest_y = Rand((WORLD_Y <<4) - 1);
    sprite->orig_x = x - 30;
    sprite->orig_y = y;
    break;

  case AIR:
    sprite->x_offset = 24; sprite->y_offset = 0;
    sprite->x_hot = 48; sprite->y_hot = 16;
    if (x > ((WORLD_X - 20) <<4)) {
      sprite->x -= 100 + 48;
      sprite->dest_x = sprite->x - 200;
      sprite->frame = 7;
    } else {
      sprite->dest_x = sprite->x + 200;
      sprite->frame = 11;
    }
    sprite->dest_y = sprite->y;
    break;

  case TOR:
    sprite->x_offset = 24; sprite->y_offset = 0;
    sprite->x_hot = 40; sprite->y_hot = 36;
    sprite->frame = 1;
    sprite->count = 200;
    break;

  case EXP:
    sprite->x_offset = 24; sprite->y_offset = 0;
    sprite->x_hot = 40; sprite->y_hot = 16;
    sprite->frame = 1;
    break;
  }
}

DestroyAllSprites()
{
  /* TIPC: called when a city is loaded or created.  The original only set frame = 0 and kept the records, so a city
     loaded after play started with a different sprite list order than a fresh start; now the whole sprite system goes
     back to its start state (empty list, empty pool, Cycle 0), which keeps in-game load == cold load with sprites on. */
  int i;
  for (i = 0; i < OBJN; i++) GlobalSprites[i] = NULL;
  sim->sprite = NULL;
  FreeSprites = NULL;
  PoolUsed = 0;
  Cycle = 0;
  absDist = 0;     /* the plane code reads the last GetDir distance before it is set in some branches (an original quirk) */
  return 0;
}

DestroySprite(SimSprite SPRNEAR *sprite)
{
  SimSprite SPRNEAR *p, SPRNEAR *prev = NULL;

  if (GlobalSprites[sprite->type] == sprite) {
    GlobalSprites[sprite->type] = (SimSprite SPRNEAR *)NULL;
  }

  for (p = sim->sprite; p != NULL; prev = p, p = p->next) {
    if (sprite == p) {
      if (prev) prev->next = p->next; else sim->sprite = p->next;
      break;
    }
  }

  sprite->next = FreeSprites;
  FreeSprites = sprite;
}

SimSprite SPRNEAR *
GetSprite(int type)
{
  SimSprite SPRNEAR *sprite;

  if (((sprite = GlobalSprites[type]) == NULL) ||
      (sprite->frame == 0))
    return (SimSprite SPRNEAR *)NULL;
  else
    return sprite;
}

SimSprite SPRNEAR *
MakeSprite(int type, int x, int y)
{
  SimSprite SPRNEAR *sprite;

  if (!SpriteAllowed(type)) return NULL;
  if ((sprite = GlobalSprites[type]) == NULL) {
    sprite = NewSprite(type, x, y);
  } else {
    InitSprite(sprite, x, y);
  }
  return sprite;
}

SimSprite SPRNEAR *
MakeNewSprite(int type, int x, int y)
{
  return NewSprite(type, x, y);
}

short GetChar(int x, int y)
{
  x >>= 4;
  y >>= 4;
  if (!TestBounds(x, y))
    return(-1);
  else
    return(Map[x][y] & LOMASK);
}


short TurnTo(int p, int d)
{
  if (p == d) return(p);
  if (p < d)
    if ((d - p) < 4) p++;
    else p--;
  else
    if ((p - d) < 4) p--;
    else p++;
  if (p > 8) p = 1;
  if (p < 1) p = 8;
  return(p);
}


TryOther(int Tpoo, int Told, int Tnew)
{
  register short z;

  z = Told + 4;
  if (z > 8) z -= 8;
  if (Tnew != z) return(0);
  if ((Tpoo == POWERBASE) || (Tpoo == POWERBASE + 1) ||
      (Tpoo == RAILBASE) || (Tpoo == RAILBASE + 1))
    return(1);
  return(0);
}


short SpriteNotInBounds(SimSprite SPRNEAR *sprite)
{
  int x = sprite->x + sprite->x_hot;
  int y = sprite->y + sprite->y_hot;

  if ((x < 0) || (y < 0) ||
      (x >= (WORLD_X <<4)) ||
      (y >= (WORLD_Y <<4))) {
    return (1);
  }
  return (0);
}


short GetDir(int orgX, int orgY, int desX, int desY)
{
  static short Gdtab[13] = { 0, 3, 2, 1, 3, 4, 5, 7, 6, 5, 7, 8, 1 };
  int dispX, dispY, z;

  dispX = desX - orgX;
  dispY = desY - orgY;
  if (dispX < 0)
    if (dispY < 0) z = 11;
    else z = 8;
  else
    if (dispY < 0) z = 2;
    else z = 5;
  if (dispX < 0) dispX = -dispX;
  if (dispY < 0) dispY = -dispY;

  absDist = dispX + dispY;

  if ((dispX <<1) < dispY) z++;
  else if ((dispY <<1) < dispY) z--;

  if ((z < 0) || (z > 12)) z = 0;

  return (Gdtab[z]);
}


GetDis(int x1, int y1, int x2, int y2)
{
  register short dispX, dispY;
	
  if (x1 > x2) dispX = x1 - x2;
  else dispX = x2 - x1;
  if (y1 > y2) dispY = y1 - y2;
  else dispY = y2 - y1;

  return (dispX + dispY);
}


int CheckSpriteCollision(SimSprite SPRNEAR *s1, SimSprite SPRNEAR *s2)
{
  if ((s1->frame != 0) && (s2->frame != 0) &&
      GetDis(s1->x + s1->x_hot, s1->y + s1->y_hot,
	     s2->x + s2->x_hot, s2->y + s2->y_hot) < 30)
    return(1);
  return(0);
}



MoveObjects()
{
  SimSprite SPRNEAR *sprite;

  if (!SimSpeed) return;
  if (sim->sprite == NULL) return;   /* TIPC: nothing to move (all switched off): no cost */
  Cycle++;

  for (sprite = sim->sprite; sprite != NULL;) {
    if (sprite->frame) {
      switch (sprite->type) {
      case TRA: DoTrainSprite(sprite); break;
      case COP: DoCopterSprite(sprite); break;
      case AIR: DoAirplaneSprite(sprite); break;
      case SHI: DoShipSprite(sprite); break;
      case GOD: DoMonsterSprite(sprite); break;
      case TOR: DoTornadoSprite(sprite); break;
      case EXP: DoExplosionSprite(sprite); break;
      }
      sprite = sprite->next;
    } else {
      SimSprite SPRNEAR *s = sprite;
      sprite = sprite->next;
      DestroySprite(s);
    }
  }
}


DoTrainSprite(SimSprite SPRNEAR *sprite)
{
  static short Cx[4] = {   0,  16,   0, -16 };
  static short Cy[4] = { -16,   0,  16,   0 };
  static short Dx[5] = {   0,   4,   0,  -4,   0 };
  static short Dy[5] = {  -4,   0,   4,   0,   0 };
  static short TrainPic2[5] = { 1, 2, 1, 2, 5 };
  register short z, dir, dir2;
  short c;

  if ((sprite->frame == 3) || (sprite->frame == 4))
    sprite->frame = TrainPic2[sprite->dir];
  sprite->x += Dx[sprite->dir];
  sprite->y += Dy[sprite->dir];
  if (!(Cycle & 3)) {
    dir = Rand16() & 3;
    for (z = dir; z < (dir + 4); z++) {
      dir2 = z & 3;
      if (sprite->dir != 4) {
	if (dir2 == ((sprite->dir + 2) & 3)) continue;
      }
      c = GetChar(sprite->x + Cx[dir2] + 48,
		  sprite->y + Cy[dir2]);
      if (((c >= RAILBASE) && (c <= LASTRAIL)) || /* track? */
	  (c == RAILVPOWERH) ||
	  (c == RAILHPOWERV)) {
	if ((sprite->dir != dir2) &&
	    (sprite->dir != 4)) {
	  if ((sprite->dir + dir2) == 3)
	    sprite->frame = 3;
	  else
	    sprite->frame = 4;
	} else
	  sprite->frame = TrainPic2[dir2];

	if ((c == RAILBASE) || (c == (RAILBASE + 1)))
	  sprite->frame = 5;
	sprite->dir = dir2;
	return;
      }
    }
    if (sprite->dir == 4) {
      sprite->frame = 0;
      return;
    }
    sprite->dir = 4;
  }
}


DoCopterSprite(SimSprite SPRNEAR *sprite)
{
  static short CDx[9] = { 0,  0,  3,  5,  3,  0, -3, -5, -3 };
  static short CDy[9] = { 0, -5, -3,  0,  3,  5,  3,  0, -3 };
  register short z, d, x, y;

  if (sprite->sound_count > 0) sprite->sound_count--;

  if (sprite->control < 0) {

    if (sprite->count > 0) sprite->count--;

    if (!sprite->count) {
      /* Attract copter to monster and tornado so it blows up more often */
      SimSprite SPRNEAR *s = GetSprite(GOD);
      if (s != NULL) {
	sprite->dest_x = s->x;
	sprite->dest_y = s->y;
      } else {
	s = GetSprite(TOR);
	if (s != NULL) {
	  sprite->dest_x = s->x;
	  sprite->dest_y = s->y;
	} else {
	  sprite->dest_x = sprite->orig_x;
	  sprite->dest_y = sprite->orig_y;
	}
      }
    }
    if (!sprite->count) { /* land */
      GetDir(sprite->x, sprite->y, sprite->orig_x, sprite->orig_y);
      if (absDist < 30) {
	sprite->frame = 0;
	return;
      }
    }
  } else {
    GetDir(sprite->x, sprite->y, sprite->dest_x, sprite->dest_y);
    if (absDist < 16) {
      sprite->dest_x = sprite->orig_x;
      sprite->dest_y = sprite->orig_y;
      sprite->control = -1;
    }
  }

  if (!sprite->sound_count) { /* send report  */
    x = (sprite->x + 48) >>5;
    y = sprite->y >>5;
    if ((x >= 0) &&
	(x < (WORLD_X >>1)) &&
	(y >= 0) &&
	(y < (WORLD_Y >>1))) {
      /* Don changed from 160 to 170 to shut the #$%#$% thing up! */
      if ((TrfDensity[x][y] > 170) && ((Rand16() & 7) == 0)) {
	SendMesAt(-41, (x <<1) + 1, (y <<1) + 1);
	MakeSound("city", "HeavyTraffic"); /* chopper */
	sprite->sound_count = 200;
      }
    }
  }
  z = sprite->frame;
  if (!(Cycle & 3)) {
    d = GetDir(sprite->x, sprite->y, sprite->dest_x, sprite->dest_y);
    z = TurnTo(z, d);
    sprite->frame = z;
  }

  sprite->x += CDx[z];
  sprite->y += CDy[z];
}


DoAirplaneSprite(SimSprite SPRNEAR *sprite)
{
  static short CDx[12] = { 0,  0,  6,  8,  6,  0, -6, -8, -6,  8,  8,  8 };
  static short CDy[12] = { 0, -8, -6,  0,  6,  8,  6,  0, -6,  0,  0,  0 };

  register short z, d;

  z = sprite->frame;
	
  if (!(Cycle % 5)) {
    if (z > 8) { /* TakeOff  */
      z--;
      if (z < 9) z = 3;
      sprite->frame = z;
    } else { /* goto destination */
      d = GetDir(sprite->x, sprite->y, sprite->dest_x, sprite->dest_y);
      z = TurnTo(z, d);
      sprite->frame = z;
    }
  }	

  if (absDist < 50) { /* at destination  */
    sprite->dest_x = Rand((WORLD_X * 16) + 100) - 50;
    sprite->dest_y = Rand((WORLD_Y * 16) + 100) - 50;
  }

  /* deh added test for !Disasters */
  if (!NoDisasters) {
    SimSprite SPRNEAR *s;
    int explode = 0;

    for (s = sim->sprite; s != NULL; s = s->next) {
      if ((s->frame != 0) &&
	  ((s->type == COP) ||
	   ((sprite != s) &&
	    (s->type == AIR))) &&
	  CheckSpriteCollision(sprite, s)) {
	ExplodeSprite(s);
	explode = 1;
      }
    }
    if (explode)
      ExplodeSprite(sprite);
  }

  sprite->x += CDx[z];
  sprite->y += CDy[z];
  if (SpriteNotInBounds(sprite)) sprite->frame = 0;
}


DoShipSprite(SimSprite SPRNEAR *sprite)
{
  static short BDx[9] = { 0,  0,  1,  1,  1,  0, -1, -1, -1 };
  static short BDy[9] = { 0, -1, -1,  0,  1,  1,  1,  0, -1 };
  static short BPx[9] = { 0,  0,  2,  2,  2,  0, -2, -2, -2 };
  static short BPy[9] = { 0, -2, -2,  0,  2,  2,  2,  0, -2 };
  static short BtClrTab[8] = { RIVER, CHANNEL, POWERBASE, POWERBASE + 1,
			       RAILBASE, RAILBASE + 1, BRWH, BRWV };
  register short x, y, z, t = RIVER;
  short tem, pem;

  if (sprite->sound_count > 0) sprite->sound_count--;
  if (!sprite->sound_count) {
    if ((Rand16() & 3) == 1) {
      if ((ScenarioID == 2) && /* San Francisco */
	  (Rand(10) < 5)) {
	MakeSound("city", "HonkHonk-Low -speed 80");
      } else {
	MakeSound("city", "HonkHonk-Low");
      }
    }
    sprite->sound_count = 200;
  }

  if (sprite->count > 0) sprite->count--;
  if (!sprite->count) {
    sprite->count = 9;
    if (sprite->frame != sprite->new_dir) {
      sprite->frame = TurnTo(sprite->frame, sprite->new_dir);
      return;
    }
    tem = Rand16() & 7;
    for (pem = tem; pem < (tem + 8); pem++) {
      z = (pem & 7) + 1;

      if (z == sprite->dir) continue;
      x = ((sprite->x + (48 - 1)) >>4) + BDx[z];
      y = (sprite->y >>4) + BDy[z];
      if (TestBounds(x, y)) {
	t = Map[x][y] & LOMASK;
	if ((t == CHANNEL) || (t == BRWH) || (t == BRWV) ||
	    TryOther(t, sprite->dir, z)) {
	  sprite->new_dir = z;
	  sprite->frame = TurnTo(sprite->frame, sprite->new_dir);
	  sprite->dir = z + 4;
	  if (sprite->dir > 8) sprite->dir -= 8;
	  break;
	}
      }
    }
    if (pem == (tem + 8)) {
      sprite->dir = 10;
      sprite->new_dir = (Rand16() & 7) + 1;
    }
  } else {
    z = sprite->frame;
    if (z == sprite->new_dir)  {
      sprite->x += BPx[z];
      sprite->y += BPy[z];
    }
  }
  if (SpriteNotInBounds(sprite)) {
    sprite->frame = 0;
    return;
  }
  if (!NoDisasters) {
    for (z = 0; z < 8; z++) {
      if (t == BtClrTab[z]) break;
      if (z == 7) {
	ExplodeSprite(sprite);
	Destroy(sprite->x + 48, sprite->y);
      }
    }
  }
}


MakeAirCrash(void)
{
#ifndef NO_AIRCRASH
  if (GetSprite(AIR) == NULL) {
    short x, y;

    x = Rand(WORLD_X - 20) + 10;
    y = Rand(WORLD_Y - 10) + 5;

    GeneratePlane(x, y);
  }

  if (GetSprite(AIR) != NULL)   /* TIPC: the plane may not exist (sprites off) */
    ExplodeSprite(GetSprite(AIR));
#endif
}


DoMonsterSprite(SimSprite SPRNEAR *sprite)
{
  static short Gx[5] = {  2,  2, -2, -2,  0 };
  static short Gy[5] = { -2,  2,  2, -2,  0 };
  static short ND1[4] = {  0,  1,  2,  3 };
  static short ND2[4] = {  1,  2,  3,  0 };
  static short nn1[4] = {  2,  5,  8, 11 };
  static short nn2[4] = { 11,  2,  5,  8 };
  register short d, z, c;

  if (sprite->sound_count > 0) sprite->sound_count--;

  if (sprite->control < 0) {
    /* business as usual */

    if (sprite->control == -2) {
      d = (sprite->frame - 1) / 3;
      z = (sprite->frame - 1) % 3;
      if (z == 2) sprite->step = 0;
      if (z == 0) sprite->step = 1;
      if (sprite->step) z++;
      else z--;
      c = GetDir(sprite->x, sprite->y, sprite->dest_x, sprite->dest_y);
      if (absDist < 18) {
	sprite->control = -1;
	sprite->count = 1000;
	sprite->flag = 1;
	sprite->dest_x = sprite->orig_x;
	sprite->dest_y = sprite->orig_y;
      } else {
	c = (c - 1) / 2;
	if (((c != d) && (!Rand(5))) ||
	    (!Rand(20))) {
	  int diff = (c - d) & 3;
	  if ((diff == 1) || (diff == 3)) {
	    d = c;
	  } else {
	    if (Rand16() & 1) d++; else d--;
	    d &= 3;
	  }
	} else {
	  if (!Rand(20)) {
	    if (Rand16() & 1) d++; else d--;
	    d &= 3;
	  }
	}
      }
    } else {

      d = (sprite->frame - 1) / 3;

      if (d < 4) { /* turn n s e w */
	z = (sprite->frame - 1) % 3;
	if (z == 2) sprite->step = 0;
	if (z == 0) sprite->step = 1;
	if (sprite->step) z++;
	else z--;
	GetDir(sprite->x, sprite->y, sprite->dest_x, sprite->dest_y);
	if (absDist < 60) {
	  if (sprite->flag == 0) {
	    sprite->flag = 1;
	    sprite->dest_x = sprite->orig_x;
	    sprite->dest_y = sprite->orig_y;
	  } else {
	    sprite->frame = 0;
	    return;
	  }
	}
	c = GetDir(sprite->x, sprite->y, sprite->dest_x, sprite->dest_y);
	c = (c - 1) / 2;
	if ((c != d) && (!Rand(10))) {
	  if (Rand16() & 1) z = ND1[d];
	  else z = ND2[d];
	  d = 4;
	  if (!sprite->sound_count) {
	    MakeSound("city", "Monster -speed [MonsterSpeed]"); /* monster */
	    sprite->sound_count = 50 + Rand(100);
	  }
	}
      } else {
	d = 4;
	c = sprite->frame;
	z = (c - 13) & 3;
	if (!(Rand16() & 3)) {
	  if (Rand16() & 1) z = nn1[z];
	  else z = nn2[z];
	  d = (z - 1) / 3;
	  z = (z - 1) % 3;
	}
      }
    }
  } else {
    /* somebody's taken control of the monster */

    d = sprite->control;
    z = (sprite->frame - 1) % 3;

    if (z == 2) sprite->step = 0;
    if (z == 0) sprite->step = 1;
    if (sprite->step) z++;
    else z--;
  }

  z = (((d * 3) + z) + 1);
  if (z > 16) z = 16;
  sprite->frame = z;

  sprite->x += Gx[d];
  sprite->y += Gy[d];

  if (sprite->count > 0) sprite->count--;
  c = GetChar(sprite->x + sprite->x_hot, sprite->y + sprite->y_hot);
  if ((c == -1) ||
      ((c == RIVER) &&
       (sprite->count != 0) &&
       (sprite->control == -1))) {
    sprite->frame = 0; /* kill zilla */
  }

  { SimSprite SPRNEAR *s;
    for (s = sim->sprite; s != NULL; s = s->next) {
      if ((s->frame != 0) &&
	  ((s->type == AIR) ||
	   (s->type == COP) ||
	   (s->type == SHI) ||
	   (s->type == TRA)) &&
	  CheckSpriteCollision(sprite, s)) {
	  ExplodeSprite(s);
	}
      }
    }

  Destroy(sprite->x + 48, sprite->y + 16);
}


DoTornadoSprite(SimSprite SPRNEAR *sprite)
{
  static short CDx[9] = {  2,  3,  2,  0, -2, -3 };
  static short CDy[9] = { -2,  0,  2,  3,  2,  0 };
  register short z;

  z = sprite->frame;

  if (z == 2) /* cycle animation... post Rel */
    if (sprite->flag)
      z = 3;
    else
      z = 1;
  else {
    if (z == 1)
      sprite->flag = 1;
    else
      sprite->flag = 0;
    z = 2;
  }

  if (sprite->count > 0) sprite->count--;

  sprite->frame = z;

  { SimSprite SPRNEAR *s;
    for (s = sim->sprite; s != NULL; s = s->next) {
      if ((s->frame != 0) &&
	  ((s->type == AIR) ||
	   (s->type == COP) ||
	   (s->type == SHI) ||
	   (s->type == TRA)) &&
	  CheckSpriteCollision(sprite, s)) {
	  ExplodeSprite(s);
	}
      }
    }

  z = Rand(5);
  sprite->x += CDx[z];
  sprite->y += CDy[z];
  if (SpriteNotInBounds(sprite)) sprite->frame = 0;

  if ((sprite->count != 0) &&
      (!Rand(500)))
    sprite->frame = 0;

  Destroy(sprite->x + 48, sprite->y + 40);
}


DoExplosionSprite(SimSprite SPRNEAR *sprite)
{
  short x, y;

  if (!(Cycle & 1)) {
    if (sprite->frame == 1) {
      MakeSound("city", "Explosion-High"); /* explosion */
      x = (sprite->x >>4) + 3;
      y = (sprite->y >>4);
      SendMesAt(32, x, y);
    }
    sprite->frame++;
  }

  if (sprite->frame > 6) {
    sprite->frame = 0;

    StartFire(sprite->x + 48 - 8, sprite->y + 16);
    StartFire(sprite->x + 48 - 24, sprite->y);
    StartFire(sprite->x + 48 + 8, sprite->y);
    StartFire(sprite->x + 48 - 24, sprite->y + 32);
    StartFire(sprite->x + 48 + 8, sprite->y + 32);
    return;
  }
}

ExplodeSprite(SimSprite SPRNEAR *sprite)
{
  int x, y;

  sprite->frame = 0;

  x = sprite->x + sprite->x_hot;
  y = sprite->y + sprite->y_hot;
  MakeExplosionAt(x, y);

  x = (x >>4);
  y = (y >>4);

  switch (sprite->type) {
  case AIR:
    CrashX = x;
    CrashY = y;
    SendMesAt(-24, x, y);
    break;
  case SHI:
    CrashX = x;
    CrashY = y;
    SendMesAt(-25, x, y);
    break;
  case TRA:
    CrashX = x;
    CrashY = y;
    SendMesAt(-26, x, y);
    break;
  case COP:
    CrashX = x;
    CrashY = y;
    SendMesAt(-27, x, y);
    break;
  case BUS:
    CrashX = x;
    CrashY = y;
    SendMesAt(-26, x, y); /* XXX for now */
    break;
  }
  MakeSound("city", "Explosion-High"); /* explosion */
  return;
}


int checkWet(int x)
{
  if ((x == POWERBASE) || (x == POWERBASE + 1) ||
      (x == RAILBASE) || (x == RAILBASE + 1) ||
      (x == BRWH) || (x == BRWV))
    return(1);
  else
    return(0);
}


Destroy(int ox, int oy)
{
  short t, z, x, y;
	
  x = ox >>4;
  y = oy >>4;
  if (!TestBounds(x, y))
    return;
  z = Map[x][y];
  t = z & LOMASK;
  if (t >= TREEBASE) {
    /* TILE_IS_BRIDGE(t) */
    if (!(z & BURNBIT)) {		
      if ((t >= ROADBASE) && (t <= LASTROAD))
	Map[x][y] = RIVER;
      return;
    }
    if (z & ZONEBIT) {
      OFireZone(x, y, z);
      if (t > RZB) {
	MakeExplosionAt(ox, oy);
      }
    }
    if (checkWet(t))
      Map[x][y] = RIVER;
    else
      Map[x][y] = (DoAnimation
		   ? TINYEXP
		   : (LASTTINYEXP - 3)) | BULLBIT | ANIMBIT;
  }
}


OFireZone(int Xloc, int Yloc, int ch)
{
  register short Xtem, Ytem;
  short x, y, XYmax;

  RateOGMem[Xloc >>3][Yloc >>3] -= 20;

  ch &= LOMASK;
  if (ch < PORTBASE)
    XYmax = 2;
  else
    if (ch == AIRPORT) XYmax = 5;
    else XYmax = 4;

  for (x = -1; x < XYmax; x++)
    for (y = -1; y < XYmax; y++) {
      Xtem = Xloc + x;
      Ytem = Yloc + y;
      if ((Map[Xtem][Ytem] & LOMASK) >= ROADBASE)
	Map[Xtem][Ytem] |= BULLBIT;
    }
}


StartFire(int x, int y)
{
  register t, z;

  x >>= 4;
  y >>= 4;
  if ((x >= WORLD_X) ||
      (y >= WORLD_Y) ||
      (x < 0) || (y < 0))
    return;
  z = Map[x][y];
  t = z & LOMASK;
  if ((!(z & BURNBIT)) && (t != 0)) return;
  if (z & ZONEBIT) return;
  Map[x][y] = FIRE + (Rand16() & 3) + ANIMBIT;
}


GenerateTrain(int x, int y)
{
  if (!SpriteAllowed(TRA)) return;   /* TIPC: before any random draw, so sprites off == no sprite code */
  if ((TotalPop > 20) &&
      (GetSprite(TRA) == NULL) &&
      (!Rand(25))) {
    MakeSprite(TRA, (x <<4) + TRA_GROOVE_X, (y <<4) + TRA_GROOVE_Y);
  }
}

GenerateShip(void)
{
  register short x, y;

  if (!SpriteAllowed(SHI)) return;   /* TIPC */

  if (!(Rand16() & 3))
    for (x = 4; x < WORLD_X - 2; x++)
      if (Map[x][0] == CHANNEL)  {
	MakeShipHere(x, 0);
	return;
      }
  if (!(Rand16() & 3))
    for (y = 1; y < WORLD_Y - 2; y++)
      if (Map[0][y] == CHANNEL)  {
	MakeShipHere(0, y);
	return;
      }
  if (!(Rand16() & 3))
    for (x = 4; x < WORLD_X - 2; x++)			
      if (Map[x][WORLD_Y - 1] == CHANNEL)  {
	MakeShipHere(x, WORLD_Y - 1);
	return;
      }
  if (!(Rand16() & 3))
    for (y = 1; y < WORLD_Y - 2; y++)
      if (Map[WORLD_X - 1][y] == CHANNEL)  {
	MakeShipHere(WORLD_X - 1, y);
	return;
      }
}


MakeShipHere(int x, int y)
{
  MakeSprite(SHI, (x <<4) - (48 - 1), (y <<4));
}


MakeMonster(void)
{
  register x, y, z, done = 0;
  SimSprite SPRNEAR *sprite;

  if ((sprite = GetSprite(GOD)) != NULL) {
    sprite->sound_count = 1;
    sprite->count = 1000;
    sprite->dest_x = PolMaxX <<4;
    sprite->dest_y = PolMaxY <<4;
    return;
  }

  for (z = 0; z < 300; z++)  {
    x = Rand(WORLD_X - 20) + 10;
    y = Rand(WORLD_Y - 10) + 5;
    if ((Map[x][y] == RIVER) || (Map[x][y] == RIVER + BULLBIT)) {
      MonsterHere(x, y);
      done = 1;
      break;
    }
  }
  if (!done == 0)
    MonsterHere(60, 50);
}


MonsterHere(int x, int y)
{
  short z;

  if (MakeSprite(GOD, (x <<4) + 48, (y <<4)) == NULL) return;   /* TIPC: no art or no memory */
  ClearMes();
  SendMesAt(-21, x + 5, y);
}


GenerateCopter(int x, int y)
{
  if (!SpriteAllowed(COP)) return;   /* TIPC */
  if (GetSprite(COP) != NULL) return;

  MakeSprite(COP, (x <<4), (y <<4) + 30);
}


GeneratePlane(int x, int y)
{
  if (!SpriteAllowed(AIR)) return;   /* TIPC */
  if (GetSprite(AIR) != NULL) return;

  MakeSprite(AIR, (x <<4) + 48, (y <<4) + 12);
}


MakeTornado(void)
{
  short x, y;
  SimSprite SPRNEAR *sprite;

  if ((sprite = GetSprite(TOR)) != NULL) {
    sprite->count = 200;
    return;
  }

  x = Rand((WORLD_X <<4) - 800) + 400;
  y = Rand((WORLD_Y <<4) - 200) + 100;
  if (MakeSprite(TOR, x, y) == NULL) return;   /* TIPC: no art or no memory */
  ClearMes();
  SendMesAt(-22, (x >>4) + 3, (y >>4) + 2);
}


MakeExplosion(int x, int y)
{
  if ((x >= 0) && (x < WORLD_X) &&
      (y >= 0) && (y < WORLD_Y)) {
    MakeExplosionAt((x << 4) + 8, (y << 4) + 8);
  }
}


MakeExplosionAt(int x, int y)
{
  MakeNewSprite(EXP, x - 40, y - 16);
}

