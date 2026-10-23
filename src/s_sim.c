/* s_sim.c
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


/* Simulation */


short ValveFlag;
short CrimeRamp, PolluteRamp ;
short RValve, CValve, IValve;
short ResCap, ComCap, IndCap;
short CashFlow;
double EMarket = 4.0;
short DisasterEvent;
short DisasterWait;
short ScoreType;
short ScoreWait;
short PwrdZCnt;
short unPwrdZCnt;
short NewPower; /* post */
short AvCityTax;
short Scycle = 0;
short Fcycle = 0;
short Spdcycle = 0;
short DoInitialEval = 0;
short MeltX, MeltY;


/* comefrom: doEditWindow scoreDoer doMapInFront graphDoer doNilEvent */
SimFrame(void)
{
  short i;

  if (SimSpeed == 0)
    return;

  if (++Spdcycle > 1023)
    Spdcycle = 0;

  if (SimSpeed == 1 && Spdcycle % 5)
    return;

  if (SimSpeed == 2 && Spdcycle % 3)
    return;

  if (++Fcycle > 1023) Fcycle = 0;
/*  if (InitSimLoad) Fcycle = 0; */
  Simulate(Fcycle & 15);
}


/* comefrom: SimFrame */
Simulate(int mod16)
{
  static short SpdPwr[4] = { 1,  2,  4,  5 };
  static short SpdPtl[4] = { 1,  2,  7, 17 };
  static short SpdCri[4] = { 1,  1,  8, 18 };
  static short SpdPop[4] = { 1,  1,  9, 19 };
  static short SpdFir[4] = { 1,  1, 10, 20 };
  short x;

  x = SimSpeed;
  if (x > 3) x = 3;

  switch (mod16)  {
    case 0:
      if (++Scycle > 1023) Scycle = 0;	/* this is cosmic */
      if (DoInitialEval) {
	DoInitialEval = 0;
	CityEvaluation();
      }
      CityTime++;
      AvCityTax += CityTax;		/* post */
      if (!(Scycle & 1)) SetValves();
      ClearCensus();
      break;
    case 1:
      MapScan(0, 1 * WORLD_X / 8);
      break;
    case 2:
      MapScan(1 * WORLD_X / 8, 2 * WORLD_X / 8);
      break;
    case 3:
      MapScan(2 * WORLD_X / 8, 3 * WORLD_X / 8);
      break;
    case 4:
      MapScan(3 * WORLD_X / 8, 4 * WORLD_X / 8);
      break;
    case 5:
      MapScan(4 * WORLD_X / 8, 5 * WORLD_X / 8);
      break;
    case 6:
      MapScan(5 * WORLD_X / 8, 6 * WORLD_X / 8);
      break;
    case 7:
      MapScan(6 * WORLD_X / 8, 7 * WORLD_X / 8);
      break;
    case 8:
      MapScan(7 * WORLD_X / 8, WORLD_X);
      break;
    case 9:
      if (!(CityTime % CENSUSRATE)) TakeCensus();
      if (!(CityTime % (CENSUSRATE * 12))) Take2Census();

      if (!(CityTime % TAXFREQ))  {
	CollectTax();
	CityEvaluation();
      }
      break;
    case 10:
      if (!(Scycle % 5)) DecROGMem();
      DecTrafficMem();
      NewMapFlags[TDMAP] = 1;
      NewMapFlags[RDMAP] = 1;
      NewMapFlags[ALMAP] = 1;
      NewMapFlags[REMAP] = 1;
      NewMapFlags[COMAP] = 1;
      NewMapFlags[INMAP] = 1;
      NewMapFlags[DYMAP] = 1;
      SendMessages();
      break;
    case 11:
      if (!(Scycle % SpdPwr[x])) {
	DoPowerScan();
	NewMapFlags[PRMAP] = 1;
	NewPower = 1; /* post-release change */
      }
      break;
    case 12:
      if (!(Scycle % SpdPtl[x])) PTLScan();
      break;
    case 13:
      if (!(Scycle % SpdCri[x])) CrimeScan();
      break;
    case 14:
      if (!(Scycle % SpdPop[x])) PopDenScan();
      break;
    case 15:
      if (!(Scycle % SpdFir[x])) FireAnalysis();
      DoDisasters();	
      break;
  }
}


extern unsigned long map_sum(void);
extern int simdbg;
#ifdef SIMTRACE
#define DBG(n) if (simdbg) printf("%s %08lx tp%d\n", n, map_sum(), TotalPop)
#else
#define DBG(n)
#endif
/* comefrom: Simulate */
DoSimInit(void)
{
  Fcycle = 0;
  Scycle = 0;

  if (InitSimLoad == 2) 			/* if new city    */
    InitSimMemory();

  if (InitSimLoad == 1)  			/* if city just loaded  */
    SimLoadInit(); DBG("SimLoadInit();");

  SetValves(); DBG("SetValves();");
  ClearCensus(); DBG("ClearCensus();");
#if 1
  MapScan(0, WORLD_X); DBG("MapScan(0, WORLD_X);"); /* XXX are you sure ??? */
#endif
  DoPowerScan(); DBG("DoPowerScan();");
  NewPower = 1;		/* post rel */
  PTLScan(); DBG("PTLScan();");
  CrimeScan(); DBG("CrimeScan();");
  PopDenScan(); DBG("PopDenScan();");
  FireAnalysis(); DBG("FireAnalysis();");
  NewMap = 1;
  doAllGraphs();
  NewGraph = 1;
  TotalPop = 1;
  DoInitialEval = 1;
}


/* comefrom: SimLoadInit */
DoNilPower(void)
{
  register short x, y, z;

  for (x = 0; x < WORLD_X; x++)
    for (y = 0; y < WORLD_Y; y++) {
      z = MAPXY(x, y);
      if (z & ZONEBIT) {
	SMapX = x;
	SMapY = y;
	CChr = z;
	SetZPower();
      }
    }
}


/* comefrom: Simulate */
DecTrafficMem(void)		/* tends to empty TrfDensity   */
{
  register short x, y, z;

  for (x = 0; x < HWLDX; x++)
    for (y = 0; y < HWLDY; y++)
      if (z = TrfDensity[x][y]) {
	if (z > 24) {
	  if (z > 200) TrfDensity[x][y] = z - 34;
	  else TrfDensity[x][y] = z - 24;
	}
	else TrfDensity[x][y] = 0;
      }
}


/* comefrom: Simulate */
DecROGMem(void)			/* tends to empty RateOGMem   */
{
  register short x, y, z;

  for (x = 0; x < SmX; x++)
    for (y = 0; y < SmY; y++)	{
      z = RateOGMem[x][y];
      if (z == 0) continue;
      if (z > 0)  {
	--RateOGMem[x][y];
	if (z > 200) RateOGMem[x][y] = 200;    /* prevent overflow */
	continue;
      }
      if (z < 0)  {
	++RateOGMem[x][y];
	if (z < -200) RateOGMem[x][y] = -200;
      }
    }
}


/* comefrom: DoSimInit */
InitSimMemory(void)
{	
  register short x, z;

  z = 0;
  SetCommonInits();
  for (x = 0; x < 240; x++)  {
    ResHis[x] = z;
    ComHis[x] = z;
    IndHis[x] = z;
    MoneyHis[x] = 128;
    CrimeHis[x] = z;
    PollutionHis[x] = z;
  }
  CrimeRamp = z;
  PolluteRamp = z;
  TotalPop = z;
  RValve = z;
  CValve = z;
  IValve = z;
  ResCap = z;
  ComCap = z;
  IndCap = z;

  EMarket = 6.0;
  DisasterEvent = 0;
  ScoreType = 0;

  /* This clears powermem */
  PowerStackNum = z;		
  DoPowerScan();
  NewPower = 1;		/* post rel */	

  InitSimLoad = 0;
}


/* comefrom: DoSimInit */
SimLoadInit(void)
{
  static short DisTab[9] = { 0, 2, 10, 5, 20, 3, 5, 5, 2 * 48};
  static short ScoreWaitTab[9] = { 0, 30 * 48, 5 * 48, 5 * 48, 10 * 48,
				   5 * 48, 10 * 48, 5 * 48, 10 * 48 };
  register int z;

  z = 0;
  EMarket = (double)MiscHis[1];
  ResPop = MiscHis[2];
  ComPop = MiscHis[3];
  IndPop = MiscHis[4];
  RValve = MiscHis[5];
  CValve = MiscHis[6];
  IValve = MiscHis[7];
  CrimeRamp = MiscHis[10];
  PolluteRamp = MiscHis[11];
  LVAverage = MiscHis[12];
  CrimeAverage = MiscHis[13];
  PolluteAverage = MiscHis[14];
  GameLevel = MiscHis[15];

  if (CityTime < 0) CityTime = 0;
  if (!EMarket) EMarket = 4.0;
  if ((GameLevel > 2) || (GameLevel < 0)) GameLevel = 0;
  SetGameLevel(GameLevel);

  SetCommonInits();

  CityClass = MiscHis[16];
  CityScore = MiscHis[17];

  if ((CityClass > 5) || (CityClass < 0)) CityClass = 0;
  if ((CityScore > 999) || (CityScore < 1)) CityScore = 500;

  ResCap = 0;
  ComCap = 0;
  IndCap = 0;

  AvCityTax = (CityTime % 48) * 7;  /* post */

  for (z = 0; z < PWRMAPSIZE; z++)
    PowerMap[z] = ~0; /* set power Map */
  DoNilPower();

  if (ScenarioID > 8) ScenarioID = 0;

  if (ScenarioID) {
    DisasterEvent = ScenarioID;
    DisasterWait = DisTab[DisasterEvent];
    ScoreType = DisasterEvent;
    ScoreWait = ScoreWaitTab[DisasterEvent];
  } else {
    DisasterEvent = 0;
    ScoreType = 0;
  }

  RoadEffect = 32;
  PoliceEffect = 1000; /*post*/
  FireEffect = 1000;
  InitSimLoad = 0;
}


/* comefrom: InitSimMemory SimLoadInit */
SetCommonInits(void)
{
  EvalInit();
  RoadEffect = 32;
  PoliceEffect = 1000;
  FireEffect = 1000;
  TaxFlag = 0;
  TaxFund = 0;
/*
  if ((GameLevel > 2) || (GameLevel < 0)) GameLevel = 0;
  setGameLevel(GameLevel);
*/
}


/* comefrom: Simulate DoSimInit */
SetValves(void)
{
  static short TaxTable[21] = {
    200, 150, 120, 100, 80, 50, 30, 0, -10, -40, -100,
    -150, -200, -250, -300, -350, -400, -450, -500, -550, -600 };
  double Employment, Migration, Births, LaborBase, IntMarket;
  double Rratio, Cratio, Iratio, temp;
  double NormResPop, PjResPop, PjComPop, PjIndPop;
  register short z;

  MiscHis[1] = (short)EMarket;
  MiscHis[2] = ResPop;
  MiscHis[3] = ComPop;
  MiscHis[4] = IndPop;
  MiscHis[5] = RValve;
  MiscHis[6] = CValve;
  MiscHis[7] = IValve;
  MiscHis[10] = CrimeRamp;
  MiscHis[11] = PolluteRamp;
  MiscHis[12] = LVAverage;
  MiscHis[13] = CrimeAverage;
  MiscHis[14] = PolluteAverage;
  MiscHis[15] = GameLevel;
  MiscHis[16] = CityClass;
  MiscHis[17] = CityScore;

  NormResPop = ResPop / 8;
  LastTotalPop = TotalPop;
  TotalPop = NormResPop + ComPop + IndPop;

  if (NormResPop) Employment = ((ComHis[1] + IndHis[1]) / NormResPop);
  else Employment = 1;

  Migration = NormResPop * (Employment - 1);
  Births = NormResPop * (.02); 			/* Birth Rate  */
  PjResPop = NormResPop + Migration + Births;	/* Projected Res.Pop  */

  if (temp = (ComHis[1] + IndHis[1])) LaborBase = (ResHis[1] / temp);
  else LaborBase = 1;
  if (LaborBase > 1.3) LaborBase = 1.3;
  if (LaborBase < 0) LaborBase = 0;  /* LB > 1 - .1  */

  for (z = 0; z < 2; z++)
    temp = ResHis[z] + ComHis[z] + IndHis[z];
  IntMarket = (NormResPop + ComPop + IndPop) / 3.7;

  PjComPop = IntMarket * LaborBase;			

  z = GameLevel;			/* New ExtMarket */
  temp = 1;
  switch (z)  {
  case 0:
    temp = 1.2;
    break;
  case 1:
    temp = 1.1;
    break;
  case 2:
    temp = .98;
    break;
  }

  PjIndPop = IndPop * LaborBase * temp;
  if (PjIndPop < 5) PjIndPop = 5;

  if (NormResPop) Rratio = (PjResPop / NormResPop); /* projected -vs- actual */
  else Rratio = 1.3;
  if (ComPop) Cratio = (PjComPop / ComPop);
  else Cratio = PjComPop;
  if (IndPop) Iratio = (PjIndPop / IndPop);
  else Iratio = PjIndPop;

  if (Rratio > 2) Rratio = 2;
  if (Cratio > 2) Cratio = 2;
  if (Iratio > 2) Iratio = 2;

  z = CityTax + GameLevel;
  if (z > 20) z = 20;
#ifdef SIMTRACE
  { extern int simdbg; if (simdbg == 3 && CityTime >= 2160) printf("V %ld R%d C%d I%d rp%d cp%d ip%d h%d,%d,%d %.17g %.17g %.17g %.17g\n", (long)CityTime, RValve, CValve, IValve, ResPop, ComPop, IndPop, ResHis[1], ComHis[1], IndHis[1], Rratio, Cratio, Iratio, LaborBase); }
#endif
  Rratio = ((Rratio -1) * 600) + TaxTable[z]; /* global tax/Glevel effects */
  Cratio = ((Cratio -1) * 600) + TaxTable[z];
  Iratio = ((Iratio -1) * 600) + TaxTable[z];

  if (Rratio > 0)		/* ratios are velocity changes to valves  */
    if (RValve <  2000) RValve += Rratio;
  if (Rratio < 0)
    if (RValve > -2000) RValve += Rratio;
  if (Cratio > 0)
    if (CValve <  1500) CValve += Cratio;
  if (Cratio < 0)
    if (CValve > -1500) CValve += Cratio;
  if (Iratio > 0)
    if (IValve <  1500) IValve += Iratio;
  if (Iratio < 0)
    if (IValve > -1500) IValve += Iratio;

  if (RValve >  2000) RValve =  2000;
  if (RValve < -2000) RValve = -2000;
  if (CValve >  1500) CValve =  1500;
  if (CValve < -1500) CValve = -1500;
  if (IValve >  1500) IValve =  1500;
  if (IValve < -1500) IValve = -1500;

  if ((ResCap) && (RValve > 0)) RValve = 0;	/* Stad, Prt, Airprt  */
  if ((ComCap) && (CValve > 0)) CValve = 0;
  if ((IndCap) && (IValve > 0)) IValve = 0;
  ValveFlag = 1;
}


/* comefrom: Simulate DoSimInit */
ClearCensus(void)
{
  register short x, y, z;

  z = 0;
  PwrdZCnt = z;
  unPwrdZCnt = z;
  FirePop = z;
  RoadTotal = z;
  RailTotal = z;
  ResPop = z;
  ComPop = z;
  IndPop = z;
  ResZPop = z;
  ComZPop = z;
  IndZPop = z;
  HospPop = z;
  ChurchPop = z;
  PolicePop = z;
  FireStPop = z;
  StadiumPop = z;
  CoalPop = z;
  NuclearPop = z;
  PortPop = z;
  APortPop = z;
  PowerStackNum = z;		/* Reset before Mapscan */
  for (x = 0; x < SmX; x++)
    for (y = 0; y < SmY; y++) {
      FireStMap[x][y] = z;
      PoliceMap[x][y] = z;
    }
}


/* comefrom: Simulate */
TakeCensus(void)
{
  short x;

  /* put census#s in Historical Graphs and scroll data  */
  ResHisMax = 0;
  ComHisMax = 0;
  IndHisMax = 0;
  for (x = 118; x >= 0; x--)	{
    if ((ResHis[x + 1] = ResHis[x]) > ResHisMax) ResHisMax = ResHis[x];
    if ((ComHis[x + 1] = ComHis[x]) > ComHisMax) ComHisMax = ComHis[x];
    if ((IndHis[x + 1] = IndHis[x]) > IndHisMax) IndHisMax = IndHis[x];
    CrimeHis[x + 1] = CrimeHis[x];
    PollutionHis[x + 1] = PollutionHis[x];
    MoneyHis[x + 1] = MoneyHis[x];
  }

  Graph10Max = ResHisMax;
  if (ComHisMax > Graph10Max) Graph10Max = ComHisMax;
  if (IndHisMax > Graph10Max) Graph10Max = IndHisMax;

  ResHis[0] = ResPop / 8;
  ComHis[0] = ComPop;
  IndHis[0] = IndPop;

  CrimeRamp += (CrimeAverage - CrimeRamp) / 4;
  CrimeHis[0] = CrimeRamp;

  PolluteRamp += (PolluteAverage - PolluteRamp) / 4;
  PollutionHis[0] = PolluteRamp;

  x = (CashFlow / 20) + 128;	/* scale to 0..255  */
  if (x < 0) x = 0;
  if (x > 255) x = 255;

  MoneyHis[0] = x;
  if (CrimeHis[0] > 255) CrimeHis[0] = 255;
  if (PollutionHis[0] > 255) PollutionHis[0] = 255;

  ChangeCensus(); /* XXX: if 10 year graph view */

  if (HospPop < (ResPop >>8)) NeedHosp = TRUE;
  if (HospPop > (ResPop >>8)) NeedHosp = -1;
  if (HospPop == (ResPop >>8)) NeedHosp = FALSE;

  if (ChurchPop < (ResPop >>8)) NeedChurch = TRUE;
  if (ChurchPop > (ResPop >>8)) NeedChurch = -1;
  if (ChurchPop == (ResPop >>8)) NeedChurch = FALSE;
}


/* comefrom: Simulate */
Take2Census(void)    /* Long Term Graphs */
{
  short x;

  Res2HisMax = 0;
  Com2HisMax = 0;
  Ind2HisMax = 0;
  for (x = 238; x >= 120; x--)	{
    if ((ResHis[x + 1] = ResHis[x]) > Res2HisMax) Res2HisMax = ResHis[x];
    if ((ComHis[x + 1] = ComHis[x]) > Com2HisMax) Com2HisMax = ComHis[x];
    if ((IndHis[x + 1] = IndHis[x]) > Ind2HisMax) Ind2HisMax = IndHis[x];
    CrimeHis[x + 1] = CrimeHis[x];
    PollutionHis[x + 1] = PollutionHis[x];
    MoneyHis[x + 1] = MoneyHis[x];
  }
  Graph120Max = Res2HisMax;
  if (Com2HisMax > Graph120Max) Graph120Max = Com2HisMax;
  if (Ind2HisMax > Graph120Max) Graph120Max = Ind2HisMax;

  ResHis[120] = ResPop / 8;
  ComHis[120] = ComPop;
  IndHis[120] = IndPop;
  CrimeHis[120] = CrimeHis[0] ;
  PollutionHis[120] = PollutionHis[0];
  MoneyHis[120] = MoneyHis[0];
  ChangeCensus(); /* XXX: if 120 year graph view */
}


/* comefrom: Simulate */
CollectTax(void)
{	
  static double RLevels[3] = { 0.7, 0.9, 1.2 };
  static double FLevels[3] = { 1.4, 1.2, 0.8 };
  short z;

  CashFlow = 0;
  if (!TaxFlag) { /* if the Tax Port is clear  */
    /* XXX: do something with z */
    z = AvCityTax / 48;  /* post */
    AvCityTax = 0;			
    PoliceFund = PolicePop * 100;
    FireFund   = FireStPop * 100;
    RoadFund   = (RoadTotal + (RailTotal * 2)) * RLevels[GameLevel];
    TaxFund = (((QUAD)TotalPop * LVAverage) / 120) *
      	      CityTax * FLevels[GameLevel];
    if (TotalPop) {	/* if there are people to tax  */
      CashFlow = TaxFund - (PoliceFund + FireFund + RoadFund);

      DoBudget();
    } else {
      RoadEffect = 32;
      PoliceEffect = 1000;
      FireEffect = 1000;
    }
  }
}


UpdateFundEffects(void)
{

  if (RoadFund)
    RoadEffect = (short)(((double)RoadSpend /
			  (double)RoadFund) * 32.0);
  else
    RoadEffect = 32;

  if (PoliceFund)
    PoliceEffect = (short)(((double)PoliceSpend /
			    (double)PoliceFund) * 1000.0);
  else
    PoliceEffect = 1000;

  if (FireFund)
    FireEffect = (short)(((double)FireSpend /
			  (double)FireFund) * 1000.0);
  else
    FireEffect = 1000;

  drawCurrPercents();
}


#if defined(__WATCOMC__) && !defined(NOASM) && !defined(NOSCAN)
/* Returns how many of the cnt tiles at p are left once it finds one with (tile & LOMASK) >= FLOOD
   (that tile counts as left; p itself is not advanced), or 0 if none. */
unsigned short mapskip(short NEAR *p, unsigned short cnt);
#pragma aux mapskip = \
  "or cx, cx" "jz short sdone" "cld" \
  "snext: lods word ptr ss:[si]" "and ax, 3FFh" "cmp ax, 48" "jae short shit" "loop snext" "jmp short sdone" \
  "shit: " "sdone:" \
  parm [si] [cx] value [cx] modify [ax si cx];

/* Same idea with NewPower on: also does SetZPower inline for every conducting tile that needs nothing else,
   and stops at anything the C code must handle (anything between FLOOD and POWERBASE, zone centres,
   rail, tiny explosions, power plants). SetZPower is idempotent, so the C code redoing it on a stop is harmless.
   pb = PowerMapData + PwrXOff[x] (bx), xm = PwrXMask[x] (dx), yo = y * 16 (di). */
unsigned short mapfast(short NEAR *p, unsigned short cnt, unsigned short yo, short NEAR *pb, unsigned short xm);
#pragma aux mapfast = \
  "push bp" "or cx, cx" "jz fdone" "cld" \
  "ftop: mov ax, ss:[si]" "add si, 2" \
  "mov bp, ax" "and bp, 3FFh" \
  "cmp bp, 30h" "jb fnext" \
  "cmp bp, 0D0h" "jb fexit" \
  "test ax, 400h" "jnz fexit" \
  "cmp bp, 0E0h" "jb fnr" "cmp bp, 0F0h" "jb fexit" \
  "fnr: cmp bp, 360h" "jb fne" "cmp bp, 363h" "jbe fexit" \
  "fne: test ax, 4000h" "jz fnext" \
  "cmp bp, 2EEh" "je fexit" "cmp bp, 330h" "je fexit" \
  "test ss:[bx+di], dx" "jz foff" \
  "or word ptr ss:[si-2], 8000h" "jmp short fnext" \
  "foff: and word ptr ss:[si-2], 7FFFh" \
  "fnext: add di, 16" "dec cx" "jz fdone" "jmp ftop" \
  "fexit: " "fdone: pop bp" \
  parm [si] [cx] [di] [bx] [dx] value [cx] modify [ax si cx di bx dx];
#endif

/* TIPC front end: called once per map column inside MapScan so the UI can redraw and read keys while
   the sim works.  The callback must not touch sim state.  NULL in the headless build. */
void (*MapScanYield)(void) = 0;

/* comefrom: Simulate DoSimInit */
MapScan(int x1, int x2)
{
  register short x, y;
  int col;	/* TIPC speed pass: column index into MapData instead of MAPXY(x, y) each tile */
#if defined(__WATCOMC__) && !defined(NOASM) && !defined(NOSCAN)
  /* TIPC speed pass: an asm loop skips tiles below FLOOD (dirt, water, trees: they do
     nothing in the original loop either), so the C dispatch only sees real work. */
  short wy = WORLD_Y;
  short NEAR *p;
  unsigned short rem, n;
  short hidx = -1, last, i;	/* last tile the C code handled (visited with CChr/CChr9 set) */

  for (x = x1; x < x2; x++)  {
    if (MapScanYield) MapScanYield();
    p = (short NEAR *)&MapData[x * WORLD_Y];
    n = wy;
    for (;;) {
      if (NewPower)
	rem = mapfast(p, n, (wy - n) << 4, (short NEAR *)((char NEAR *)PowerMapData + PwrXOff[x]), PwrXMask[x]);
      else
	rem = mapskip(p, n);		/* tiles left, counting the one that stopped it */
      p += n - rem;
      n = rem;
      if (!n) break;
      y = wy - n;
      hidx = (short)(p - (short NEAR *)MapData);
      CChr = *p;
      CChr9 = CChr & LOMASK;
      SMapX = x;
      SMapY = y;
      if (CChr9 < ROADBASE) {
	if (CChr9 >= FIREBASE) {
	  FirePop++;
	  if (!(Rand16() & 3)) DoFire();	/* 1 in 4 times */
	  goto mnext;
	}
	if (CChr9 < RADTILE)  DoFlood();
	else DoRadTile();
	goto mnext;
      }

      if (NewPower && (CChr & CONDBIT))
	SetZPower();

      if (CChr9 < POWERBASE) {
	DoRoad();
	goto mnext;
      }

      if (CChr & ZONEBIT) {	/* process Zones */
	DoZone();
	goto mnext;
      }

      if ((CChr9 >= RAILBASE) &&
	  (CChr9 < RESBASE)) {
	DoRail();
	goto mnext;
      }
      if ((CChr9 >= SOMETINYEXP) &&
	  (CChr9 <= LASTTINYEXP))  /* clear AniRubble */
	MAPXY(x, y) = RUBBLE + (Rand16() & 3) + BULLBIT;
    mnext:
      p++;
      if (!--n) break;
    }
  }
  /* The original loop leaves CChr = last tile of the range and CChr9 = last nonzero tile's
     class; DoPowerScan reads CChr9 (stopall quirk). Tiles the asm loop skipped did not set them,
     so reproduce the final values here. Everything after hidx was skipped, so it is unchanged. */
  last = x2 * wy - 1;
  if (x2 > x1 && hidx != last) {
    CChr = MapData[last];
    for (i = last; i > hidx; i--)
      if (MapData[i]) { CChr9 = MapData[i] & LOMASK; break; }
  }
#else

  for (x = x1; x < x2; x++)  {
    if (MapScanYield) MapScanYield();
    col = x * WORLD_Y;
    for (y = 0; y < WORLD_Y; y++) {
#ifdef SIMTRACE
      if (simdbg == 3 && CityTime >= 2160) { extern unsigned long rngcount; printf("%ld %d %d %04x %lu\n", (long)CityTime, x, y, (unsigned short)MAPXY(x, y), rngcount); }
#endif
      if (CChr = MapData[col + y]) {
	CChr9 = CChr & LOMASK;	/* Mask off status bits  */
	if (CChr9 >= FLOOD) {
	  SMapX = x;
	  SMapY = y;
	  if (CChr9 < ROADBASE) {
	    if (CChr9 >= FIREBASE) {
	      FirePop++;
	      if (!(Rand16() & 3)) DoFire();	/* 1 in 4 times */
	      continue;
	    }
	    if (CChr9 < RADTILE)  DoFlood();
	    else DoRadTile();
	    continue;
	  }

	  if (NewPower && (CChr & CONDBIT))
	    SetZPower();

	  if ((CChr9 >= ROADBASE) &&
	      (CChr9 < POWERBASE)) {
	    DoRoad();
	    continue;
	  }

	  if (CChr & ZONEBIT) {	/* process Zones */
	    DoZone();
	    continue;
	  }

	  if ((CChr9 >= RAILBASE) &&
	      (CChr9 < RESBASE)) {
	    DoRail();
	    continue;
	  }
	  if ((CChr9 >= SOMETINYEXP) &&
	      (CChr9 <= LASTTINYEXP))  /* clear AniRubble */
	    MAPXY(x, y) = RUBBLE + (Rand16() & 3) + BULLBIT;
	}
      }
    }
  }
#endif
}


/* comefrom: MapScan */
DoRail(void)
{
  RailTotal++;
  GenerateTrain(SMapX, SMapY);
  if (RoadEffect < 30) /* Deteriorating  Rail  */
    if (!(Rand16() & 511))
      if (!(CChr & CONDBIT))
	if (RoadEffect < (Rand16() & 31)) {
	  if (CChr9 < (RAILBASE + 2))
	    MAPXY(SMapX, SMapY) = RIVER;
	  else
	    MAPXY(SMapX, SMapY) = RUBBLE + (Rand16() & 3) + BULLBIT;
	  return;
	}
}


/* comefrom: MapScan */
DoRadTile(void)
{
  if (!(Rand16() & 4095)) MAPXY(SMapX, SMapY) = 0; /* Radioactive decay */
}


/* comefrom: MapScan */
DoRoad(void)
{
  register short Density, tden, z;
  static short DenTab[3] = { ROADBASE, LTRFBASE, HTRFBASE };

  RoadTotal++;
/*  GenerateBus(SMapX, SMapY); */
  if (RoadEffect < 30) /* Deteriorating Roads */
    if (!(Rand16() & 511))
      if (!(CChr & CONDBIT))
	if (RoadEffect < (Rand16() & 31)) {
	  if (((CChr9 & 15) < 2) || ((CChr9 & 15) == 15))
	    MAPXY(SMapX, SMapY) = RIVER;
	  else
	    MAPXY(SMapX, SMapY) = RUBBLE + (Rand16() & 3) + BULLBIT;
	  return;
	}

  if (!(CChr & BURNBIT)) { /* If Bridge */
    RoadTotal += 4;			
    if (DoBridge())  return;
  }
  if (CChr9 < LTRFBASE) tden = 0;
  else if (CChr9 < HTRFBASE) tden = 1;
  else {
    RoadTotal++;
    tden = 2;
  }

  Density = (TrfDensity[SMapX >>1][SMapY >>1]) >>6;  /* Set Traf Density  */
  if (Density > 1) Density--;
  if (tden != Density) { /* tden 0..2   */
    z = ((CChr9 - ROADBASE) & 15) + DenTab[Density];
    z += CChr & (ALLBITS - ANIMBIT);
    if (Density) z += ANIMBIT;
    MAPXY(SMapX, SMapY) = z;
  }
}


/* comefrom: MapScan */
DoBridge(void)
{
  static short HDx[7] = { -2,  2, -2, -1,  0,  1,  2 };
  static short HDy[7] = { -1, -1,  0,  0,  0,  0,  0 };
  static short HBRTAB[7] = {
    HBRDG1 | BULLBIT, HBRDG3 | BULLBIT, HBRDG0 | BULLBIT,
    RIVER, BRWH | BULLBIT, RIVER, HBRDG2 | BULLBIT };
  static short HBRTAB2[7] = {
    RIVER, RIVER, HBRIDGE | BULLBIT, HBRIDGE | BULLBIT, HBRIDGE | BULLBIT,
    HBRIDGE | BULLBIT, HBRIDGE | BULLBIT };
  static short VDx[7] = {  0,  1,  0,  0,  0,  0,  1 };
  static short VDy[7] = { -2, -2, -1,  0,  1,  2,  2 };
  static short VBRTAB[7] = {
    VBRDG0 | BULLBIT, VBRDG1 | BULLBIT, RIVER, BRWV | BULLBIT,
    RIVER, VBRDG2 | BULLBIT, VBRDG3 | BULLBIT };
  static short VBRTAB2[7] = {
    VBRIDGE | BULLBIT, RIVER, VBRIDGE | BULLBIT, VBRIDGE | BULLBIT,
    VBRIDGE | BULLBIT, VBRIDGE | BULLBIT, RIVER };
  register z, x, y, MPtem;

  if (CChr9 == BRWV) { /*  Vertical bridge close */
    if ((!(Rand16() & 3)) &&
	(GetBoatDis() > 340))
      for (z = 0; z < 7; z++) { /* Close  */
	x = SMapX + VDx[z];
	y = SMapY + VDy[z];
	if (TestBounds(x, y))
	  if ((MAPXY(x, y) & LOMASK) == (VBRTAB[z] & LOMASK))
	    MAPXY(x, y) = VBRTAB2[z];
      }
    return (TRUE);
  }
  if (CChr9 == BRWH) { /*  Horizontal bridge close  */
    if ((!(Rand16() & 3)) &&
	(GetBoatDis() > 340))
      for (z = 0; z < 7; z++) { /* Close  */
	x = SMapX + HDx[z];
	y = SMapY + HDy[z];
	if (TestBounds(x, y))
	  if ((MAPXY(x, y) & LOMASK) == (HBRTAB[z] & LOMASK))
	    MAPXY(x, y) = HBRTAB2[z];
      }
    return (TRUE);
  }

  if ((GetBoatDis() < 300) || (!(Rand16() & 7))) {
    if (CChr9 & 1) {
      if (SMapX < (WORLD_X - 1))
	if (MAPXY(SMapX + 1, SMapY) == CHANNEL) { /* Vertical open */
	  for (z = 0; z < 7; z++) {
	    x = SMapX + VDx[z];
	    y = SMapY + VDy[z];
	    if (TestBounds(x, y))  {
	      MPtem = MAPXY(x, y);
	      if ((MPtem == CHANNEL) ||
		  ((MPtem & 15) == (VBRTAB2[z] & 15)))
		MAPXY(x, y) = VBRTAB[z];
	    }
	  }
	  return (TRUE);
	}
      return (FALSE);
    } else {
      if (SMapY > 0)
	if (MAPXY(SMapX, SMapY - 1) == CHANNEL) { /* Horizontal open  */
	  for (z = 0; z < 7; z++) {
	    x = SMapX + HDx[z];
	    y = SMapY + HDy[z];
	    if (TestBounds(x, y)) {
	      MPtem = MAPXY(x, y);
	      if (((MPtem & 15) == (HBRTAB2[z] & 15)) ||
		  (MPtem == CHANNEL))
		MAPXY(x, y) = HBRTAB[z];
	    }
	  }
	  return (TRUE);
	}
      return (FALSE);
    }
  }
  return (FALSE);
}


int
GetBoatDis(void)
{
  register int dist, mx, my, dx, dy;
  SimSprite SPRNEAR *sprite;

  dist = 32767; /* TIPC 16-bit fix: was 99999, which wraps negative in a 16-bit int */
  mx = (SMapX <<4) + 8;
  my = (SMapY <<4) + 8;

  for (sprite = sim->sprite; sprite != NULL; sprite = sprite->next) {
    if ((sprite->type == SHI) &&
	(sprite->frame != 0)) {
      if ((dx = sprite->x + sprite->x_hot - mx) < 0) dx = -dx;
      if ((dy = sprite->y + sprite->y_hot - my) < 0) dy = -dy;
      dx += dy;
      if (dx < dist) dist = dx;
    }
  }
  return (dist);
}


/* comefrom: MapScan */
DoFire(void)
{
  static short DX[4] = { -1,  0,  1,  0 };
  static short DY[4] = {  0, -1,  0,  1 };
  register short z, Xtem, Ytem, Rate, c;

  for (z = 0; z < 4; z++) {
    if (!(Rand16() & 7)) {
      Xtem = SMapX + DX[z];
      Ytem = SMapY + DY[z];
      if (TestBounds(Xtem, Ytem)) {
	c = MAPXY(Xtem, Ytem);
	if (c & BURNBIT) {
	  if (c & ZONEBIT) {
	    FireZone(Xtem, Ytem, c);
	    if ((c & LOMASK) > IZB)  { /*  Explode  */
	      MakeExplosionAt((Xtem <<4) + 8, (Ytem <<4) + 8);
	    }
	  }
	  MAPXY(Xtem, Ytem) = FIRE + (Rand16() & 3) + ANIMBIT;
	}
      }
    }
  }
  z = FireRate[SMapX >>3][SMapY >>3];
  Rate = 10;
  if (z) {
    Rate = 3;
    if (z > 20) Rate = 2;
    if (z > 100) Rate = 1;
  }
  if (!Rand(Rate))
    MAPXY(SMapX, SMapY) = RUBBLE + (Rand16() & 3) + BULLBIT;
}


/* comefrom: DoFire MakeFlood */
FireZone(int Xloc, int Yloc, int ch)
{
  register short Xtem, Ytem;
  short x, y, XYmax;

  RateOGMem[Xloc >>3][Yloc >>3] -= 20;

  ch = ch & LOMASK;
  if (ch < PORTBASE)
    XYmax = 2;
  else
    if (ch == AIRPORT)
      XYmax = 5;
    else
      XYmax = 4;

  for (x = -1; x < XYmax; x++)
    for (y = -1; y < XYmax; y++) {
      Xtem = Xloc + x;
      Ytem = Yloc + y;
      if ((Xtem < 0) || (Xtem > (WORLD_X - 1)) ||
	  (Ytem < 0) || (Ytem > (WORLD_Y - 1)))
	continue;
      if ((short)(MAPXY(Xtem, Ytem) & LOMASK) >= ROADBASE) /* post release */
	MAPXY(Xtem, Ytem) |= BULLBIT;
    }
}


/* comefrom: DoSPZone DoHospChur */
RepairZone(short ZCent, short zsize)
{
  short cnt;
  register short x, y, ThCh;

  zsize--;
  cnt = 0;
  for (y = -1; y < zsize; y++)
    for (x = -1; x < zsize; x++) {
      int xx = SMapX + x;
      int yy = SMapY + y;
      cnt++;
      if (TestBounds(xx, yy)) {
	ThCh = MAPXY(xx, yy);
	if (ThCh & ZONEBIT) continue;
	if (ThCh & ANIMBIT) continue;
	ThCh = ThCh & LOMASK;
	if ((ThCh < RUBBLE) || (ThCh >= ROADBASE)) {
	  MAPXY(xx, yy) = ZCent - 3 - zsize + cnt + CONDBIT + BURNBIT;
	}
      }
    }
}


/* comefrom: DoZone */
DoSPZone(short PwrOn)
{
  static short MltdwnTab[3] = { 30000, 20000, 10000 };  /* simadj */
  register z;

  switch (CChr9) {

  case POWERPLANT:
    CoalPop++;
    if (!(CityTime & 7))
      RepairZone(POWERPLANT, 4); /* post */
    PushPowerStack();
    CoalSmoke(SMapX, SMapY);
    return;

  case NUCLEAR:
    if (!NoDisasters && !Rand(MltdwnTab[GameLevel])) {
      DoMeltdown(SMapX, SMapY);
      return;
    }
    NuclearPop++;
    if (!(CityTime & 7))
      RepairZone(NUCLEAR, 4); /* post */
    PushPowerStack();
    return;

  case FIRESTATION:
    FireStPop++;
    if (!(CityTime & 7))
      RepairZone(FIRESTATION, 3); /* post */

    if (PwrOn)
      z = FireEffect;			/* if powered get effect  */
    else
      z = FireEffect >>1;		/* from the funding ratio  */

    if (!FindPRoad())
      z = z >>1;			/* post FD's need roads  */

    FireStMap[SMapX >>3][SMapY >>3] += z;
    return;

  case POLICESTATION:
    PolicePop++;
    if (!(CityTime & 7))
      RepairZone(POLICESTATION, 3); /* post */

    if (PwrOn)
      z = PoliceEffect;
    else
      z = PoliceEffect >>1;

    if (!FindPRoad())
      z = z >>1; /* post PD's need roads */

    PoliceMap[SMapX >>3][SMapY >>3] += z;
    return;

  case STADIUM:
    StadiumPop++;
    if (!(CityTime & 15))
      RepairZone(STADIUM, 4);
    if (PwrOn)
      if (!((CityTime + SMapX + SMapY) & 31)) {	/* post release */
	DrawStadium(FULLSTADIUM);
	MAPXY(SMapX + 1, SMapY) = FOOTBALLGAME1 + ANIMBIT;
	MAPXY(SMapX + 1, SMapY + 1) = FOOTBALLGAME2 + ANIMBIT;
      }
    return;

 case FULLSTADIUM:
    StadiumPop++;
    if (!((CityTime + SMapX + SMapY) & 7))	/* post release */
      DrawStadium(STADIUM);
    return;

 case AIRPORT:
    APortPop++;
    if (!(CityTime & 7))
      RepairZone(AIRPORT, 6);

    if (PwrOn) { /* post */
      if ((MAPXY(SMapX + 1, SMapY - 1) & LOMASK) == RADAR)
	MAPXY(SMapX + 1, SMapY - 1) = RADAR + ANIMBIT + CONDBIT + BURNBIT;
    } else
      MAPXY(SMapX + 1, SMapY - 1) = RADAR + CONDBIT + BURNBIT;

    if (PwrOn)
      DoAirport();
    return;

 case PORT:
    PortPop++;
    if ((CityTime & 15) == 0) {
      RepairZone(PORT, 4);
    }
    if (PwrOn &&
	(GetSprite(SHI) == NULL)) {
      GenerateShip();
    }
    return;
  }
}


/* comefrom: DoSPZone */
DrawStadium(int z)
{
  register int x, y;

  z = z - 5;
  for (y = (SMapY - 1); y < (SMapY + 3); y++)
    for (x = (SMapX - 1); x < (SMapX + 3); x++)
      MAPXY(x, y) = (z++) | BNCNBIT;
  MAPXY(SMapX, SMapY) |= ZONEBIT | PWRBIT;
}


/* comefrom: DoSPZone */
DoAirport(void)
{
  if (!(Rand(5))) {
    GeneratePlane(SMapX, SMapY);
    return;
  }
  if (!(Rand(12)))
    GenerateCopter(SMapX, SMapY);
}


/* comefrom: DoSPZone */
CoalSmoke(int mx, int my)
{
  static short SmTb[4] = { COALSMOKE1, COALSMOKE2, COALSMOKE3, COALSMOKE4 };
  static short dx[4] = {  1,  2,  1,  2 };
  static short dy[4] = { -1, -1,  0,  0 };
  register short x;

  for (x = 0; x < 4; x++)
    Map[mx + dx[x]][my + dy[x]] =
      SmTb[x] | ANIMBIT | CONDBIT | PWRBIT | BURNBIT;
}


/* comefrom: DoSPZone MakeMeltdown */
DoMeltdown(int SX, int SY)
{
  register x, y, z, t;

  MeltX = SX; MeltY = SY;

  MakeExplosion(SX - 1, SY - 1);
  MakeExplosion(SX - 1, SY + 2);
  MakeExplosion(SX + 2, SY - 1);
  MakeExplosion(SX + 2, SY + 2);

  for (x = (SX - 1); x < (SX + 3); x++)
    for (y = (SY - 1); y < (SY + 3); y++)
      MAPXY(x, y) = FIRE + (Rand16() & 3) + ANIMBIT;

  for (z = 0; z < 200; z++)  {
    x = SX - 20 + Rand(40);
    y = SY - 15 + Rand(30);
    if ((x < 0) || (x >= WORLD_X) ||
	(y < 0) || (y >= WORLD_Y))
      continue;
    t = MAPXY(x, y);
    if (t & ZONEBIT)
      continue;
    if ((t & BURNBIT) || (t == 0))
      MAPXY(x, y) = RADTILE;
  }

  ClearMes();
  SendMesAt(-43, SX, SY);
}


#define RANDOM_RANGE 0xffff

short
Rand(short range)
{
  /* TIPC speed pass: unsigned 16-bit math gives the same results as the
     32-bit version for range 0..32767 (RANDOM_RANGE / r and rnum fit 16 bits).
     maxMultiple is cached per range and power-of-two ranges use a mask. */
  static unsigned short MaxMul[64];
  unsigned short maxMultiple, rnum, r;

  r = (unsigned short)range + 1;
  if (r < 64) {
    if (!(maxMultiple = MaxMul[r])) {
      maxMultiple = (unsigned short)RANDOM_RANGE / r;
      MaxMul[r] = maxMultiple *= r;
    }
  } else {
    maxMultiple = (unsigned short)RANDOM_RANGE / r;
    maxMultiple *= r;
  }
  while ((rnum = FASTRAND()) >= maxMultiple)
    continue;
  if (!(r & (r - 1)))
    return (short)(rnum & (r - 1));
  return (short)(rnum % r);
}


#undef Rand16
int
Rand16(void)
{
  return ((int)sim_rand32()); /* callers mask or cast to short */
}
#ifdef FASTRAND_INLINE
#define Rand16() ((int)sim_rand16())
#endif


int
Rand16Signed(void)
{
  /* TIPC 16-bit fix: compute in 32 bits, result fits a short. */
  QUAD i = sim_rand32();

  if (i > 32767) {
    i = 32767 - i;
  }
  return ((int)i);
}


RandomlySeedRand()
{
  /* TIPC: time source moved to the platform layer (0 in the headless test). */
  SeedRand((int)(sim_entropy() ^ sim_rand32()));
}


SeedRand(int seed)
{
  sim_srand(seed);
}
