/* glue.c: headless replacements for the X11/Tk front end (w_*.c).
 * Micropolis for TIPC. Derived from Micropolis (GPL v3), w_stubs.c, w_budget.c,
 * w_graph.c, w_update.c, w_util.c and rand.c. Code by Shurr Nuff (AI assistant),
 * produced by Will (stovebolt), Flying Horse Labs. GPL v3. */
#include "sim.h"

static Sim sim_struct; Sim *sim = &sim_struct; /* empty: no views, no sprites */
QUAD TotalFunds; short PunishCnt; short autoBulldoze, autoBudget; QUAD LastMesTime;
short GameLevel; short InitSimLoad; short ScenarioID; short SimSpeed; short SimMetaSpeed;
short UserSoundOn; char *CityName; short NoDisasters; short MesNum; short EvalChanged;
short flagBlink; short NewGraph; short Graph10Max, Graph120Max;
unsigned char *History10[HISTORIES], *History120[HISTORIES];
short MustUpdateFunds, MustUpdateOptions; QUAD LastCityTime, LastCityYear, LastCityMonth, LastFunds;
QUAD LastR, LastC, LastI; short CrashX, CrashY; short OverRide_s;
int OverRide, PendingTool, UpdateDelayed; int DoAnimation = 1;
QUAD fireValue, policeValue, roadValue;

/* rand.c */
#define SIM_RAND_MAX 0xffff
static unsigned QUAD next = 1;
unsigned long rngcount = 0;
/* TIPC speed pass: same LCG (next = next*1103515245 + 12345, mod 2^32) and the
   same output ((next % 2^24) >> 8 == bits 8..23), computed with 16x16 multiplies
   instead of the 32-bit library multiply and divide. */
#if defined(__WATCOMC__) && !defined(NOASM)
unsigned short rlo = 1, rhi = 0;	/* next, as two words */
QUAD sim_rand32(void)
{
  unsigned short r;
#ifdef SIMTRACE
  rngcount++;
#endif
  _asm {
    mov  ax, ss:rlo
    mov  bx, 4E6Dh
    mul  bx            ; dx:ax = lo * 0x4E6D
    mov  cx, ax
    mov  si, dx
    mov  ax, ss:rhi
    mul  bx            ; ax = low16(hi * 0x4E6D)
    add  si, ax
    mov  ax, ss:rlo
    mov  bx, 41C6h
    mul  bx            ; ax = low16(lo * 0x41C6)
    add  si, ax
    add  cx, 3039h     ; + 12345
    adc  si, 0
    mov  ss:rlo, cx
    mov  ss:rhi, si
    mov  al, ch        ; result = bits 8..23
    mov  bx, si
    mov  ah, bl
    mov  r, ax
  }
  return (QUAD)r;
}
#else
QUAD sim_rand32(void)
{
  unsigned short lo = (unsigned short)next, hi = (unsigned short)(next >> 16);
  unsigned long p = (unsigned long)lo * 0x4E6DU;
  unsigned short h = (unsigned short)(p >> 16)
                   + (unsigned short)(hi * 0x4E6DU) + (unsigned short)(lo * 0x41C6U);
  rngcount++;
  next = (((unsigned long)h << 16) | (unsigned short)p) + 12345UL;
  return (QUAD)(unsigned short)(next >> 8);
}
#endif
int sim_rand(void) { return (int)sim_rand32(); }
unsigned QUAD sim_entropy(void) { return 0; }
void sim_srand(unsigned int seed)
{
  next = seed;
#if defined(__WATCOMC__) && !defined(NOASM)
  rlo = seed; rhi = 0;
#endif
}

#ifdef TIPCUI
/* TIPC memory pass: the sim's map arrays come from one exact-size DOS block instead of the C heap
   (the heap rounds every 3000-byte map up to a 4K segment and wasted about 5K). */
#include <dos.h>
#include <string.h>
#define NEAR_BYTES 7200u     /* static: DGROUP is padded to 64K by the runtime anyway, so this costs nothing */
#define ARENA_BYTES 12500u     /* the rest, one exact DOS block */
static unsigned char near_arena[NEAR_BYTES];
unsigned arena_left = 0, near_left = NEAR_BYTES;
Ptr NewPtr(size) int size;
{
  static unsigned char far *arena; static int tried; static unsigned nused;
  size = (size + 1) & ~1;
  if ((unsigned)size <= near_left) {
    Ptr p = (Ptr)(near_arena + nused);
    nused += size; near_left -= size;
    return p;                                  /* BSS is zero */
  }
  if (!tried) {
    unsigned seg;
    tried = 1;
    if (!_dos_allocmem((ARENA_BYTES + 15) >> 4, &seg)) { arena = (unsigned char far *)MK_FP(seg, 0); arena_left = ARENA_BYTES; }
  }
  if (arena && (unsigned)size <= arena_left) {
    Ptr p = (Ptr)arena;
    _fmemset(p, 0, size);
    arena += size; arena_left -= size;
    return p;
  }
  return (Ptr)calloc(size, 1);
}
#else
Ptr NewPtr(size) int size; { return (Ptr)calloc(size, 1); }
#endif
QUAD TickCount(void) { return 0; }

int Spend(QUAD dollars) { SetFunds(TotalFunds - dollars); return 0; }
int SetFunds(QUAD dollars) { TotalFunds = dollars; return 0; }
int Eval(char *s) { return 0; }
int SetGameLevel(short level) { GameLevel = level; return 0; }
int SetGameLevelFunds(short level) { SetFunds(level == 1 ? 10000 : level == 2 ? 5000 : 20000); SetGameLevel(level); return 0; }
int DoNewGame(void) { return 0; }
int ChangeCensus(void) { CensusChanged = 1; return 0; }
int ChangeEval(void) { EvalChanged = 1; return 0; }
int doAllGraphs(void) { return 0; }
int drawCurrPercents(void) { return 0; }
int ResetLastKeys(void) { return 0; }
int DoUpdateHeads(void) { return 0; }
#ifdef TIPCUI
extern void snd_event(const char *b);
int MakeSound(char *a, char *b) { snd_event(b); return 0; }
#else
int MakeSound(char *a, char *b) { return 0; }
#endif
#ifndef TIPCUI
int GetIndString(char *str, int id, short num) { str[0] = 0; return 0; }
#endif
int InvalidateMaps(void) { return 0; }
int InvalidateEditors(void) { return 0; }
int UpdateFunds(void) { return 0; }
int setSpeed(short speed) { SimSpeed = speed; return 0; }
int setSkips(int s) { return 0; }
int setCityName(char *n) { return 0; }
int DropFireBombs(void) { return 0; }
int DoEarthQuake(void) { return 0; }
#ifdef NOSPRITES
/* MPTIPCL (the 256K colour build): no sprite code at all; the sim sees an empty sprite world. */
short SpritesOn = 0, DisasterSprites = 0; unsigned short SpriteArt = 0, SpriteMade[OBJN]; short Cycle; int absDist;
SimSprite SPRNEAR *GlobalSprites[OBJN];
SimSprite SPRNEAR *GetSprite(int type) { return NULL; }
int DestroyAllSprites(void) { return 0; }
int MoveObjects(void) { return 0; }
void SpritesSet(int on) { }
void SetDisasters(int on) { NoDisasters = on ? 0 : 1; }
int GenerateCopter(int x, int y) { return 0; }
int GeneratePlane(int x, int y) { return 0; }
int GenerateShip(void) { return 0; }
int GenerateTrain(int x, int y) { return 0; }
int MakeAirCrash(void) { return 0; }
int MakeExplosion(int x, int y) { return 0; }
int MakeExplosionAt(int x, int y) { return 0; }
int MakeMonster(void) { return 0; }
int MakeTornado(void) { return 0; }
#endif

int InitGraphMax(void)
{
  register int x;
  ResHisMax = ComHisMax = IndHisMax = 0;
  for (x = 118; x >= 0; x--) {
    if (ResHis[x] > ResHisMax) ResHisMax = ResHis[x];
    if (ComHis[x] > ComHisMax) ComHisMax = ComHis[x];
    if (IndHis[x] > IndHisMax) IndHisMax = IndHis[x];
    if (ResHis[x] < 0) ResHis[x] = 0;
    if (ComHis[x] < 0) ComHis[x] = 0;
    if (IndHis[x] < 0) IndHis[x] = 0;
  }
  Graph10Max = ResHisMax;
  if (ComHisMax > Graph10Max) Graph10Max = ComHisMax;
  if (IndHisMax > Graph10Max) Graph10Max = IndHisMax;
  Res2HisMax = Com2HisMax = Ind2HisMax = 0;
  for (x = 238; x >= 120; x--) {
    if (ResHis[x] > Res2HisMax) Res2HisMax = ResHis[x];
    if (ComHis[x] > Com2HisMax) Com2HisMax = ComHis[x];
    if (IndHis[x] > Ind2HisMax) Ind2HisMax = IndHis[x];
    if (ResHis[x] < 0) ResHis[x] = 0;
    if (ComHis[x] < 0) ComHis[x] = 0;
    if (IndHis[x] < 0) IndHis[x] = 0;
  }
  Graph120Max = Res2HisMax;
  if (Com2HisMax > Graph120Max) Graph120Max = Com2HisMax;
  if (Ind2HisMax > Graph120Max) Graph120Max = Ind2HisMax;
  return 0;
}

/* w_budget.c DoBudgetNow, autoBudget path only (headless). */
void (*BudgetHook)(void) = 0;   /* TIPC UI: opens the budget page when auto-budget is off */
int DoBudget(void)
{
  QUAD yumDuckets, total, moreDough, fireInt, policeInt, roadInt;
  if (!autoBudget && BudgetHook) BudgetHook();
  /* TIPC 16-bit fix: original cast to (int), which truncates to 16 bits on DOS. */
  fireInt = (QUAD)(((double)FireFund) * firePercent);
  policeInt = (QUAD)(((double)PoliceFund) * policePercent);
  roadInt = (QUAD)(((double)RoadFund) * roadPercent);
  total = fireInt + policeInt + roadInt;
  yumDuckets = TaxFund + TotalFunds;
  if (yumDuckets > total) {
    fireValue = fireInt; policeValue = policeInt; roadValue = roadInt;
  } else if (total > 0) {
    if (yumDuckets > roadInt) {
      roadValue = roadInt; yumDuckets -= roadInt;
      if (yumDuckets > fireInt) {
        fireValue = fireInt; yumDuckets -= fireInt;
        if (yumDuckets > policeInt) { policeValue = policeInt; yumDuckets -= policeInt; }
        else { policeValue = yumDuckets;
          policePercent = (yumDuckets > 0) ? ((double)yumDuckets) / ((double)PoliceFund) : 0.0; }
      } else {
        fireValue = yumDuckets; policeValue = 0; policePercent = 0.0;
        firePercent = (yumDuckets > 0) ? ((double)yumDuckets) / ((double)FireFund) : 0.0;
      }
    } else {
      roadValue = yumDuckets;
      roadPercent = (yumDuckets > 0) ? ((double)yumDuckets) / ((double)RoadFund) : 0.0;
      fireValue = 0; policeValue = 0; firePercent = 0.0; policePercent = 0.0;
    }
  } else {
    fireValue = policeValue = roadValue = 0;
    firePercent = policePercent = roadPercent = 1.0;
  }
  fireMaxValue = FireFund; policeMaxValue = PoliceFund; roadMaxValue = RoadFund;
  /* Headless: always pay what we can (auto budget). */
  moreDough = (QUAD)(TaxFund - (fireValue + policeValue + roadValue));
  Spend(-moreDough);
  FireSpend = fireValue; PoliceSpend = policeValue; RoadSpend = roadValue;
  return 0;
}

double roadPercent = 0.0, policePercent = 0.0, firePercent = 0.0;
QUAD roadMaxValue, policeMaxValue, fireMaxValue;
char *CityFileName = NULL; char *ResourceDir = NULL;
void InitFundingLevel(void)
{
  firePercent = 1.0; fireValue = 0;
  policePercent = 1.0; policeValue = 0;
  roadPercent = 1.0; roadValue = 0;
}
int Kick(void) { return 0; }
int setAnyCityName(char *n) { return 0; }

#ifdef TIPCUI
/* The Pentium FDIV-bug probe in the C library (3K of code) is pointless on an 8088. */
void __verify_pentium_fdiv_bug(void) {}
#endif

/* TIPC UI budget page helpers (floating point kept out of the UI module). which: 0 road, 1 fire, 2 police */
void budget_calc(QUAD *tax, QUAD *road, QUAD *fire, QUAD *pol)
{
  static const double RL[3] = { 0.7, 0.9, 1.2 }, FL[3] = { 1.4, 1.2, 0.8 };
  int g = (GameLevel >= 0 && GameLevel < 3) ? GameLevel : 0;
  *pol = PolicePop * 100;
  *fire = FireStPop * 100;
  *road = (QUAD)((RoadTotal + (RailTotal * 2)) * RL[g]);
  *tax = (QUAD)((((QUAD)TotalPop * LVAverage) / 120) * CityTax * FL[g]);
}
static double *fund_ptr(int which) { return which == 0 ? &roadPercent : which == 1 ? &firePercent : &policePercent; }
int fund_pct(int which) { return (int)(*fund_ptr(which) * 100.0 + 0.5); }
void fund_pct_set(int which, int pct) { *fund_ptr(which) = pct / 100.0; }
QUAD fund_amount(QUAD req, int which) { return (QUAD)(((double)req) * *fund_ptr(which)); }
