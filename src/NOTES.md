# Micropolis for TIPC: port notes

Source: SimHacker/micropolis, `micropolis-activity/src/sim/s_*.c` (Don Hopkins GPL v3 release).
Copied 2026-10-22. License files: COPYING (GPL v3), MicropolisPublicNameLicense.md.
Code by Shurr Nuff (AI assistant), produced by Will (stovebolt), Flying Horse Labs.

## Layout
- `s_*.c` original sim core (minimal edits, every edit tagged `TIPC`).
- `headers/sim.h` trimmed headless header (original kept as `headers/sim_orig.h`); `port.h` (QUAD = exact 32-bit), `stubs.h`.
- `glue.c` headless replacements for the X11/Tk front end (budget, graphs, RNG, funds, no-op UI, no sprites).
- `driver.c` test driver: `simtest CITY.CTY YEARS [seed] [debug]`, one census line per game year:
  year, CityTime, funds, CityPop, R/C/I history, score, class, crime, pollution, land value, traffic, 32-bit map checksum.
- `Makefile` (Linux gcc reference), `Makefile.wat` (Open Watcom, 8086, large model, `-0 -fpi -ox`).
- `runtests.sh YEARS` runs all 24 bundled cities on Linux and on DOS (emu2) and compares the logs.
- `ref/` saved 12-year reference logs for haight, kobe, linecity.
- Build `-DSIMTRACE` for per-stage and per-tile trace output (debug arg 1/2/3).

## Test conditions (both builds)
Seed 1, auto-budget on, auto-bulldoze on, disasters off, speed 3, no sprites (trains, planes,
ships, copters, monsters, tornadoes are not simulated in the headless build; they come back with the TIPC front end).

## Step 1 fixes (Linux reference, also needed for DOS)
1. `Sim *sim` was NULL in headless mode; GetBoatDis dereferenced it. Now an empty static Sim.
2. s_eval.c VoteProblems read `ProblemTable[PROBNUM]` (one past the end): undefined, made runs nondeterministic. Slot now treated as 0, random draw kept.
3. RandomlySeedRand used gettimeofday; now uses platform `sim_entropy()` (0 in tests, timer in the game).
4. `start_time`/gettimeofday calls removed from s_fileio.c and s_gen.c.
5. QUAD was `long` (64-bit on Linux); now exactly 32-bit everywhere (`port.h`). `long l` in loadFile/saveFile and the swap helpers changed to QUAD.

## 16-bit fixes (Open Watcom / 8086)
1. **Rand()**: `maxMultiple`/`rnum` were int; RANDOM_RANGE 0xffff and sim_rand() 0..65535 overflow 16 bits. Now 32-bit (QUAD); new `sim_rand32()`.
2. **Rand16Signed()**: same; computes in 32 bits, result fits a short. Rand16() callers all mask or cast, kept int.
3. **GetBoatDis()**: `dist = 99999` wrapped to a negative int, so bridges always thought a boat was near and skipped a random draw. Now 32767. (Found by per-tile RNG trace on kobe.)
4. **DoBudget** (from w_budget.c): `(int)(FireFund * firePercent)` etc. truncated to 16 bits; now `(QUAD)`.
5. **saveFile**: `(int)(policePercent * 65536)` etc. truncated; now `(QUAD)`.
6. **Floating point**: all `float` in the sim (SetValves, GetScore, fund effects, budget percents, eval) changed to `double`, so gcc does not compute in single precision while Watcom promotes. Then `-fpc` (Watcom software-float library) was still off by 1 ULP in SetValves (0.94000000000000017 vs IEEE 0.93999999999999995), which flipped a truncation to short and changed the RNG path in radial at year 1946. Switched to `-fpi` (8087 instructions with the Watcom 8087 emulator linked in; on a TIPC without an 8087 the emulator runs, same as in emu2, which has no FPU). With `-fpi` all results match. (Found with a per-tile RNG trace plus a SetValves trace, `-DSIMTRACE`.)
7. Prototypes added for Rand/Rand16/Rand16Signed/sim_rand32/SetFunds/Spend (long args/returns must be prototyped on 16-bit).

## Results 2026-10-22
- Linux: two runs identical for all cities (also identical at -O0/-O2 and under ASan/UBSan, no errors).
- DOS build: SIMTEST.EXE 67,240 bytes (with the 8087 emulator). Load image 87,712 bytes (code about 61 KB, DGROUP 25.6 KB incl. 8 KB stack), plus about 50 KB heap for maps, so about 140 KB total for the headless sim.
- Run in emu2 (headless DOS emulator, 8086, no FPU): **all 24 bundled cities match the Linux reference byte for byte for 12 years and for 30 years** (`./runtests.sh 12`, `./runtests.sh 30`).
- Map allocation is already one 24,000-byte block (single segment); the 120 row pointers are far pointers in large model. Speed work (near offsets / asm scan) is step 3.

## Open items
- Run in MAME tipc with TI DOS 2.11 and time it (step 3). DOSBox-X build on the box is missing shared libs (libSDL2_net, libpcap, libslirp, libfluidsynth), so emu2 was used for correctness.
- Disasters and sprites (trains, planes, ships, monster, tornado) are not in the headless comparison; they return with the TIPC front end.
- 8087 emulator speed on a 5 MHz 8088 is unknown; the float code runs only a few times per game month, so expected to be minor.

## Step 3: MAME tipc timing (2026-10-22)
Setup: `mame/run.sh OUTDIR RAM YEARS CITY` builds a 360K TI MS-DOS 2.11 boot disk (A:) and a 720K data disk (B:),
boots our MAME tipc (speaker commit 84a61cf4, 8088 at 5 MHz), runs `SIMTEST CITY.CTY YEARS > OUT0.TXT`.
`mame/time.lua` finds the `MPTMARK!` marker in RAM, logs emulated time at every CityTime change and samples CS:IP each frame.
All MAME outputs match the Linux reference exactly.

| City (pop at start) | RAM | s per game month | s per game year | load+init |
|---|---|---|---|---|
| linecity (small, ~25K) | 768K | 3.6 | 44 | 18 s |
| radial (mid, ~77K) | 768K | 9.4 | 113 | 22 s |
| haight (large, ~430K) | 768K | 22.5 | 270 | 29 s |
| haight first year | 192K / 256K / 512K | 19.4 | (1 yr run) | 29 s |

Memory: SIMTEST.EXE 67,496 bytes; load image 87.7 KB; maps 57 KB (3,550 paragraphs, MEM lines). CHKDSK free with
TI DOS 2.11 + COMMAND.COM: 750,560 (768K), 488,416 (512K), 226,272 (256K), 160,736 (192K). The headless sim ran
correctly at 192K, 256K, 512K and 768K. MAME offers 64/128/192/256/512/768K, which matches real TI boards (64-256K on
the motherboard and memory-connector board, then 256K expansion boards); 384K is not a stock TI configuration.

Profile (haight, 36,162 samples): TestForCond 13.9%, MapScan 13.8%, SetZPower 12.3%, DoPowerScan 5.7%,
C library + 8087 emulator 5.5%, DriveDone 5.1%, MoveMapSim 5.1%, TryGo 4.8%, sim_rand32 4.7%, DoRoad 3.3%,
GetFromMap 3.1%, PTLScan 2.0%. Power flood fill (TestForCond+SetZPower+DoPowerScan) about 32%,
traffic (DriveDone, TryGo, MoveMapSim, GetFromMap, TryDrive, FindP*) about 21%.
linecity: MapScan 32%, PTLScan 7.5%, DecTrafficMem 6.3%, DoSmooth 5.8%.
Floating point: all runtime library code including the 8087 emulator is about 6% on every city, so a fixed-point
rewrite is not worth it for speed. MAME tipc does not emulate the 8087 socket (noted in tipc.cpp), so no 8087 numbers.

## Step 3b progress (checkpoint 2026-10-22 18:40 America/Chicago)
Commits so far: 62a70e8, d57704e, 203a4e1, a97dfd0, 7e93128, ab3eced, b733258, 22331f3. All 24 cities x 30 years match
the ref30/ baseline on Linux and DOS (emu2) at HEAD 22331f3. MAME timings of 22331f3 (1 yr, same at 768K and 256K):
linecity 3.18 s/month, radial 7.72, haight 18.16 (step 3: 3.6 / 9.4 / 22.5), i.e. only 1.2x. Profile (haight):
MapScan 16%, DoPowerScan 14.6%, SetZPower 12.6%, sim_rand32 9.5%, TryGo 8.1%, DriveDone 4.8%, PTLScan 4.6%, DoRoad 4.5%.
Next: table lookups instead of variable shifts (8086 `shl r,cl` costs 4 clocks per bit), SetZPower in asm, RNG inlined
at call sites (#pragma aux, no far call), MapScan asm skip loop for tiles below FLOOD, cached Rand() ranges.
`mame/prof.py prof.txt SIMTEST.MAP` maps CS:IP samples to symbols. MAME RAM options are lowercase: 256k, 768k.

### Checkpoint 2026-10-22 ~19:00 America/Chicago
- 6b5650e: MapScan asm skip loop + SetZPower asm. MAME (haight, 1 yr): 18.8 s/month (no gain: 8088 cost is per instruction, not per call).
- Found: DoPowerScan's "stopall" quirk reads the global CChr9, which MapScan leaves at the last nonzero tile it visited.
  Skipping tiles changes that value. 6b5650e only passed because a power plant is rarely the last tile. Fixed by
  recomputing the final CChr/CChr9 at the end of MapScan (commit after b4d3a41 chain, see git log).
- New asm `mapfast` does SetZPower inline for conducting tiles and stops at roads, zone centres, rail, explosions,
  plants. First version broke radial on DOS (CChr9 quirk above); with the fix all 24 cities x 30 years match on Linux and DOS.
- MAME haight with mapfast: 15.7 s/month (1.44x vs step 3).

## Step 3b result (2026-10-22, America/Chicago): speed pass, target 2-3x NOT met
MAME tipc, TI DOS 2.11, 1 game year, HEAD 179cdc2 (+ notes). Identical at 768K and 256K. Output of every run equals the
reference log; all 24 cities x 30 years match the ref30/ baseline on Linux and under DOS (emu2).

| City | Step 3 (s/month) | Step 3b (s/month) | Speedup | s/game year before -> after |
|---|---|---|---|---|
| linecity (small) | 3.56 | 3.05 | 1.17x | 42.7 -> 36.6 |
| radial (mid) | 9.40 | 6.93 | 1.36x | 112.8 -> 83.2 |
| haight (large) | 22.5 | 15.67 | 1.44x | 270 -> 188 |

Tried and dropped: `-os` instead of `-ox` (haight 18.9, slower). Remaining profile (haight): MapScan 16%, TryGo 15%,
DoPowerScan 15%, DriveDone 6%, DoRoad 6%, PTLScan 5%. The 8088 runs about one instruction per 20 clocks here, so
gains come only from fewer instructions; each remaining function is already tight. Further 2x needs algorithmic change
(e.g. a bulk power-bit pass, or skipping scans), which changes behaviour and is not done. DGROUP is 52,448 bytes.
Gotchas found: asm that touches the data segment must use `ss:` (DS is not DGROUP everywhere); inline `_asm` in a function
with register variables can clobber them (use `#pragma aux` with a modify list); DoPowerScan reads the global CChr9
left over from MapScan, so any MapScan shortcut must restore the final CChr/CChr9.

## Step 4: map view (2026-10-22 evening, America/Chicago)
Files: `art/tools/mktiles.py` (converter, run at build time; GPL art in `art/src/`), `art/tools/render_city.py` (offline preview),
`tipc/tblit.asm` (asm_draw_map / asm_draw_map1 blitters), `tipc/mptipc.c` (front end), `tipc/build.sh`, `tipc/run_mame.sh`
(+ `mkdisks.py`, `shots.lua`: boots TI DOS 2.11 in MAME, runs MPTIPC, takes snapshots, injects keys). Screenshots in `shots/`.

Design as built
- Tiles are 16x9 (a TIPC pixel is 1.8x taller than wide, so 16x9 is within 1.3% of square). Window = 45x32 tiles = 720x288 px; the
  25th text row is the status line, drawn in the text layer (no font code). Map is 120x100, so the window scrolls.
- 8-colour: each tile pixel pair is a 50% checkerboard of two TIPC colours (8 pure + 28 pairs, matched in Lab with linear-light mixing),
  per-tile 3 planes x 9 words. TILES8.BIN = 52,368 B (dedupes identical tiles). Source dirt really is salmon (201,126,101): red/white checker.
- Mono: from the original 1-bit set (tilesbw.xpm), downscaled with Bayer 4x4 ordered dither + sharpen 1.5; plane A only, palette latch
  value 0x02 so pixel value 1 = white. TILES1.BIN = 18,366 B. (Dithering the colour set to luminance looked noisy; rejected.)
- Blitter: one far tile segment (load_tiles), map offsets in DGROUP via `ss:`, shadow array `shown[]`; only changed tiles are drawn
  (movsw per row per plane, +90 per row). Cursor = XOR frame. Page-style scrolling (recentre when the cursor leaves the window).
- Sim and UI: MapScanYield callback per MapScan column calls ui_tick (keys + redraw at most every 100 ms), so keys stay responsive
  while the sim runs. Start view is centred on the centroid of developed tiles. Keys: arrows move cursor, SPACE pause, ESC quit.
- Bug found: NASM without `cpu 8086` silently turned an out-of-range `je` into the 386 `0F 84` near jump (illegal on the 8088; emu2
  reported "unimplemented opcode 0F"). Fixed with `cpu 8086` and an explicit `jne draw / jmp skip`.

Numbers (MAME tipc 5 MHz 8088, haight, 1440 tiles visible)
- Full redraw (every tile): colour 235 ms, mono 110 ms. Clean compare pass (nothing changed): 26-28 ms.
- Dirty redraw while the sim runs: 592 tiles over 8 game weeks (128 phases) = 20-50 cs total, i.e. 25-50 ms per 16 phases.
- Memory: MPTIPC.EXE 75,740 B. At 256K RAM: colour 6,016 B free after tiles+city (runs, tight), mono 40,016 B free; at 768K 530,304 B free.
- Load: city+tiles read takes 31-45 s in MAME (floppy at one sector per revolution, no interleave); whole boot-to-map is about 140 s.
  Worth a follow-up: format/interleave the image or use bigger reads.
- Interactive MAME runs at 256K and 768K, colour and mono: map renders, keys scroll, status line shows date/funds/pop/cursor, and the
  map updates as the sim runs (haight Nov 2000 -> Aug 2002 in 600 s). Snapshots: `shots/step4_*.png` (+ `_crt.png`, 960x720 via tipc_preview.py --crt).
- Not done: the 4x3 small tile set (tilessm.xpm) is not converted yet; it is only needed for an overview map (later step).

## Step 5: keyboard UI (2026-10-22 evening, America/Chicago)
Done-when (design doc): a new city can be founded, zoned, powered and grown to 2,000 population in MAME using only the keyboard. MET
(colour and mono, 256K RAM, typed from a script through the real INT 4Ah keyboard path).

Files
- `tool.c` = the original `w_tool.c` (all tools, footprints, costs, rubble/zone fixing) cut loose from X11/Tk; entry `do_tool_xy(state,x,y)`
  (1 ok, -1 bad location, -2 no funds). `tcon.c` = `w_con.c` (road/rail/wire auto-connect). Costs are the original `CostOf[]`.
- `tipc/strings.c` = the original query and message strings (res/stri.*). `tipc/mptipc.c` = UI. `glue.c`/`s_alloc.c` memory changes (below).
- `tools/botmap.c, botrun.c, mkplan.py` = Linux headless player and a plan generator: `mkplan.py SEED PREFIX` writes the same town as a bot
  plan and as the keystroke script used in MAME (about 375 keys). It was used to find a flat site (seed 1) and to prove the plan reaches 2,000.

Keys (H shows them in game)
- Arrows move the cursor (map page-scrolls); Enter builds/uses the tool; D toggles drag mode (build on every move, for roads/wires/rails/doze);
  G = go to X Y; Space = pause; - / + = slower / faster (slow, medium, fast; status shows PAUSE/SLOW/MED/FAST);
  X = disasters on/off (OFF by default); Z = new city (Y confirms); ESC = quit (Y confirms); H or ? = help.
- Tools by letter or F-key: Q/F1 query, B/F2 bulldozer, R/F3 road, T/F4 rail, W/F5 wire, 1/F6 residential, 2/F7 commercial,
  3/F8 industrial, P/F9 police, F/F10 fire, K park, S stadium, C coal, N nuclear, E seaport, A airport. Zone footprints show as a 3x3/4x4/6x6
  XOR frame when fully on screen.
- Status line: date, funds, population, R/C/I demand (+ wanted / - not wanted), tool and cost, speed, DRAG, message, cursor x,y.
  Messages come from the sim (power out, need roads, ...) and from tool results (Bad location, Not enough funds, query text).
- Query tool: Enter on a tile shows its name plus density, land value, crime, pollution and growth rate, with the original strings.
- Command line: `MPTIPC [city.cty] [N[seed]] [M] [B]`. No city file = generate a new city (terrain generator from `s_gen.c`, N1234 = fixed seed).
  M = mono, B = benchmark text. Disasters are off until X.
- Not in this step: budget/tax dialog and graphs/evaluation screens (taxes and funds work with the default 7% and auto budget), sprites
  (ships, trains, planes, monsters are still stubbed), sound.

Test (MAME tipc, 256K, interleave 2): `tipc/run_mame.sh DIR - "N1" SECS` with env `SCRIPT=/tmp/town_keys.txt START=65 STEP=0.25`
(`shots.lua` types one token per STEP seconds: characters, `{LEFT}` etc. via the ioport, newline = Enter, `{WAIT}` = nothing).
- Colour 256K, seed 1: pop 300 Apr 1901, 1140 Jan 1902, 2060 Mar 1903, 3300 Apr 1904, 7440 by 1907 (an earlier run).
- Mono 256K: pop 1840 Dec 1904, 4240 May 1905, 6740 1908. (Sim timing depends on key timing, so runs differ a little.)
- The Linux bot with the same plan reaches 2,000 in 1903. Snapshots: `shots/step5_color_pop*_crt.png`, `step5_mono_pop4240_crt.png`,
  `step5_help_crt.png`, `step5_query_crt.png`.
- Also checked in MAME: help, goto prompt, drag, speed -/-, pause, X disasters message, Z/Y new city, query results.

Memory work (256K, colour; "free" = DOS paragraphs free after tiles+city, measured before any printf)
- Start of step 5: 6,016 B (step 4, no UI). With the UI added (+20K code) it first dropped to 3.6K and failed to load tiles.
- Now (default build, `-fpi`, checkall green): colour 18.4K free after tiles+city, mono 52.4K, 768K about 540K. Target of 40K for colour is NOT met.
- Optional `FP=c tipc/build.sh` (`-fpc`, no 8087 emulator): colour 24.9K, mono 58.8K, and the sim runs faster. NOT the default: with the
  small software-float library one of the 24 test cities (radial) diverges from the Linux baseline after 1926 (RValve differs by 1 in
  SetValves, a last-bit rounding difference in the library), so checkall is not green. Needs either a correctly rounded replacement for
  the few double expressions in SetValves/eval or acceptance of tiny sim differences; Will's call.
- What helped: scratch maps alias the power-scan stacks (-6000 B heap); sim maps (3000 B each, would waste 5K in 4K segments) carved
  from a static near arena (11,000 B) plus one exact DOS block (8,700 B); `_fheapshrink/_nheapshrink` after load to free fopen buffers;
  the Pentium FDIV probe stubbed out; `-fpc` (see above) would drop the 10.6K 8087 emulator; stack 2048 (deepest use measured 716 B).
- What did not help: -os for cold files; RAM-saving by shrinking BSS or stack (DGROUP is padded to 64K anyway, only code, heap and tiles
  cost RAM); deflating tiles (24.9K but not random access; planes are almost never constant, 11 planes only).
- Remaining ideas, with rough gains: replace stdio (fopen/fread/fseek, printf) by a minimal DOS handle layer, +4..6K; lazy-load rare tile
  ranges (disaster/animation frames) or a tile cache backed by the floppy, up to +13K but slows scrolling; use the unused 4.4K at the end of
  each video plane (C000/C800/D000 + 27,600..32,767), about +13K, but untested on real hardware. Recommendation: mono at 256K (58.8K free),
  colour at 512K+ (or the FP=c build at 256K with 25K free).

Load time (MAME, B disk interleave 2; 1:1 interleave was 45.8 s; 3 = 25.3 s, 4 = 26.9 s; boot disk A also at 2 saves about 30 s of boot)
- New city, colour 256K: 17.0 s (tiles 6.3 s + generate). haight file: 21.9 s (city file load + initial scans 14.4 s, tiles 7.0 s).
  Mono tiles 3.3 s. Boot-to-map with `MPTIPC` in AUTOEXEC and both disks at interleave 2: about 55-60 s (was 140 s).
- Real disks must be written with interleave 2 (the MAME images use IMD; `tipc/img2imd.py RAW OUT.imd 2`). Use `IL=` (empty) for 1:1.
- Remaining time is CPU (loadFile + initial map scans), not the floppy.

## Steps 6 and 7: save/load and budget/evaluation/graph screens (2026-10-23, America/Chicago)

### Save / load (original .cty format)
- Keys: `S` save (prompt "Save city as: name_"; Enter = save, Esc = cancel), `L` load (prompt "Load city: name_"). Default drive is the current drive; `B:NAME.CTY` etc. is accepted. Command line: `mptipc city.cty ...` cold-loads a city.
- Format: 27,120 bytes (6 history arrays + MiscHis of 240 shorts, then 12,000 big-endian map shorts). Funds, date, tax rate, flags and police/fire/road percent live in MiscHis exactly as in the original, so saves move both ways unchanged. The loader also accepts the 99,120/219,120 byte variants, as the original does.
- Errors handled (message on the status line, game continues): file not found, not a city file (bad size), disk full (pre-check with `_dos_getdiskfree` before writing, partial file removed after a failed write), write protect/other I/O, out of memory (distinguished from I/O errors).
- `s_fileio.c` (shared with the Linux build): `_save_short`/`_save_long` swap back in place even when fwrite fails (otherwise a failed save corrupted the live city), fclose on the bad-size path, map read/write checks are `== 0`, `saveFile` returns 0 when fclose fails.
- `s_alloc.c: clearSimArrays()` zeroes the derived sim arrays before an in-game load. `driver.c` argv[5] = save filename (reference side of the comparison).
- Load runs the original DoSimInit, whose init scan alters a few map tiles/header words versus the file. So "identical" means: TIPC load == Linux reference load of the same file.

### Test matrix (MAME tipc, TI MS-DOS 2.11, 256K colour unless noted)
| Test | Result |
|---|---|
| Haight load + save, TIPC vs Linux reference | byte-identical files (map sum 7e4d1960) |
| TIPC-built town saved as T1.CTY: file map sum | d61a3a7a; Linux loads it and runs years; Linux load sum d69a3a72; cold TIPC reload of T1 = d69a3a72 (match) |
| Linux-saved city R1 loaded in TIPC | both 4447bf58, time 5040, funds 965279 |
| In-game: build town by keystrokes, pause, S A:T1.CTY, L | LOADSUM 482674cd = SAVESUM 482674cd, time 213, funds 14893; sim continues (time 227, pop 1800), no ENOMEM at 10.5K free |
| not found / not a city file / disk full (FILLA test hook) | clean error messages, state unchanged |
- Known limitation: an in-game load can give a map checksum that differs from a cold load of the same file in some sessions, due to stale globals that the original DoSimInit does not reset (RNG state etc.). Mitigated with clearSimArrays and sim_srand(1); not fully eliminated. Cold load (command line) is exact.
- Test hooks: args `C` (print SAVESUM/LOADSUM/QUITSUM checksum lines at quit), `P` (pause at start), `FILLA` option in mkdisks.py (fill A: to force disk full).

### Budget, evaluation, graphs
- `M` budget page: Up/Down selects a line (tax rate, auto-budget on/off, road/fire/police funding), Left/Right changes it (tax 0-20%, funding in steps of 5% from 0 to 100%), Enter/Esc returns. Shows requested vs funded amounts per service and the cash flow (taxes, spending, net, funds now, funds after the year). When auto-budget is off, DoBudget calls `BudgetHook` at year-end (only if population > 0), which opens the same page so the player sets the budget each year. Calc helpers in glue.c (`budget_calc`, `fund_pct`, `fund_pct_set`, `fund_amount`) use the same formulas as CollectTax; doubles stay in glue.c so mptipc.c does not pull in the 4K float printf.
- `V` evaluation page: public opinion (mayor yes/no %), top four problems with vote %, population, net migration, assessed value, category (Village..Megalopolis), game level, overall score and its change. Any key returns.
- `J` graphs page: two pages (Tab switches): Residential/Commercial/Industrial and Money/Crime/Pollution; `1` = 10-year, `2` = 120-year range (the history arrays); Esc returns. Direct pixel writes into the planes. The money graph of a brand-new city has no history (arrays are zero).
- `U` is now the stadium key (H freed for help). Help text must stay <= 24 lines.
- All pages are modal text-layer + `gfx_clear` pages, work in colour and mono. Screenshots: shots/step7_{color,mono}_{budget,eval,graph_*}[_crt].png.

### Memory (free heap after tiles+city, before any printf)
- 256K colour, haight: 10,512 bytes (was 18.4K before step 6/7; code grew 146,160 -> 152,368 bytes and the DGROUP arena was re-tuned: NEAR_BYTES 9600, ARENA_BYTES 10100). New city 10,528. Load time 22.5 s with haight (spec target <20 s was for step 5; 17.1 s for a new city).
- 256K mono, haight: 44,512 bytes free.
- STACK=1280 (default in build.sh): needed so the near heap can hold FILE structs for fopen; STACK=1792 gave ENOMEM on fopen. Measured stack use 770 bytes.
- `FP=c ./build.sh` (-fpc, no 8087 emulator) frees about 6.5K but breaks checkall bit-exactness (radial city diverges after 1926), so -fpi stays the default.
- Moving constants to code (-zc) was net neutral and was reverted. A printf anywhere costs an 8K stdout buffer, so measure memory before printf.
- No gating was needed: all new screens fit at 256K colour.

## Step 7 sounds and the in-game load fix (2026-10-23, America/Chicago)

### Sounds (tipc/snd.asm, about 1.4K of the 256K colour budget)
- Output: TIPC speaker = 8253 counter 0 (divisor = 1.25 MHz / Hz, port 17h control 36h, port 14h data) gated by port 00h bit 0 (SPKEN); the ROM's shadow of port 00h at [ROMDS:0] is kept in step (ROMDS = ES from INT 48H AH=9). The ROM routines INT 48H AH=2/3/4 are used only by the PoR port; Micropolis writes the ports itself because the sequencer runs in interrupt context.
- Timing: the ROM calls INT 58H on every 25 ms timer tick (TechRef p. 4-6; default is a dummy). snd.asm chains a hook there (old vector saved and chained with a far jump; restored by `asm_snd_exit` in `quit_now`). So a sound steps on in the background and never waits for the sim, the disk or the screen: an earlier polled version (checked from the main loop and the MapScan yield) stretched sounds by up to 1.9 s because PTLScan/CrimeScan/PopDenScan do not yield. The hook costs about 10 cycles on idle ticks and under 200 cycles on active ticks (about 0.15% of a 5 MHz tick). A crash that skips quit_now would leave the vector pointing into freed memory: reboot or run any program that exits normally. (A user timer node at 0:0020 was rejected: the ROM calls its callback with a near call into the ROM segment.)
- Effects (8-byte segments: divisor sweep d0..d1 over n steps of 25 ms, optional random wobble from its own LCG, never the sim RNG): 0 click (1500 then 2200 Hz, 50 ms), 1 bulldoze (180 to 110 Hz rasp, 150 ms), 2 error (two 260 Hz buzzes, 250 ms), 3 chime (660/880/1320 Hz, 300 ms; save, load, sound on), 4-6 honk-honk low/med/high (280/380/500 Hz, 250 ms), 7 siren (500-900 Hz sweeps x4, 1.2 s), 8 monster (growl sweeps with wobble, 1.2 s), 9 explosion high (900 to 120 Hz, 400 ms), 10 explosion low (220 to 40 Hz, 600 ms).
- Hook-up: `MakeSound()` in glue.c (TIPCUI builds) calls `snd_event()` with the original's event names (Explosion-High/Low, Siren, HonkHonk-Low/Med/High, Monster), so tool.c explosions and the s_msg.c disaster/traffic messages sound exactly where the original made them. mptipc.c adds click/bulldoze on a successful tool, error on a refused one, chime on save/load/sound-on. Clicks never interrupt or queue; other effects queue (two deep) behind a playing one; a real effect preempts a click. `O` toggles sound (not stored in the .cty: UserSoundOn is left alone so saves stay byte-identical to the reference).
- Not done: sprites (helicopter, train, plane, ship), so no chopper/heavy-traffic sounds (the original code never calls MakeSound for them; the Tcl UI did).
- Test hook: arg `T` plays all 11 effects 0.6 s apart through the same paths (`MakeSound` for the event ones) and prints `SND played 11`. In-game demo script: sound/ingame_keys.txt (goto, road, bulldoze, coal plant, error, explosion, mute, chime).
- Recordings (MAME `-wavwrite`, 48 kHz): sound/wav/*.wav (one per effect), sound/micropolis_tipc_sounds.mp3 (all 11, 10.9 s, 130 KB), sound/micropolis_tipc_ingame.mp3 (in-game events, 26 s, 310 KB), sound/effects_spectrogram_pitch.png (spectrogram plus zero-crossing pitch track for each effect), tools sound/split.py, spec.py, sheet.py. Measured lengths match the design to within one 25 ms tick in both colour and mono (click 54, bulldoze 130, error 235, chime 282, honks 250, siren 1205, monster 1203, explosions 383/605 ms).
- Memory at 256K, haight: colour 8,944 B free (was 10,512 before sound, 18.4K before step 6), mono 42,944 B. `atexit()` alone cost 8K (it pulls in the stdio exit chain), hence the explicit unhook in quit_now.

### In-game load now equals a cold load
- Cause: the original loader does not reset state left by the previous city. Found by building `reloadtest.c` (Linux; load A, play, reset, load B, play, against a fresh start on B) and diffing every global in .data/.bss (`rt/`, with an oracle that restores single globals from a startup snapshot and bisects). The globals that change the result of the load's first scan: NewPower, TrafficAverage, CCx2, CCy2, NeedChurch, NeedHosp, and NoDisasters (startup value 0 before the load).
- Fix: `ResetForLoad()` in s_alloc.c clears the sim arrays, seeds the RNG as at startup (`sim_srand(1)`) and zeroes those scalars (plus the other leftovers listed there). `do_load` calls it, no longer reseeds from the clock after the load, and keeps the player's speed setting instead of the file's (speed changes scan frequencies).
- Linux proof: 24 cities in a cycle, A played 6 or 15 years, in-game load of B, 12 or 25 more years, with and without random disasters: 96 of 96 identical to the cold run (`sh rt/matrix.sh 6 12`, `sh rt/matrix.sh 15 25`; tools in rt/: rt.sh, bisect.py, dumpdiff.py, union.py).
- MAME proof (256K, colour and mono): haight cold `CITY.CTY C Y2` vs a new city, `L`, then Y2 (`Yn` test hook = quit n years after the start or the last load): both QUITSUM f78aab7b, time 4935, funds 982594, pop 214380, and the same map sum as the Linux reference at T4935. Keystroke-built town (tipc/town_keys.txt: plant, three zones, police, fire, roads, wire, park; saved with S, loaded with L, Y3): colour LOADSUM 749a334f, QUITSUM 8a4565bd T232 funds 16085; mono 8a4565ad / 8a4565bd T239 funds 16094; each equals the cold run on the saved file and the Linux reference.

### Box notes
- After a box reset the tools were missing: nasm (`~/.local/bin/nasm`, extracted from the nasm.us rpm), GNU make (`~/.local/bin/make`, built from the GNU tarball), libSDL2_ttf for MAME (`LD_LIBRARY_PATH=/home/box/.local/sdl/usr/lib/x86_64-linux-gnu`), and the MAME ROM path (`run_mame.sh` now uses `ROMPATH`, default /workspace/t/rp with tipc/u62.bin u63.bin u25.bin copied from /workspace/tipc/roms).

## Step 8: sprites and settings (2026-10-23, America/Chicago) CHECKPOINT 12:45
### Done
- `s_sprite.c`: the original w_sprite.c logic (train, helicopter, airplane, ship, monster, tornado, explosion; bus unused), static pool of 12, near pointers (`SPRNEAR`). Generators return before any RNG draw when a type is not allowed, so sprites-off runs are bit-identical to the old baseline.
- Switches: `SpritesOn` (traffic sprites), `DisasterSprites` (set by `SetDisasters()` only; monster/tornado/explosions allowed when either is on). When sprites are off the sim creates and moves none (MoveObjects returns at once).
- Art: `art/tools/mksprites.py` converts the GPL xpm sprites at build time (SPR8.BIN 32,332 B colour, SPR1.BIN 16,204 B mono; 32x18 and 48x27 after the 9/16 height scale). Loaded by `load_sprites()` in priority order TRA, SHI, AIR, COP, EXP, GOD, TOR only if free memory >= bytes + 6144 B reserve; a type without art is never created.
- Drawing: tile pass, then `asm_spr_blit` (tblit.asm) masked merge per sprite; moved/vanished sprites dirty their old tiles; unchanged sprites with no repainted tiles under them are skipped.
- Two builds: MPTIPC.EXE (full, tbuild/) and MPTIPCL.EXE (`NOSPR=1 sh tipc/build.sh`, tbuild_lean/, `-DNOSPRITES`, no sprite code) for colour 256K, where sprite code (~7K) plus art cannot fit. The settings page is in both.
- Settings page (Tab or I): 1 Sprites, 2 Sound, 3 Disasters (default OFF), 4 Auto-bulldoze, 5 Game speed. Up/Down, Left/Right/Space/Enter, 1-5, Esc/Tab to leave. O toggles sound, X disasters, +/- speed (also saved). Status line shows SPR when sprites are on. MP.CFG (8 bytes, 'M','P',1,sprites,sound,nodis,autobull,speed) is written on the current disk with `_dos_creat`; .cty files unchanged. Args NOSPR / SPR override it.
- Sound: effect 11 (chopper) for "HeavyTraffic".
- Test hooks: args SD (demo of all 7 sprite types), SF1/SF2/SF3 (find rail/channel/airport, cursor there, spawn train/ship/plane+helicopter); `CFGFILE=` env in mkdisks.py puts a prepared MP.CFG on B:.
### Measured (MAME tipc)
- Sprite blit after the rewrite: 15 ms per 48x27 sprite colour, 9 ms mono (was 45 / 20). 7-sprite demo: FULLDRAW 235 ms colour -> 340 ms with sprites (mono 105 -> 170); unchanged sprites cost 15/10 ms per refresh.
- Sim speed, 128 phases, haight: colour 512K 5120 cs sprites on (no sprite in view) vs ~5070 off; with the 7-sprite demo 5750 cs (+12%). Mono 256K 4960 cs off, 5470 with the demo (+10%).
- Free memory: colour 512K 256,832 B after tiles+city, 224,448 after 32,256 B art (all 7 types). Mono 256K haight 28,688 B after tiles+city, 12,432 B after 16,128 B art (all 7 types). Colour 256K lean build: 4,992 B free, no sprites (was 8,944 B before step 8; the settings page, config and sound cost the rest).
- Save/load at 256K: lean colour OK (SAVESUM = LOADSUM, 4,992 B free), full mono OK, full colour 512K OK.
- Determinism: `./checkall.sh` (sprites off) 24/24 Linux and DOS; `./checkall_sprites.sh 30`: Linux and DOS (emu2) S 24/24 and SD 24/24 against ref30s/ref30sd.
- Settings persistence in MAME: toggles written to B:MP.CFG, a prepared MP.CFG (sprites off) is read at start.
### Screenshots: shots/step8_*.png
### Not done / limits
- No sprites in the colour 256K build (MPTIPCL). DropFireBombs and DoEarthQuake still stubs. Bus sprite unused.
- In-game load == cold load with sprites on: Linux matrix `SPRITES=1 sh rt/matrix.sh 6 12` and `15 25` both pairs=48 bad=0 (also sprites off 48/0). Needed three fixes: ResetForLoad now clears SpritesOn/DisasterSprites and the sprite list (the load scan must run with sprites off, as at startup), `absDist` (stale GetDir distance the plane code reads) is reset in DestroyAllSprites, and do_load re-applies the sprite setting afterwards (new_city also clears sprites). Not re-run in MAME with sprites on (the MAME load script did not get past the paused start in my attempts); MAME save/load itself passes (below).
- Final numbers after these changes: lean colour 256K 3,936 B free after tiles+city (was 4,992 before the load fix; it still saves and loads: SAVESUM = LOADSUM), full mono 256K 28,656 B -> 12,400 B after the art; colour 512K 256,800 -> 224,416 B. Sim 128 phases haight: mono 256K 5030 cs sprites on vs 4960 off (+1.4%), colour 512K 5120 cs on (lean colour 256K, no sprites: 5040).
- Screenshots: step8_{train,ship,plane}_{color,mono}_crt.png are 3x zoomed CRT crops around the sprite (the cursor box sits on the spawn tile, which hides the ship a bit), *_full_crt.png the whole screen; demo of all 7 types step8_demo_*; settings step8_settings_{color,mono}_crt.png; status line without SPR step8_status_spr_off_color_crt.png.
- Step 8 final (2026-10-23 12:58 PM America/Chicago): DOS sprites-on checkall S and SD 24/24 on the final code; sprites-off checkall 24/24 (Linux and DOS). Design doc updated (Step 8 bullets after the sounds step, read back OK).

## Step 9: release (2026-10-23, America/Chicago) CHECKPOINT 13:25
- Decision (Will): colour needs 512K, mono runs on 256K. The lean colour 256K build (MPTIPCL, NOSPRITES) is dropped from the release: tipc/build.sh now builds only MPTIPC.EXE; mkdisks.py and .gitignore lost the LEAN paths (the NOSPRITES #ifdef code in glue.c/mptipc.c is left in the source but not built).
- Colour with too little RAM: the program already stopped before the map; the message is now "Not enough memory. Micropolis in color needs 512K of RAM. With 256K, start it in mono: MONO" (mono: needs 256K; a missing tile file has its own message). Tested in MAME colour at 256K.
- Title banner (text layer, shown while loading; the string is `far` so DGROUP is not touched): name, credits line, Micropolis trademark attribution.
- Release tooling in release/: README.TXT, STICKER.TXT, diskdefs.cfg (Greaseweazle tipc.360/tipc.720 = ibm.360/ibm.720 with interleave=2), build_disks.py (program and cities disks without DOS; BOOT360.img only from the user's own TI MS-DOS image via --dos; --imd for 2:1 interleave copies), mkzip.py (reproducible zip from git ls-files + tbuild/), tipc/run_release_mame.sh (boot test of a package in MAME).
- Disks: MICRO360 (program + 2 cities, 66,560 B free = 2 saves), MICRO720 (12 cities, 141,312 B free = 4 saves), CITIES720 (all 24), BOOT360 (315,392 B free).
- COLOR.BAT / MONO.BAT on the program disks start MPTIPC (MONO passes M); AUTOEXEC.BAT on the boot disk switches to B: and echoes the two commands.
- Text-start fix: DOS text scrolled the ROM's display start, so the game's status line showed mid-screen. video_init reads the ROM display-begin pointer (INT 48H AH=17H), saves the 2K of DOS screen text into a DOS block (_dos_allocmem 128 paras) and sets CRTC R12/R13 to 0; video_exit puts the text and the start back, so the DOS prompt comes back as it was (shots/release_mono256_720_after_quit_crt.png).
- Test-script finding: MAME cannot write to IMD images, so the save test needs a raw A.img (AF=A.img). Save to A: in the package (mono 256K, 720K program disk, `s` then `a:mycity.cty`) wrote MYCITY.CTY, 27,120 B, to A:.
## Step 9 final (2026-10-23 ~2:05 PM America/Chicago)
- Zip: /workspace/t/micropolis_tipc_RC1.zip (252 files). Unzipped to a fresh dir, built from scratch with tipc/build.sh: MPTIPC.EXE byte-identical to the shipped bin/MPTIPC.EXE.
- Fresh-dir checkall: sprites off 24/24 (Linux vs baseline, DOS years=30 pass=24 fail=0); sprites on: Linux S and SD all match, DOS S 24/24, DOS SD 24/24.
- MAME boot tests of the package (TI DOS boot disk built locally from our scroll.img; not shipped), 256K mono and 512K colour, program disk 360K (raw image in B:) and 720K (IMD, interleave 2): AUTOEXEC menu, MONO/COLOR LINECITY.CTY, banner, map with sprites running, Quit prompt, DOS screen restored. All four pass.
- Screenshots: shots/release_{mono256,color512}_{360,720}_map_crt.png, release_color512_360_banner_crt.png, release_mono256_720_after_quit_crt.png.
- Not done: nothing was written to or booted on real disks or a real TIPC (interleave 2 and the Greaseweazle formats are untested); no ready-made bootable disk (TI MS-DOS not shipped; build_disks.py --dos makes it); load of a saved city was not re-tested in the package (save was).
