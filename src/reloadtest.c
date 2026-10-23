/* reloadtest.c: Linux-only test for the in-game load (compare cold load against load-after-play).
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

extern void ResetForLoad(void);
extern char __data_start, _end;
static void dump(const char *fn) { FILE *f = fopen(fn, "wb"); fwrite(&__data_start, 1, &_end - &__data_start, f); fclose(f); }
static char *snap;
static void restore_ranges(void)       /* oracle for finding stale globals: RANGES=file with "offset size" lines (offsets from __data_start) */
{
  FILE *f; unsigned long off, sz;
  if (!getenv("RANGES") || !(f = fopen(getenv("RANGES"), "r"))) return;
  while (fscanf(f, "%lu %lu", &off, &sz) == 2) memcpy(&__data_start + off, snap + off, sz);
  fclose(f);
}
static void run_years(int years, int quiet)
{
  QUAD last = CityTime / 48 + years;
  autoBudget = 1; autoBulldoze = 1; SetDisasters(getenv("DISASTERS") ? 1 : 0); setSpeed(3);
  SpritesOn = getenv("SPRITES") ? 1 : 0;   /* SPRITES=1: sprites move during the run */
  while (CityTime / 48 < last) {
    QUAD before = CityTime;
    SimFrame();
    MoveObjects();
    if (!quiet && CityTime != before && CityTime % 48 == 0) report();
  }
}
/* reloadtest cold|warm|warm2 A.cty B.cty yearsA yearsB dumpfile */
int main(int argc, char **argv)
{
  int warm = strcmp(argv[1], "cold") != 0;
  sim_srand(1);
  initMapArrays();
  snap = malloc(&_end - &__data_start); memcpy(snap, &__data_start, &_end - &__data_start);
  if (!warm) { char n[200]; sprintf(n, "%s.pre", argv[6]); dump(n); }
  if (warm) {
    if (!loadFile(argv[2])) return 2;
    StartingYear = 1900;
    run_years(atoi(argv[4]), 1);
    ResetForLoad();                 /* what an in-game load does before loadFile() */
    restore_ranges();
    { char n[200]; sprintf(n, "%s.pre", argv[6]); dump(n); }
  }
  if (!loadFile(argv[3])) return 2;
  StartingYear = 1900;
  dump(argv[6]);
  report();
  run_years(atoi(argv[5]), 0);
  return 0;
}
