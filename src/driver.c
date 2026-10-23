/* driver.c: headless deterministic test driver for Micropolis for TIPC.
 * Usage: SIMTEST city.cty years [seed]
 * Loads a city, runs the simulation at full speed with auto-budget and no
 * disasters, and prints one census line per game year.
 * Code by Shurr Nuff (AI assistant), produced by Will (stovebolt), Flying Horse Labs. GPL v3. */
#include "sim.h"

int SimFrame(void);
int initMapArrays(void);
int InitWillStuff(void);
extern short SimSpeed;

unsigned long map_sum(void)
{
  unsigned long s = 0;
  int x, y;
  for (x = 0; x < WORLD_X; x++)
    for (y = 0; y < WORLD_Y; y++)
      s = (((s << 3) | (s >> 29)) & 0xFFFFFFFFUL) ^ (unsigned short)Map[x][y];
  return s & 0xFFFFFFFFUL;
}

static void report(void)
{
  printf("Y%ld T%ld F%ld P%ld R%d C%d I%d S%d K%d cr%d po%d lv%d tr%d M%08lx\n",
         (long)(CityTime / 48 + StartingYear), (long)CityTime, (long)TotalFunds,
         (long)CityPop, ResHis[0], ComHis[0], IndHis[0], CityScore, CityClass,
         CrimeAverage, PolluteAverage, LVAverage, TrafficAverage, map_sum());
}

int simdbg = 0;
/* Timing marker for the MAME Lua script: it finds the signature in RAM and
   logs emulated time whenever ct changes. */
volatile struct { char sig[8]; long ct; short phase; } tmark = { {'M','P','T','M','A','R','K','!'}, -1L, 0 };
#ifdef __WATCOMC__
#include <dos.h>
static unsigned dos_free_paras(void) { unsigned seg = 0, r; r = _dos_allocmem(0xFFFF, &seg); return seg; }
#endif
int main(int argc, char **argv)
{
  int years;
  QUAD last;
  if (argc < 3) { printf("usage: simtest city.cty years [seed]\n"); return 1; }
  years = atoi(argv[2]);
  if (argc > 4) simdbg = atoi(argv[4]);
  sim_srand(argc > 3 ? (unsigned)atoi(argv[3]) : 1);
#ifdef __WATCOMC__
  printf("MEM psp=%04x top=%04x freeparas_start=%u\n", _psp, *(unsigned far *)MK_FP(_psp, 2), dos_free_paras());
#endif
  initMapArrays();
  if (!loadFile(argv[1])) { printf("cannot load %s\n", argv[1]); return 2; }
#ifdef __WATCOMC__
  printf("MEM freeparas_loaded=%u\n", dos_free_paras());
#endif
  StartingYear = 1900;
  if (simdbg == 2) { FILE *f = fopen("MAP.BIN", "wb"); int x; for (x = 0; x < WORLD_X; x++) fwrite(Map[x], 2, WORLD_Y, f); fclose(f); }
  autoBudget = 1; autoBulldoze = 1; SetDisasters(0); setSpeed(3);
  /* argv[6]: 'S' = sprites on (trains, ships, planes, helicopters), 'D' = random disasters on as well */
  if (argc > 6) { char *o = argv[6]; for (; *o; o++) { if (*o == 'S') SpritesOn = 1; if (*o == 'D') SetDisasters(1); } }
  report();
  tmark.phase = 1; tmark.ct = CityTime;
  last = CityTime / 48 + years;
  while (CityTime / 48 < last) {
    QUAD before = CityTime;
    SimFrame();
    MoveObjects();   /* the original calls MoveObjects once per simulation loop */
    tmark.ct = CityTime;
    if (CityTime != before && CityTime % 48 == 0) report();
    else if (simdbg == 3 && CityTime != before) printf("t%ld M%08lx v%d,%d,%d re%d pe%d fe%d F%ld\n", (long)CityTime, map_sum(), RValve, CValve, IValve, RoadEffect, PoliceEffect, FireEffect, (long)TotalFunds);
  }
  tmark.phase = 2;
  if (SpritesOn || DisasterSprites)
    printf("SPRITES tra%u cop%u air%u shi%u god%u tor%u exp%u\n", SpriteMade[TRA], SpriteMade[COP], SpriteMade[AIR], SpriteMade[SHI], SpriteMade[GOD], SpriteMade[TOR], SpriteMade[EXP]);
  if (argc > 5 && argv[5][0] != '-') { setSpeed(0); printf("SAVE %s %s\n", argv[5], saveFile(argv[5]) ? "ok" : "FAILED"); }
  return 0;
}
