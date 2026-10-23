/* botmap.c: Linux-only helper. Generates a terrain for a seed and prints an ASCII map. */
#include "sim.h"
int initMapArrays(void); int GenerateSomeCity(int r);
int main(int argc, char **argv)
{
  int x, y, seed = atoi(argv[1]);
  sim_srand(1); initMapArrays(); autoBudget = 1; autoBulldoze = 1; NoDisasters = 1;
  GenerateSomeCity(seed);
  for (y = 0; y < WORLD_Y; y++) { for (x = 0; x < WORLD_X; x++) { int t = Map[x][y] & LOMASK;
    putchar(t == 0 ? '.' : (t >= 2 && t <= 20) ? '~' : (t >= 21 && t <= 43) ? 't' : '#'); } putchar('\n'); }
  return 0;
}
