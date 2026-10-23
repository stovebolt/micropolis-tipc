MICROPOLIS FOR TIPC
A city builder for the TI Professional Computer
Release candidate 1, October 23, 2026
=============================================================================
Code by Shurr Nuff (AI assistant), produced by Will (stovebolt)
Flying Horse Labs


WHAT IT IS
Micropolis for TIPC is a port of Micropolis to the TI Professional Computer
(TIPC).  Micropolis is the open source version of the 1989 city simulator
that was first published as SimCity.  You zone land for houses, shops and
factories, lay roads, rails and power lines, pay for police and fire
protection, set taxes, and try to keep a growing city out of trouble.

This is a real port, not a look-alike.  The simulation is the original C
code, rebuilt for the 8088, and it is checked against a reference build: the
same city gives the same numbers year after year (24 sample cities, 30 game
years each, on both the PC reference and the TIPC build).  What is new is the
TIPC part: the map is drawn straight into the TIPC's three graphics planes,
everything is run from the keyboard (the TIPC has no mouse), and the PC
speaker makes the sound effects.

This package is a port of Micropolis, the open source release.  It is not
SimCity, it is not from or endorsed by Electronic Arts, and it is not an
official Micropolis release.


WHAT YOU NEED
- A TI Professional Computer (8088, 5 MHz) with two floppy drives:
  both 360K, or both 720K (TI 80-track).
- Memory:  COLOR needs 512K of RAM.  MONO needs 256K.
  If you start COLOR with too little memory the program stops at once and
  says so ("Micropolis in color needs 512K of RAM"); nothing is damaged.
- Your own bootable TI MS-DOS 2.11 disk.  TI's DOS is not ours to give out, so
  the boot disk is built from YOUR disk (see HOW TO MAKE THE DISKS).
- To write the disks: a Greaseweazle or any other tool that writes 360K and
  720K floppy images.  A modern PC with Python 3 for build_disks.py.

COLOR or MONO?  COLOR is the full 8-color picture and needs 512K.  MONO uses
one graphics plane (black and white, dithered), needs only 256K, and is a bit
faster.  Both play the same game.  Pick the one your memory allows.

Tested in MAME (the "tipc" driver, 8088 at 5 MHz): COLOR at 512K and MONO at
256K, with the 360K and the 720K program disks.  It has not been tried on a
real TIPC yet.


WHAT IS IN THE PACKAGE
  README.TXT, STICKER.TXT      this file, and text for disk labels
  COPYING                      GNU General Public License, version 3
  MicropolisPublicNameLicense.md   the Micropolis Public Name License
  disks/MICRO360.img           program disk for 360K drives (2 sample cities)
  disks/MICRO720.img           program disk for 720K drives (12 sample cities)
  disks/CITIES720.img          all 24 sample cities
  build_disks.py, fat12.py, img2imd.py   builds the boot disk from your TI MS-DOS disk
  diskdefs.cfg                 Greaseweazle formats with 2:1 interleave
  bin/                         the program and its art files
  src/                         the complete source, build scripts and tests
  docs/                        a few screenshots and the sound recording


HOW TO MAKE THE DISKS
You make two disks (three if you want every sample city): a BOOT disk and a
PROGRAM disk.

1. The BOOT disk.  It holds TI MS-DOS and our AUTOEXEC.BAT, so it has to be
   made from your own TI MS-DOS 2.11 boot disk image (360K):
      python3 build_disks.py --dos tidos.img
   This writes disks/BOOT360.img.  (The other three images are already in
   disks/ and hold nothing from TI.)
2. The PROGRAM disk: disks/MICRO360.img for 360K drives, disks/MICRO720.img for
   720K drives.  CITIES720.img is an extra with all 24 sample cities.

Writing the disks with a Greaseweazle (gw):
      gw write --format=ibm.360 disks/BOOT360.img
      gw write --format=ibm.360 disks/MICRO360.img
      gw write --format=ibm.720 disks/MICRO720.img
      gw write --format=ibm.720 disks/CITIES720.img
   Put a blank disk in the drive each time and use a high-density-free
   (double density) disk.  If your Greaseweazle drive is an 80-track drive
   writing a 40-track 360K disk, add  --tracks=step=2  (see the Greaseweazle
   wiki, "Specifying Tracks").  Use --drive=B for a second drive.

Interleave (faster loading).  The TIPC is slow and reads a disk better with
every other sector skipped (2:1 interleave).  The plain ibm.360 and ibm.720
formats write the sectors one after the other, which works but loads slower.
diskdefs.cfg adds two formats with 2:1 interleave:
      gw write --diskdefs=diskdefs.cfg --format=tipc.360 disks/BOOT360.img
      gw write --diskdefs=diskdefs.cfg --format=tipc.720 disks/MICRO720.img
   (Use tipc.360 for BOOT360 and MICRO360, tipc.720 for MICRO720 and
   CITIES720.)  For an emulator, "python3 build_disks.py --dos tidos.img --imd"
   writes .imd copies that already have 2:1 interleave (in MAME the 720K disks
   were tested that way, and the 360K disks as the plain .img files).
   Writing to real disks has not been tested.

Other tools work too (for example ImageDisk, a Gotek with the images, or a PC
with a matching drive).  The images are plain 512-byte-sector FAT12 disk
images: 360K is 40 tracks, 2 sides, 9 sectors; 720K is 80 tracks, 2 sides, 9
sectors, in TI's format.  Boot from the 360K BOOT disk.


HOW TO START
1. Put the BOOT disk in A: and the PROGRAM disk in B:.  Switch on.
2. The boot disk switches to B: and shows a few lines.  Type
      COLOR            if you have 512K or more
      MONO             if you have 256K
   and press Enter.  To start with a sample city instead of a new one:
      COLOR LINECITY.CTY       or       MONO RADIAL.CTY
3. A title appears while the city and art load (a minute or so at 5 MHz, a bit
   longer for a big city), then the map.  Without a city name you get a new
   random city, which starts empty.  Z makes a new one later.
4. Press H or ? at any time for the key list.


KEYS
  Arrow keys   move the cursor (the map scrolls when you reach the edge)
  Enter        build with the current tool, or query the tile (Query tool)
  D            drag mode: build on every cursor move (roads, wires, rails, bulldozer)
  Tools (the letter or the F key)                          cost
    Q  F1  query       B  F2  bulldozer $1     R  F3  road $10    T  F4  rail $20
    W  F5  wire $5     K  park $10             1  F6  residential $100
    2  F7  commercial $100                     3  F8  industrial $100
    P  F9  police station $500                 F  F10 fire station $500
    U  stadium $5,000   C  coal power plant $3,000   N  nuclear plant $5,000
    E  seaport $3,000   A  airport $10,000
  G            go to a map position (X Y)
  Space        pause           - and +   slower and faster
  M            budget and taxes (Up/Down, Left/Right, Enter or Esc to leave)
  V            evaluation (score, problems, votes)
  J            graphs (population, money, R/C/I, history)
  S and L      save and load a city (a name prompt; see SAVING)
  Tab or I     settings (see below)
  O            sound on/off      X   disasters on/off
  Z            new city          Esc   quit (asks first)
  H or ?       the key list

The status line at the bottom shows the date, your funds, the population, the
residential/commercial/industrial demand, the tool, the speed, and SPR when
sprites are on.  A zone needs power (a wire path from a power plant) and a
road.  Start with a power plant, some wire, a road, and zones next to it.


SETTINGS
Press Tab (or I).  Up and Down choose a line, Left, Right, Space or Enter
change it, or press its number.  Esc or Tab leaves.
  1  Sprites     ON/OFF   the moving trains, ships, planes and helicopters
  2  Sound       ON/OFF
  3  Disasters   OFF/ON   fires, floods, tornados, monsters, crashes (default OFF)
  4  Auto-bulldoze ON/OFF clear rubble when you build
  5  Game speed  SLOW/MED/FAST
The choices are saved in a small file, MP.CFG, on the disk you started from,
and are used the next time.  (It does not change your saved cities.)  The
words NOSPR or SPR after COLOR or MONO override the saved sprite setting once,
for example  MONO NOSPR.


SPRITES
Trains run on your rails, ships sail the channels, planes use your airport and
a helicopter watches the traffic.  With disasters on you also get a monster, a
tornado and explosions.  Sprites cost a little speed (about 1% while none are
on screen, more when many are, mostly the drawing) and use 16K (MONO) or 32K
(COLOR) of memory for their pictures.  Turn them off on the settings page and
the simulation does not create or move any at all.  If memory is tight at
start, the program loads the pictures it can, in the order train, ship, plane,
helicopter, explosion, monster, tornado, and keeps 6K free.  The settings page
tells you what was loaded.


SOUND
The TIPC speaker plays 12 sound effects: build clicks, bulldozer, error buzz,
a chime, honks, siren, explosions, the monster, and a chopper for the
helicopter's traffic report.  O turns the sound off and on.  The sounds run
from the timer so they never slow the simulation.


SAVING AND LOADING
S asks for a file name, L asks for one.  The prompt starts with the last name;
type a new one (for example A:MYCITY.CTY) and press Enter, Esc cancels.
A saved city is 27,120 bytes, the same file format as the original Micropolis,
so a city saved here is a normal Micropolis city file, and the sample
cities are the original ones.  Put the drive letter first to choose the disk:
  - A: is the boot disk (about 310K free: eleven cities).  Do not
    write-protect it.
  - B: is the program disk: the 360K one has room for two cities, the 720K
    one for four.
Loading takes about 15 seconds.  If a disk is full the game says so and
nothing is lost.  Never take a disk out while the disk light is on.
The sample cities are on the program disks (and all 24 on CITIES720): swap
CITIES720 into B: and type  B:KYOTO.CTY  at the L prompt.


SPEED
A TIPC runs at 5 MHz, so this is slow compared with a PC.  In MAME one game
month of a small city takes a few seconds, a large city (HAIGHT) more than
20.  Sprites off is a little faster.  The "faster" speed is the default.


SAMPLE CITIES
LINECITY, RADIAL (both disks); also on MICRO720: ABOUT, KYOTO, HAIGHT, KOBE,
BRUCE, FREDS, HAPPISLE, SENRI, SOUTHPAC, WETCITY.  CITIES720 has all 24:
ABOUT, BADNEWS, BLUEBIRD, BRUCE, DEADWOOD, FINNIGAN, FREDS, HAIGHT, HAPPISLE,
JOFFBURG, KAMAKURA, KOBE, KOWLOON, KYOTO, LINECITY, MED_ISLE, NDULLS, NEATMAP,
RADIAL, SENRI, SOUTHPAC, SPLATS, WETCITY, YOKOHAMA.  They are the cities that
come with the open source Micropolis.


KNOWN LIMITS
- Not tried on real TIPC hardware yet.  Disk writing and the sound have only
  been checked in MAME.
- Colour needs 512K.  There is no 256K colour version.
- An earthquake does its damage, but the screen does not shake.  The scenario
  disasters (such as the Hamburg fire bombs) are not there: there is no
  scenario menu.
- No mouse, no network, no scenarios menu: load a sample city instead.


CREDITS AND LICENSES
Code by Shurr Nuff (AI assistant), produced by Will (stovebolt).
Flying Horse Labs.

Micropolis for TIPC is free software under the GNU General Public License,
version 3 (COPYING), as is the Micropolis it comes from:
  Micropolis, Unix Version.  Copyright (C) 1989 - 2007 Electronic Arts Inc.
  Modified for the One Laptop Per Child program; open source release by Don
  Hopkins (https://github.com/SimHacker/micropolis).  The game's artwork and
  sample cities come from that release.
The notice that goes with the Electronic Arts code (GPL section 7 additional
terms) is kept at the top of every original source file: no trademark or
publicity rights are granted; the trademark SimCity is not used here; you may
not claim any affiliation with Electronic Arts; modified versions must be
marked as such and not be identified as the original program; and the program
is provided as is, without warranty.  THIS IS A MODIFIED VERSION: it is a port
to the TI Professional Computer, not the original program.

Micropolis is a registered trademark of Micropolis Corporation (Micropolis
GmbH) and is licensed here as a courtesy of the owner.  www.micropolis.com
The MICROPOLIS PUBLIC NAME LICENSE (MicropolisPublicNameLicense.md) is part of
this package and applies to the use of the name.  The name is used here only
for the open source game and its source code.

TI, TI Professional Computer and TI MS-DOS belong to Texas Instruments;
MS-DOS to Microsoft.  They are named only to say what this runs on.  No TI or
Microsoft code is in this package.

SOURCE OFFER
The complete source for this program is in the src folder of this package: the
simulation, the TIPC front end, the sound and drawing code in assembly, the art
converters, the build scripts and the tests.  Build it with Open Watcom 2
(16-bit DOS, large model), NASM and Python 3 (see src/NOTES.md and
src/tipc/build.sh).  If you have only the disks or the program and not this
package, you may ask whoever gave them to you for the source: under GPL
version 3, section 6, they must provide it.  The upstream source is at
https://github.com/SimHacker/micropolis (the folder micropolis-activity).
