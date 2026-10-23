/* botrun.c: Linux-only helper. Reads a plan (stdin) and plays it through the same tool code the
 * TIPC front end uses, running the sim between steps. Lines: "n SEED", "t KEY X Y" (select tool, build at X,Y),
 * "w MONTHS" (run the sim), "d" (dump census). Prints the population at each wait. */
#include "sim.h"
int initMapArrays(void); int GenerateSomeCity(int r); int SimFrame(void); int do_tool_xy(short state, short x, short y);
extern short SimSpeed;
static const struct { char key; short state; } tools[] = {
  {'Q',queryState},{'B',dozeState},{'R',roadState},{'T',rrState},{'W',wireState},{'K',parkState},{'1',residentialState},
  {'2',commercialState},{'3',industrialState},{'P',policeState},{'F',fireState},{'S',stadiumState},{'C',powerState},
  {'N',nuclearState},{'E',seaportState},{'A',airportState}};
int main(int argc, char **argv)
{
  char line[200]; int ln = 0;
  sim_srand(1); initMapArrays();
  while (fgets(line, sizeof line, stdin)) {
    char c; int a, b, i; ln++;
    if (line[0] == 'n') { sscanf(line + 1, "%d", &a);
      GenerateSomeCity(a); StartingYear = 1900; GameLevel = 0; SetFunds(20000L); CityTax = 7;
      autoBudget = 1; autoBulldoze = 1; NoDisasters = 1; setSpeed(3); }
    else if (line[0] == 't') { short st = -1; sscanf(line + 1, " %c %d %d", &c, &a, &b);
      for (i = 0; i < 16; i++) if (tools[i].key == c) st = tools[i].state;
      i = do_tool_xy(st, a, b);
      if (i != 1) printf("line %d: tool %c at %d,%d -> %d\n", ln, c, a, b, i); }
    else if (line[0] == 'w') { long t0 = CityTime; sscanf(line + 1, "%d", &a);
      while (CityTime < t0 + 4L * a) SimFrame();
      printf("%d %d  funds %ld pop %ld  R%d C%d I%d  score %d\n", (int)(CityTime / 48 + 1900), (int)((CityTime % 48) / 4) + 1,
             (long)TotalFunds, (long)CityPop, RValve, CValve, IValve, CityScore); }
  }
  return 0;
}
