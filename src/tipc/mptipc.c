/* mptipc.c: Micropolis for TIPC, map view front end (step 4).
 * Code by Shurr Nuff (AI assistant), produced by Will (stovebolt), Flying Horse Labs.
 * GPL v3 with the Micropolis Public Name License; see COPYING.
 *
 * Usage: MPTIPC [city.cty] [N[seed]] [M] [B]   (no city file: a new generated city)
 *   M  mono: one plane, TILES1.BIN (default is 8 colours, TILES8.BIN)
 *   B  benchmark: no keyboard, times redraws, prints to stdout (redirect it)
 *
 * Screen: 720x300 graphics.  The map window is 45 x 32 tiles of 16x9 pixels = 720x288, which is
 * exactly 24 text rows; the 25th text row (pixel rows 288..299) is the status line, drawn by the
 * TIPC text layer on top of the graphics.
 * Keys: arrows move the cursor (the window pages when it leaves the screen), SPACE pauses, ESC quits.
 * Hardware facts (TechRef sec. 2-3, see ports/goldbox and gfx/ in this project):
 *   planes A/B/C at C000/C800/D000, 92 bytes per row, palette latches DF010 blue / DF020 green / DF030 red,
 *   DF820 b6 text on, text RAM DE000 with attribute latch DF800, keyboard through INT 4Ah. */
#include "sim.h"
#include <dos.h>
#include <fcntl.h>
#include <io.h>
#include <i86.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>

int SimFrame(void);
int initMapArrays(void);
extern short SimSpeed;
extern void (*MapScanYield)(void);
int simdbg = 0;
unsigned long map_sum(void) { return 0; }   /* only used by SIMTRACE builds */

#define VID(o) (*(volatile unsigned char far *)MK_FP(0xDF00, (o)))
#define TXT(o) (*(volatile unsigned char far *)MK_FP(0xDE00, (o)))
#define TCOLS 45
#define TROWS 32
#define TH 9
#define WIN_W 45
#define WIN_H 32

extern unsigned __cdecl asm_draw_map(unsigned mapoff, unsigned shownoff, unsigned tileseg, unsigned pstep);
extern unsigned __cdecl asm_draw_map1(unsigned mapoff, unsigned shownoff, unsigned tileseg, unsigned pstep);

static unsigned shown[TROWS * TCOLS];
static unsigned tileseg, pstep;
static int mono = 0, bench = 0;
static int wx, wy, cx, cy;          /* window origin and cursor, in map tiles */
static int cur_on = 0, cur_tx, cur_ty;
static int paused = 0;
extern short HaveLastMessage;
static unsigned long tiles_drawn = 0, ui_draws = 0;
static const unsigned planeseg[3] = { 0xC000, 0xC800, 0xD000 };

/* ---- time: DOS clock in hundredths of a second since midnight ---- */
static unsigned long now_cs(void)
{
  struct dostime_t t;
  _dos_gettime(&t);
  return ((unsigned long)t.hour * 60UL + t.minute) * 6000UL + (unsigned long)t.second * 100UL + t.hsecond;
}
static unsigned long elapsed(unsigned long since)
{
  unsigned long n = now_cs();
  return (n >= since) ? n - since : n + 8640000UL - since;
}

static unsigned dos_free_paras(void) { unsigned seg = 0; _dos_allocmem(0xFFFF, &seg); return seg; }

/* ---- sound: snd.asm plays the effects from the ROM's 25 ms timer tick (INT 58H), so nothing here waits or polls ---- */
extern void __cdecl __far asm_snd_init(void);
extern void __cdecl __far asm_snd_exit(void);
extern void __cdecl __far asm_snd_play(unsigned id);
extern void __cdecl __far asm_snd_stop(void);
extern unsigned __cdecl __far asm_snd_busy(void);
enum { S_CLICK = 0, S_DOZE, S_ERR, S_CHIME, S_HONKL, S_HONKM, S_HONKH, S_SIREN, S_MONSTER, S_EXPH, S_EXPL, S_CHOP };
static unsigned char snd_on = 1;
static unsigned long snd_plays;
static int snd_test, snd_tid; static unsigned long snd_tt;
static void snd_stop(void) { asm_snd_stop(); }
static void snd_play(int id) { if (snd_on) { snd_plays++; asm_snd_play(id); } }
/* MakeSound() in glue.c lands here: the original's event names ("Explosion-High", "Siren", "HonkHonk-Med", "Monster ...") */
void snd_event(const char *b)
{
  switch (b[0]) {
  case 'E': snd_play(b[10] == 'H' ? S_EXPH : S_EXPL); break;
  case 'S': snd_play(S_SIREN); break;
  case 'M': snd_play(S_MONSTER); break;
  case 'H': if (b[1] == 'e') { snd_play(S_CHOP); break; }   /* "HeavyTraffic": the helicopter's traffic report */
            snd_play(b[9] == 'L' ? S_HONKL : b[9] == 'M' ? S_HONKM : S_HONKH); break;
  }
}
extern int MakeSound(char *a, char *b);
static void snd_testtick(void)      /* test hook (arg T): every effect once, 0.6 s apart; the event ones through MakeSound() like the sim */
{
  static char *const ev[12] = { 0, 0, 0, 0, "HonkHonk-Low", "HonkHonk-Med", "HonkHonk-High", "Siren", "Monster -speed 3", "Explosion-High", "Explosion-Low", "HeavyTraffic" };
  if (asm_snd_busy()) { snd_tt = now_cs(); return; }
  if (snd_tid >= 12 || elapsed(snd_tt) < 60) return;
  if (ev[snd_tid]) MakeSound("city", ev[snd_tid]); else snd_play(snd_tid);
  snd_tid++; snd_tt = now_cs();
}

/* ---- keyboard: TI keyboard DSR, INT 4Ah (AH=1 status, AH=0 read) ---- */
static int kbd_poll(void)
{
  union REGPACK r;
  memset(&r, 0, sizeof r); r.h.ah = 1; intr(0x4A, &r);
  if (r.w.flags & INTR_ZF) return -1;
  r.h.ah = 0; intr(0x4A, &r);
  return r.w.ax;
}
#define KEY_ESC   0x001B
#define KEY_UP    0x4800
#define KEY_DOWN  0x5000
#define KEY_LEFT  0x4B00
#define KEY_RIGHT 0x4D00

/* ---- video ---- */
static void gfx_clear(void)
{
  int p;
  for (p = 0; p < 3; p++) _fmemset(MK_FP(planeseg[p], 0), 0, 300U * 92U);
}
static void text_clear(void)
{
  unsigned o;
  VID(0x800) = 0x00;                               /* attribute: nothing shown */
  for (o = 0; o < 2000; o++) TXT(o) = ' ';
}
static void status_line(const char *s)
{
  unsigned i;
  VID(0x800) = 0x0F;                               /* char enable + white */
  for (i = 0; i < 80; i++) TXT(24 * 80 + i) = *s ? (unsigned char)*s++ : ' ';
}
static void crtc(unsigned char reg, unsigned char val) { VID(0x810) = reg; VID(0x812) = val; }
/* the ROM's display start offset (text RAM wraps at 2K; DOS moves it when the text scrolls) and a copy of the DOS screen text (2K in a
   DOS block) that goes back on exit */
static unsigned txt_start, txt_saveseg;
static void video_init(void)
{
  { union REGS r; r.h.ah = 0x17; int86(0x48, &r, &r); txt_start = r.x.dx; }   /* ROM AH=17H: physical display-begin pointer */
  if (!_dos_allocmem(128, &txt_saveseg)) _fmemcpy(MK_FP(txt_saveseg, 0), MK_FP(0xDE00, 0), 2048); else txt_saveseg = 0;
  crtc(12, 0); crtc(13, 0);                        /* our text rows are written from offset 0 */
  VID(0x820) = 0x40;                               /* text layer on, no vblank NMI */
  if (mono) { VID(0x10) = 0x02; VID(0x20) = 0x02; VID(0x30) = 0x02; }   /* pixel value 1 = white */
  else      { VID(0x10) = 0xAA; VID(0x20) = 0xCC; VID(0x30) = 0xF0; }   /* pixel value v = colour v */
  gfx_clear();
  text_clear();
  crtc(10, 0x20);                                  /* hide the text cursor */
}
static void video_exit(void)
{
  gfx_clear();
  if (txt_saveseg) _fmemcpy(MK_FP(0xDE00, 0), MK_FP(txt_saveseg, 0), 2048);   /* the DOS screen as it was */
  crtc(12, (unsigned char)((txt_start >> 8) & 0x3F)); crtc(13, (unsigned char)(txt_start & 0xFF));   /* give the ROM its display start back */
  VID(0x10) = 0xAA; VID(0x20) = 0xCC; VID(0x30) = 0xF0;
  VID(0x800) = 0x0F;
  crtc(10, 0x40);
}

/* ---- tiles ---- */
static int load_tiles(const char *name)
{
  int h; unsigned got, seg, paras, off; unsigned long size;
  unsigned char far *p;
  if (_dos_open(name, O_RDONLY, &h)) return 0;
  size = filelength(h);
  paras = (unsigned)((size + 15) >> 4);
  if (_dos_allocmem(paras, &seg)) { _dos_close(h); return 0; }
  p = (unsigned char far *)MK_FP(seg, 0);
  for (off = 0; off < (unsigned)size; ) {
    unsigned n = (size - off > 16384UL) ? 16384U : (unsigned)(size - off);
    if (_dos_read(h, MK_FP(seg + (off >> 4), off & 15), n, &got) || got == 0) { _dos_close(h); return 0; }
    off += got;
  }
  _dos_close(h);
  tileseg = seg;
  { unsigned nu = *(unsigned far *)(p + 6), th = *(unsigned far *)(p + 8);
    pstep = nu * th * 2 - th * 2; }
  return 1;
}

/* ---- cursor: a frame drawn with XOR (inverts the colours under it); n x n tiles for the big tools ---- */
static int cur_n = 1;
static void cursor_xor(int tx, int ty, int n)
{
  int pl, r, np = mono ? 1 : 3, k;
  for (pl = 0; pl < np; pl++) {
    unsigned char far *q = (unsigned char far *)MK_FP(planeseg[pl], (unsigned)(ty * TH * 92 + tx * 2));
    unsigned char far *b = q + (n * TH - 1) * 92;
    for (k = 0; k < n; k++) { ((unsigned far *)q)[k] ^= 0xFFFF; ((unsigned far *)b)[k] ^= 0xFFFF; }
    for (r = 1; r < n * TH - 1; r++) {
      *(unsigned far *)(q + r * 92) ^= 0x8000;
      *(unsigned far *)(q + r * 92 + (n - 1) * 2) ^= 0x0001;
    }
  }
}
static void cursor_hide(void) { if (cur_on) { cursor_xor(cur_tx, cur_ty, cur_n); cur_on = 0; } }

/* ---- tools: key, state, name ---- */
extern QUAD CostOf[];
int saveFile(char *filename);
void clearSimArrays(void);
void ResetForLoad(void);
int do_tool_xy(short state, short x, short y);
int GenerateSomeCity(int r);
extern char LastMessage[];
struct tooldef { char key; short state; const char *name; };
static const struct tooldef tools[] = {
  { 'Q', queryState, "Query" }, { 'B', dozeState, "Bulldozer" }, { 'R', roadState, "Road" }, { 'T', rrState, "Rail" },
  { 'W', wireState, "Wire" }, { 'K', parkState, "Park" }, { '1', residentialState, "Residential" },
  { '2', commercialState, "Commercial" }, { '3', industrialState, "Industrial" }, { 'P', policeState, "Police" },
  { 'F', fireState, "Fire dept" }, { 'U', stadiumState, "Stadium" }, { 'C', powerState, "Coal power" },
  { 'N', nuclearState, "Nuclear" }, { 'E', seaportState, "Seaport" }, { 'A', airportState, "Airport" }
};
#define NTOOLS (sizeof tools / sizeof tools[0])
static const unsigned char fkey_tool[10] = { 0, 1, 2, 3, 4, 6, 7, 8, 9, 10 };   /* F1..F10 -> tools[] */
static int tool_ix = 0;                                  /* current tool, index into tools[] */
static int drag = 0, speed = 3, prompt = 0, plen = 0, nodis = 1;
static int spr_pref = 1, autobull = 1;            /* settings: sprites on, auto-bulldoze on (Tab = settings page, saved in MP.CFG) */
static char pbuf[40], msg[56], simmsg[80], fname[40] = "CITY.CTY";
static unsigned long load_sum, save_sum; static long load_t, load_f; static int start_paused, csum_mode, have_load, have_save, pfresh;
static unsigned long msg_cs, simmsg_cs;
static int ystop; static long ystop_t;   /* test hook Yn: quit n years after the start or the last load */

static int tool_size(void)
{
  switch (tools[tool_ix].state) {
  case residentialState: case commercialState: case industrialState: case fireState: case policeState: return 3;
  case stadiumState: case powerState: case nuclearState: case seaportState: return 4;
  case airportState: return 6;
  }
  return 1;
}
static void cursor_show(void)
{
  int n = tool_size(), ox = cx - (n > 1), oy = cy - (n > 1);
  if (n > 1 && ox >= wx && oy >= wy && ox + n <= wx + WIN_W && oy + n <= wy + WIN_H) {
    cur_tx = ox - wx; cur_ty = oy - wy; cur_n = n;
  } else {
    cur_tx = cx - wx; cur_ty = cy - wy; cur_n = 1;
  }
  if (cur_tx >= 0 && cur_tx < WIN_W && cur_ty >= 0 && cur_ty < WIN_H) { cursor_xor(cur_tx, cur_ty, cur_n); cur_on = 1; }
}


/* ---- sprites: train, helicopter, plane, ship, monster, tornado, explosion (SPR8.BIN / SPR1.BIN, art/tools/mksprites.py) ----
   The sim side is s_sprite.c (the original sprite code).  Drawing: tiles first, then every live sprite is merged over the
   tiles with a masked blit (tblit.asm).  A sprite that moved, changed frame or vanished marks the tiles it covered as
   changed, so the tile pass repaints them before the sprites go back on.  Art that does not fit in memory is skipped, and a
   sprite type without art is never created by the sim (SpriteArt mask).  NOSPRITES builds have none of this. */
#ifndef NOSPRITES
extern void __cdecl asm_spr_blit(unsigned srcseg, unsigned srcoff, unsigned ww, unsigned rows, unsigned psz, int x, int y, unsigned nplanes);
#define SPR_RESERVE 6144UL        /* bytes kept free for file I/O (save/load) after the art is loaded */
struct sart { unsigned seg, nfr, ww, rows, psz; };
static struct sart sart[8];
static char sprmsg[48];
static const unsigned char spr_order[7] = { TRA, SHI, AIR, COP, EXP, GOD, TOR };      /* load priority: cheapest and most useful first */
static const char *const spr_name[8] = { "", "train", "helicopter", "plane", "ship", "monster", "tornado", "explosion" };
struct sdraw { short t, f, x, y; };
#define NSPD 10
static struct sdraw spo[NSPD], spn[NSPD];
static unsigned char spskip[NSPD];     /* 1: unchanged since the last refresh and nothing under it is repainted: it is still on screen */
static int nspo, nspn;
static unsigned long spr_bytes;

static void load_sprites(const char *name)
{
  int h, i, nmiss = 0; unsigned got, seg, mask = 0; unsigned hdr[38];
  SpriteArt = 0; sprmsg[0] = 0; spr_bytes = 0;
  if (_dos_open(name, O_RDONLY, &h)) { strcpy(sprmsg, "no sprite file"); return; }
  if (_dos_read(h, hdr, 76, &got) || got != 76) { _dos_close(h); strcpy(sprmsg, "bad sprite file"); return; }
  for (i = 0; i < 7; i++) {
    int t = spr_order[i];
    unsigned *e = hdr + 3 + (t - 1) * 5;                /* nframes, ww, rows, offset, bytes */
    unsigned bytes = e[4];
    if ((unsigned long)dos_free_paras() * 16UL < (unsigned long)bytes + SPR_RESERVE || _dos_allocmem((bytes + 15) >> 4, &seg)) { nmiss++; continue; }
    if (lseek(h, (long)e[3], SEEK_SET) < 0 || _dos_read(h, MK_FP(seg, 0), bytes, &got) || got != bytes) { _dos_freemem(seg); nmiss++; continue; }
    sart[t].seg = seg; sart[t].nfr = e[0]; sart[t].ww = e[1]; sart[t].rows = e[2]; sart[t].psz = e[1] * e[2] * 2;
    mask |= 1U << t; spr_bytes += bytes;
  }
  _dos_close(h);
  SpriteArt = mask;
  if (nmiss) sprintf(sprmsg, "%d of 7 sprite types did not fit", nmiss);
}

/* tiles covered by a screen rectangle are marked changed */
static void spr_inval(int sx, int sy, int w, int h)
{
  int tx0, tx1, ty0, ty1, tx, ty;
  if (sx + w <= 0 || sy + h <= 0 || sx >= WIN_W * 16 || sy >= WIN_H * TH) return;
  tx0 = sx < 0 ? 0 : sx >> 4; tx1 = (sx + w - 1) >> 4; if (tx1 >= WIN_W) tx1 = WIN_W - 1;
  ty0 = sy < 0 ? 0 : sy / TH; ty1 = (sy + h - 1) / TH; if (ty1 >= WIN_H) ty1 = WIN_H - 1;
  for (ty = ty0; ty <= ty1; ty++) for (tx = tx0; tx <= tx1; tx++) shown[ty * WIN_W + tx] = 0xFFFF;
}
/* before the tile pass: list the live sprites in window coordinates; repaint what the changed ones used to cover */
static void spr_prepare(void)
{
  SimSprite SPRNEAR *sp; int i, j;
  nspn = 0;
  for (sp = sim->sprite; sp != NULL; sp = sp->next) {
    int t = sp->type, f = sp->frame, sx, sy;
    if (!f || !(SpriteArt & (1U << t)) || f > (int)sart[t].nfr || nspn >= NSPD) continue;
    sx = sp->x + sp->x_offset - wx * 16;
    sy = ((sp->y + sp->y_offset - wy * 16) * 9) >> 4;
    if (sx >= WIN_W * 16 || sx + (int)sart[t].ww * 16 <= 0 || sy >= WIN_H * TH || sy + (int)sart[t].rows <= 0) continue;
    spn[nspn].t = t; spn[nspn].f = f; spn[nspn].x = sx; spn[nspn].y = sy; nspn++;
  }
  for (i = 0; i < nspo; i++) {
    for (j = 0; j < nspn; j++) if (!memcmp(&spo[i], &spn[j], sizeof spo[i])) break;
    if (j == nspn) spr_inval(spo[i].x, spo[i].y, (int)sart[spo[i].t].ww * 16, (int)sart[spo[i].t].rows);
  }
  /* a sprite that did not change stays on screen unless one of its tiles is about to be repainted (animated tile, or the
     tiles a moved sprite uncovered): then it is merged again.  This keeps a parked sprite free. */
  for (j = 0; j < nspn; j++) {
    int w = (int)sart[spn[j].t].ww * 16, h = (int)sart[spn[j].t].rows, tx0, tx1, ty0, ty1, tx, ty;
    spskip[j] = 0;
    for (i = 0; i < nspo; i++) if (!memcmp(&spo[i], &spn[j], sizeof spo[i])) break;
    if (i == nspo) continue;
    tx0 = spn[j].x < 0 ? 0 : spn[j].x >> 4; tx1 = (spn[j].x + w - 1) >> 4; if (tx1 >= WIN_W) tx1 = WIN_W - 1;
    ty0 = spn[j].y < 0 ? 0 : spn[j].y / TH; ty1 = (spn[j].y + h - 1) / TH; if (ty1 >= WIN_H) ty1 = WIN_H - 1;
    spskip[j] = 1;
    for (ty = ty0; ty <= ty1 && spskip[j]; ty++) for (tx = tx0; tx <= tx1; tx++)
      if (shown[ty * WIN_W + tx] != (unsigned)(MapData[(wx + tx) * WORLD_Y + wy + ty] & LOMASK)) { spskip[j] = 0; break; }
  }
}
/* after the tile pass: merge every sprite over the tiles (the merge is idempotent, so unchanged ones are simply redrawn) */
static void spr_draw(void)
{
  int i;
  for (i = 0; i < nspn; i++) {
    struct sart *a = &sart[spn[i].t];
    if (spskip[i]) continue;
    asm_spr_blit(a->seg, (spn[i].f - 1) * (mono ? 2 : 4) * a->psz, a->ww, a->rows, a->psz, spn[i].x, spn[i].y, mono ? 1 : 3);
  }
  memcpy(spo, spn, nspn * sizeof spo[0]); nspo = nspn;
}
/* test hook (arg SD): one of each sprite near the cursor, to look at the art */
static void spr_demo(void)
{
  SpritesSet(1);
  MakeSprite(AIR, ((cx - 16) << 4) + 48, ((cy - 8) << 4) + 12);
  MakeSprite(COP, (cx - 8) << 4, ((cy - 8) << 4) + 30);
  MakeSprite(SHI, (cx - 1) << 4, (cy - 8) << 4);
  MakeSprite(GOD, ((cx + 8) << 4) + 48, (cy - 8) << 4);
  MakeSprite(TOR, (cx + 14) << 4, (cy - 8) << 4);
  MakeNewSprite(EXP, (cx - 16) << 4, (cy + 2) << 4);
  MakeSprite(TRA, (cx << 4) + (-39), (cy << 4) + 6);
}
/* test hook (arg SF1 train, SF2 ship, SF3 plane and helicopter): find a rail, channel or airport tile in the loaded city,
   put the cursor there and start the sprite on it, as the sim would */
static void spr_find(int kind)
{
  int x, y, k;
  SpritesSet(spr_pref);
  for (x = 1; x < WORLD_X - 4; x++) for (y = 1; y < WORLD_Y - 4; y++) {
    int t = MapData[x * WORLD_Y + y] & LOMASK, t1 = MapData[(x + 1) * WORLD_Y + y] & LOMASK;
    if (kind == 1 && x > 12 && y > 12 && t >= RAILBASE && t <= LASTRAIL && t1 >= RAILBASE && t1 <= LASTRAIL) {
      if (spr_pref) MakeSprite(TRA, (x << 4) - 39, (y << 4) + 6); k = 1;
    } else if (kind == 2 && t == CHANNEL && t1 == CHANNEL && x > 12 && y > 12) {
      if (spr_pref) MakeSprite(SHI, (x << 4) - 47, y << 4); k = 1;
    } else if (kind == 3 && t == AIRPORT && (MapData[x * WORLD_Y + y] & ZONEBIT)) {
      if (spr_pref) { MakeSprite(AIR, (x << 4) + 48, (y << 4) + 12); MakeSprite(COP, (x << 4) - 16, (y << 4) + 30); } k = 1;
    } else k = 0;
    if (k) { cx = x; cy = y; wx = cx + WIN_W; wy = cy + WIN_H; return; }
  }
}
static void spr_apply(void)       /* the setting, as far as there is art for it */
{
  SpritesSet(spr_pref && (SpriteArt & ((1U << TRA) | (1U << SHI) | (1U << AIR) | (1U << COP))) ? 1 : 0);
}
#else
static const char sprmsg[] = "";
static unsigned long spr_bytes;
static void load_sprites(const char *name) { SpriteArt = 0; }
static void spr_prepare(void) {}
static void spr_draw(void) {}
static void spr_demo(void) {}
static void spr_find(int kind) {}
static void spr_apply(void) {}
#define nspo nspo_unused
static int nspo_unused;
#endif

static unsigned draw_map(void)
{
  unsigned mo = FP_OFF(&MapData[wx * WORLD_Y + wy]);
  return mono ? asm_draw_map1(mo, FP_OFF(shown), tileseg, pstep)
              : asm_draw_map(mo, FP_OFF(shown), tileseg, pstep);
}
static void invalidate(void) { memset(shown, 0xFF, sizeof shown); nspo = 0; }

static void say(const char *s) { strncpy(msg, s, sizeof msg - 1); msg[sizeof msg - 1] = 0; msg_cs = now_cs(); }

static const char *mname[12] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
static const char *spdname[4] = {"PAUSE", "SLOW", "MED", "FAST"};
static char sgn(short v) { return v > 0 ? '+' : v < 0 ? '-' : '='; }

static void status_update(void)
{
  char buf[120];
  long t = (long)CityTime;
  const char *m = "";
  if (prompt == 1) sprintf(buf, "Go to X Y: %s_   (Enter = go, Esc = cancel)", pbuf);
  else if (prompt == 4) sprintf(buf, "Save city as: %s_   (Enter = save, Esc = cancel)", pbuf);
  else if (prompt == 5) sprintf(buf, "Load city: %s_   (Enter = load, Esc = cancel)", pbuf);
  else if (prompt == 2) strcpy(buf, "Quit Micropolis?  Y = yes, any other key = no");
  else if (prompt == 3) strcpy(buf, "Start a new city?  Y = yes, any other key = no");
  else {
    if (msg[0] && elapsed(msg_cs) < 800) m = msg;
    else if (simmsg[0] && elapsed(simmsg_cs) < 1500) m = simmsg;
    sprintf(buf, "%s %d $%ld Pop %ld R%cC%cI%c %s $%ld %s%s%s %s",
            mname[(int)((t % 48) / 4)], (int)(t / 48 + StartingYear), (long)TotalFunds, (long)CityPop,
            sgn(RValve), sgn(CValve), sgn(IValve), tools[tool_ix].name, (long)CostOf[tools[tool_ix].state],
            spdname[SimSpeed & 3], SpritesOn ? " SPR" : "", drag ? " DRAG" : "", m);
  }
  { int n = strlen(buf); char pos[12]; sprintf(pos, "%d,%d", cx, cy);
    if (n > 72) n = 72;
    memset(buf + n, ' ', 80 - n); buf[80] = 0;
    memcpy(buf + 80 - strlen(pos), pos, strlen(pos)); }
  status_line(buf);
}

static void scroll_to_cursor(void)
{
  int nwx = wx, nwy = wy;
  if (cx < wx) nwx = cx - WIN_W / 2; else if (cx >= wx + WIN_W) nwx = cx - WIN_W / 2;
  if (cy < wy) nwy = cy - WIN_H / 2; else if (cy >= wy + WIN_H) nwy = cy - WIN_H / 2;
  if (nwx < 0) nwx = 0; if (nwx > WORLD_X - WIN_W) nwx = WORLD_X - WIN_W;
  if (nwy < 0) nwy = 0; if (nwy > WORLD_Y - WIN_H) nwy = WORLD_Y - WIN_H;
  if (nwx != wx || nwy != wy) { cursor_hide(); wx = nwx; wy = nwy; invalidate(); }
}

static unsigned stack_used(void);
static unsigned long tmap_sum(void)
{
  unsigned long h = 0; int x, y;
  for (x = 0; x < WORLD_X; x++)
    for (y = 0; y < WORLD_Y; y++)
      h = (((h << 3) | (h >> 29)) & 0xFFFFFFFFUL) ^ (unsigned short)MapData[x * WORLD_Y + y];
  return h & 0xFFFFFFFFUL;
}
static void quit_now(void)
{
  unsigned long q = tmap_sum();
  asm_snd_exit();
  video_exit();
  if (csum_mode) {   /* test hook: MPTIPC ... C > A:OUT.TXT */
    if (have_load) printf("LOADSUM %08lx time %ld funds %ld\n", load_sum, load_t, load_f);
    printf("MSG %s\n", msg);
    if (have_save) printf("SAVESUM %08lx\n", save_sum);
    if (snd_test) printf("SND played %lu\n", snd_plays);
    printf("QUITSUM %08lx time %ld funds %ld pop %ld stack %u\n", q, (long)CityTime, (long)TotalFunds, (long)CityPop, stack_used());
  }
  exit(0);
}

/* ---- query ---- */
static const short idArray[28] = {
  DIRT, RIVER, TREEBASE, RUBBLE, FLOOD, RADTILE, FIRE, ROADBASE, POWERBASE, RAILBASE, RESBASE, COMBASE,
  INDBASE, PORTBASE, AIRPORTBASE, COALBASE, FIRESTBASE, POLICESTBASE, STADIUMBASE, NUCLEARBASE,
  827, 832, FOUNTAIN, INDBASE2, FOOTBALLGAME1, VBRDG0, 952, 956
};
static int density_str(int catNo, int x, int y)
{
  int z;
  switch (catNo) {
  case 0: z = PopDensity[x >> 1][y >> 1]; return (z >> 6) & 3;
  case 1: z = LandValueMem[x >> 1][y >> 1]; return z < 30 ? 4 : z < 80 ? 5 : z < 150 ? 6 : 7;
  case 2: z = CrimeMem[x >> 1][y >> 1]; return ((z >> 6) & 3) + 8;
  case 3: z = PollutionMem[x >> 1][y >> 1]; if (z < 64 && z > 0) return 13; return ((z >> 6) & 3) + 12;
  default: z = RateOGMem[x >> 3][y >> 3]; return z < 0 ? 16 : z == 0 ? 17 : z > 100 ? 19 : 18;
  }
}
static void do_query(void)
{
  char name[40], s[5][24], buf[sizeof msg];
  short tile = MapData[cx * WORLD_Y + cy] & LOMASK;
  int x, id;
  if (tile >= COALSMOKE1 && tile < FOOTBALLGAME1) tile = COALBASE;
  for (x = 1; x < 29; x++) if (tile < idArray[x]) break;
  x--; if (x < 0) x = 0; if (x > 26) x = 26;
  GetIndString(name, 219, x + 1);
  for (x = 0; x < 5; x++) {
    id = density_str(x, cx, cy) + 1;
    if (id <= 0) id = 1; if (id > 20) id = 20;
    GetIndString(s[x], 202, id);
  }
  sprintf(buf, "%.12s D:%.6s V:%.6s C:%.6s P:%.6s G:%.6s", name, s[0], s[1], s[2], s[3], s[4]);
  say(buf);
}

static void place(void)
{
  int r;
  short st = tools[tool_ix].state;
  if (st == queryState) { do_query(); return; }
  r = do_tool_xy(st, cx, cy);
  if (r == -1) say("Bad location");
  else if (r == -2) say("Not enough funds");
  else if (r == 0) say("Can't build here");
  if (r < 1) snd_play(S_ERR); else snd_play(st == dozeState ? S_DOZE : S_CLICK);
}

static void new_city(unsigned seed)
{
  DestroyAllSprites();          /* the old city's sprites must not fly over the new one */
  GenerateSomeCity((int)seed);
  StartingYear = 1900;
  GameLevel = 0; SetFunds(20000L); CityTax = 7;
  autoBudget = 1; autoBulldoze = autobull; SetDisasters(!nodis);
  msg[0] = simmsg[0] = 0; HaveLastMessage = 0; LastMessage[0] = 0;
  cx = WORLD_X / 2; cy = WORLD_Y / 2; wx = cx + WIN_W; wy = cy + WIN_H;
  invalidate();
}

static void set_speed(void) { setSpeed(speed); }

/* the title screen of the text layer (shown while the city and tiles load); kept in a far segment, not in DGROUP */
static const char far banner[] =
  "Micropolis for TIPC - a TI Professional Computer port of Micropolis\n"
  "Code by Shurr Nuff (AI assistant), produced by Will (stovebolt)\n"
  "Flying Horse Labs.  GPL v3 (see COPYING and README.TXT).\n\n"
  "Micropolis is a registered trademark of Micropolis Corporation\n"
  "(Micropolis GmbH) and is licensed here as a courtesy of the owner.\n"
  "www.micropolis.com\n\nLoading...\n";

static void show_help(void)
{
  static const char *const h[] = {
    "MICROPOLIS FOR TIPC - keys",
    "Arrows  move the cursor (the map scrolls)",
    "Enter   build / use the tool at the cursor",
    "D       drag mode: build on every cursor move (roads, wires, rails, doze)",
    "G       go to X Y          Space  pause          - / +  slower / faster",
    "S       save city (filename prompt)      L  load city",
    "M       budget and taxes   V  evaluation   J  graphs",
    "Tab / I settings: sprites, sound, disasters, auto-bulldoze, speed",
    "X       disasters on/off   O  sound on/off   Z  new city   ESC  quit",
    "",
    "Tools (letter or F-key)                       cost",
    "Q F1 query       B F2 bulldozer $1     R F3 road $10     T F4 rail $20",
    "W F5 wire $5     K park $10            1 F6 residential $100",
    "2 F7 commercial $100                   3 F8 industrial $100",
    "P F9 police $500                       F F10 fire dept $500",
    "U stadium $5000   C coal power $3000   N nuclear $5000",
    "E seaport $3000   A airport $10000",
    "Status line: date, funds, population, R/C/I demand (+ wanted), tool,",
    "speed, SPR = sprites on.  Zones need power (wires from a plant) and roads.",
    "Code by Shurr Nuff (AI assistant), produced by Will (stovebolt),",
    "Flying Horse Labs.  Micropolis is a trademark of Micropolis",
    "Corporation, used by courtesy.  GPL v3.",
    "                     Press any key"
  };
  unsigned r, i;
  cursor_hide(); gfx_clear(); text_clear();
  VID(0x800) = 0x0F;
  for (r = 0; r < sizeof h / sizeof *h; r++)
    for (i = 0; h[r][i]; i++) TXT((r + 1) * 80 + 2 + i) = h[r][i];
  for (;;) { if (kbd_poll() != -1) break; }
  text_clear(); invalidate();
}



/* ---- full-screen pages: budget, evaluation, graphs (text layer + a little pixel drawing) ---- */
static void tput(int r, int c, const char *s)
{
  unsigned o = r * 80 + c;
  while (*s && c < 80) { TXT(o++) = (unsigned char)*s++; c++; }
}
static char pl[84];
#define TPF(r, c) (tput((r), (c), pl))
static void page_begin(void) { cursor_hide(); gfx_clear(); text_clear(); VID(0x800) = 0x0F; }
static void page_end(void) { text_clear(); invalidate(); }
static int wait_key(void) { int k; while ((k = kbd_poll()) == -1) ; return k; }
static const char *const lvlname[3] = { "Easy", "Medium", "Hard" };

/* All floating point lives in glue.c (a double in this file would pull the 4K float printf code into the program). */
void budget_calc(QUAD *tax, QUAD *road, QUAD *fire, QUAD *pol);
int fund_pct(int which);
void fund_pct_set(int which, int pct);
QUAD fund_amount(QUAD req, int which);

static void show_budget(int manual)
{
  int sel = 0, k, c, r;
  for (;;) {
    QUAD tax, road, fire, pol, out;
    QUAD req[3]; static const char *const nm[3] = { "Road funding", "Fire department", "Police department" };
    budget_calc(&tax, &road, &fire, &pol);
    req[0] = road; req[1] = fire; req[2] = pol;
    page_begin();
    sprintf(pl, "BUDGET   %s %d   Funds $%ld%s", mname[(int)((CityTime % 48) / 4)], (int)(CityTime / 48 + StartingYear), (long)TotalFunds,
            manual ? "   (year-end budget: Enter accepts)" : ""); TPF(1, 2);
    sprintf(pl, "%c Tax rate            %2d%%", sel == 0 ? '>' : ' ', (int)CityTax); TPF(4, 2);
    sprintf(pl, "%c Auto-budget         %s", sel == 1 ? '>' : ' ', autoBudget ? "ON " : "OFF"); TPF(5, 2);
    tput(7, 6, "                      Requested     Funded    Level");
    out = 0;
    for (r = 0; r < 3; r++) {
      QUAD fd = fund_amount(req[r], r);
      sprintf(pl, "%c %-18s $%7ld   $%7ld    %3d%%", sel == r + 2 ? '>' : ' ', nm[r], (long)req[r], (long)fd, fund_pct(r));
      TPF(8 + r, 2); out += fd;
    }
    tput(12, 2, "Cash flow for the year (estimate)");
    sprintf(pl, "  Taxes collected              $%7ld", (long)tax); TPF(13, 2);
    sprintf(pl, "  Roads, fire, police          $%7ld", (long)-out); TPF(14, 2);
    sprintf(pl, "  Net                          $%7ld", (long)(tax - out)); TPF(15, 2);
    sprintf(pl, "  Funds now                    $%7ld", (long)TotalFunds); TPF(17, 2);
    sprintf(pl, "  Funds after the year         $%7ld", (long)(TotalFunds + tax - out)); TPF(18, 2);
    tput(21, 2, "Up/Down: choose   Left/Right: change   Enter/Esc: done");
    if (!autoBudget && !manual) tput(22, 2, "Auto-budget OFF: the game stops at each year-end and opens this page.");
    k = wait_key(); c = k & 0xFF;
    if (c == 27 || c == 13) break;
    if (k == KEY_UP && sel > 0) sel--;
    else if (k == KEY_DOWN && sel < 4) sel++;
    else if (k == KEY_LEFT || k == KEY_RIGHT) {
      int d = (k == KEY_RIGHT) ? 1 : -1;
      if (sel == 0) { CityTax += d; if (CityTax < 0) CityTax = 0; if (CityTax > 20) CityTax = 20; }
      else if (sel == 1) autoBudget = !autoBudget;
      else { int v = fund_pct(sel - 2) + d * 5; if (v < 0) v = 0; if (v > 100) v = 100; fund_pct_set(sel - 2, v); }
    }
  }
  page_end();
}
static void budget_hook(void) { show_budget(1); }
extern void (*BudgetHook)(void);

static const char *const probname[7] = { "Crime", "Pollution", "Housing costs", "Taxes", "Traffic", "Unemployment", "Fires" };
static const char *const classname[6] = { "Village", "Town", "City", "Capital", "Metropolis", "Megalopolis" };
static void show_eval(void)
{
  int i;
  page_begin();
  sprintf(pl, "CITY EVALUATION   %s %d", mname[(int)((CityTime % 48) / 4)], (int)(CityTime / 48 + StartingYear)); TPF(1, 2);
  tput(3, 2, "Public opinion");
  tput(4, 4, "Is the mayor doing a good job?");
  sprintf(pl, "Yes %3d%%     No %3d%%", (int)CityYes, (int)CityNo); TPF(5, 6);
  tput(7, 2, "What are the biggest problems?");
  for (i = 0; i < 4; i++) {
    int p = ProblemOrder[i];
    if (p < 7 && ProblemVotes[p] > 0) { sprintf(pl, "%-16s %3d%%", probname[p], (int)ProblemVotes[p]); TPF(8 + i, 6); }
  }
  tput(13, 2, "Statistics");
  sprintf(pl, "Population          %7ld", (long)CityPop); TPF(14, 6);
  sprintf(pl, "Net migration       %7ld  (since last year)", (long)deltaCityPop); TPF(15, 6);
  sprintf(pl, "Assessed value      $%7ld", (long)CityAssValue); TPF(16, 6);
  sprintf(pl, "Category            %s", classname[(CityClass >= 0 && CityClass < 6) ? CityClass : 0]); TPF(17, 6);
  sprintf(pl, "Game level          %s", lvlname[(GameLevel >= 0 && GameLevel < 3) ? GameLevel : 0]); TPF(18, 6);
  sprintf(pl, "Overall city score  %d  (change %+d)", (int)CityScore, (int)deltaCityScore); TPF(20, 6);
  tput(23, 2, "Updated once a year by the simulation.   Any key: back");
  wait_key();
  page_end();
}

/* pixel drawing for the graphs: x 0..719, y 0..299; c = colour 1..7 (mono: always plane A) */
static void gpix(int x, int y, int c)
{
  unsigned o = (unsigned)y * 92U + (x >> 3); unsigned char m = 0x80 >> (x & 7); int p;
  if (x < 0 || x > 719 || y < 0 || y > 299) return;
  if (mono) c = 1;
  for (p = 0; p < 3; p++) if (c & (1 << p)) *(unsigned char far *)MK_FP(planeseg[p], o) |= m;
}
static void gline(int x0, int y0, int x1, int y1, int c, int pat)
{
  int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y0 - y1 : y1 - y0, sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, e = dx + dy, e2, n = 0;
  for (;;) {
    if (pat == 0 || (pat == 1 && (n & 7) < 5) || (pat == 2 && (n & 3) < 2)) gpix(x0, y0, c);
    n++;
    if (x0 == x1 && y0 == y1) break;
    e2 = 2 * e;
    if (e2 >= dy) { e += dy; x0 += sx; }
    if (e2 <= dx) { e += dx; y0 += sy; }
  }
}
#define GX0 60
#define GY0 36
#define GW 600
#define GH 200
static void plot_series(const short *h, int base, int lo, int hi, int col, int pat)
{
  int i, px = -1, py = 0;
  if (hi <= lo) hi = lo + 1;
  for (i = 119; i >= 0; i--) {                       /* index 0 is the newest sample */
    long v = h[base + i]; int x = GX0 + (119 - i) * (GW / 119), y;
    if (v < lo) v = lo; if (v > hi) v = hi;
    y = GY0 + GH - (int)((v - lo) * GH / (hi - lo));
    if (px >= 0) gline(px, py, x, y, col, pat);
    px = x; py = y;
  }
}
static void show_graphs(void)
{
  int page = 0, span = 0, k, i, lo, hi, c;
  static const char *const ttl[2] = { "Residential, Commercial, Industrial", "Money, Crime, Pollution" };
  static const char *const nm[2][3] = { { "Residential", "Commercial", "Industrial" }, { "Money", "Crime", "Pollution" } };
  static const unsigned char gcol[3] = { 2, 1, 4 };    /* colour numbers, refined after looking at the screen */
  for (;;) {
    const short *s[3]; int base = span ? 120 : 0;
    s[0] = page ? MoneyHis : ResHis; s[1] = page ? CrimeHis : ComHis; s[2] = page ? PollutionHis : IndHis;
    page_begin();
    sprintf(pl, "GRAPHS   %s   %s", ttl[page], span ? "120 years (yearly)" : "10 years (monthly)"); TPF(0, 2);
    lo = 0; hi = 1;
    if (page == 0) { for (c = 0; c < 3; c++) for (i = 0; i < 120; i++) if (s[c][base + i] > hi) hi = s[c][base + i]; }
    else hi = 255;
    for (i = 0; i <= GW; i += 5) { if (i % 60 == 0) { int x = GX0 + i, y; for (y = GY0; y <= GY0 + GH; y += 4) gpix(x, y, 7); } }
    gline(GX0, GY0, GX0, GY0 + GH, 7, 0); gline(GX0, GY0 + GH, GX0 + GW, GY0 + GH, 7, 0);
    for (c = 0; c < 3; c++) plot_series(s[c], base, lo, hi, gcol[c], c);
    sprintf(pl, "%d", hi); tput(3, 1, pl);
    tput(20, 5, span ? "120 years ago" : "10 years ago"); tput(20, 66, "now");
    for (c = 0; c < 3; c++) {
      int x = 20 + c * 160;
      gline(x, 276, x + 30, 276, gcol[c], c);
      tput(22, 6 + c * 17, nm[page][c]);
    }
    tput(24, 2, "Tab: other graph   1: 10 years   2: 120 years   Esc: back");
    k = wait_key(); c = k & 0xFF;
    if (c == 27 || c == 13) break;
    if (c == 9 || c == ' ') page = !page;
    else if (c == '1') span = 0;
    else if (c == '2') span = 1;
  }
  page_end();
}

/* ---- settings: Tab or I.  Kept in MP.CFG (8 bytes, current drive) so a saved .cty stays byte-identical to the reference ---- */
static void cfg_save(void)
{
  unsigned char b[8]; int h; unsigned n;
  b[0] = 'M'; b[1] = 'P'; b[2] = 1; b[3] = (unsigned char)spr_pref; b[4] = snd_on; b[5] = (unsigned char)nodis; b[6] = (unsigned char)autobull; b[7] = (unsigned char)speed;
  if (_dos_creat("MP.CFG", _A_NORMAL, &h)) return;       /* read-only disk etc.: the settings just are not kept */
  _dos_write(h, b, 8, &n); _dos_close(h);
}
static void cfg_load(void)
{
  unsigned char b[8]; int h; unsigned n;
  if (_dos_open("MP.CFG", O_RDONLY, &h)) return;
  if (!_dos_read(h, b, 8, &n) && n == 8 && b[0] == 'M' && b[1] == 'P' && b[2] == 1) {
    spr_pref = b[3] != 0; snd_on = b[4] != 0; nodis = b[5] != 0; autobull = b[6] != 0;
    if (b[7] >= 1 && b[7] <= 3) speed = b[7];
  }
  _dos_close(h);
}
static void settings_apply(void)
{
  spr_apply(); SetDisasters(!nodis); autoBulldoze = autobull;
  if (!snd_on) snd_stop();
  set_speed(); if (paused) setSpeed(0);
}
static void show_settings(void)
{
  int sel = 0, k, c, r;
  static const char *const nm[5] = { "Sprites (trains, ships, planes, helicopters)", "Sound", "Disasters (fire, flood, tornado, monster, crash)", "Auto-bulldoze", "Game speed" };
  for (;;) {
    int on[5]; char val[5][8];
    on[0] = spr_pref; on[1] = snd_on; on[2] = !nodis; on[3] = autobull;
    for (r = 0; r < 4; r++) strcpy(val[r], on[r] ? "ON" : "OFF");
    strcpy(val[4], spdname[speed & 3]);
    page_begin();
    tput(1, 2, "SETTINGS");
    for (r = 0; r < 5; r++) { sprintf(pl, "%c %d  %-50s %s", sel == r ? '>' : ' ', r + 1, nm[r], val[r]); TPF(4 + r * 2, 2); }
#ifndef NOSPRITES
    if (!(SpriteArt & ((1U << TRA) | (1U << SHI) | (1U << AIR) | (1U << COP)))) tput(15, 2, "Sprites: no art loaded, so no sprites (see the notes: not enough memory).");
    else { sprintf(pl, "Sprite art in memory: %lu bytes. %s", spr_bytes, sprmsg); TPF(15, 2); }
#else
    tput(15, 2, "Sprites: this build (MPTIPCL, for 256K colour) has no sprites; MPTIPC has.");
#endif
    tput(17, 2, "When sprites are off the sim creates and moves none (a little faster).");
    tput(18, 2, "The same switches: O sound, X disasters, + and - speed.");
    tput(21, 2, "Up/Down: choose   Left/Right/Space/Enter or 1-5: change   Esc or Tab: done");
    tput(22, 2, "Saved in MP.CFG on the current disk when you leave this page.");
    k = wait_key(); c = k & 0xFF;
    if (c == 27 || c == 9) break;
    if (k == KEY_UP && sel > 0) sel--;
    else if (k == KEY_DOWN && sel < 4) sel++;
    else {
      int pick = -1, d = (k == KEY_LEFT) ? -1 : 1;
      if (k == KEY_LEFT || k == KEY_RIGHT || c == ' ' || c == 13) pick = sel;
      else if (c >= '1' && c <= '5') { pick = c - '1'; sel = pick; }
      if (pick == 0) { spr_pref = !spr_pref; }
      else if (pick == 1) { snd_on = !snd_on; }
      else if (pick == 2) { nodis = !nodis; }
      else if (pick == 3) { autobull = !autobull; }
      else if (pick == 4) { speed += d; if (speed > 3) speed = 1; if (speed < 1) speed = 3; paused = 0; }
      if (pick >= 0) settings_apply();
    }
  }
  settings_apply(); cfg_save();
  page_end();
}

static void ui_tick(void);
/* ---- save / load (original .cty format, 27,120 bytes) ---- */
#define CTY_BYTES 27120UL
static void view_after_load(void)
{
  long sx = 0, sy = 0, n = 0; int x, y;
  for (x = 0; x < WORLD_X; x++) for (y = 0; y < WORLD_Y; y++)
    if ((MapData[x * WORLD_Y + y] & LOMASK) >= 64) { sx += x; sy += y; n++; }
  cx = n ? (int)(sx / n) : WORLD_X / 2; cy = n ? (int)(sy / n) : WORLD_Y / 2;
  wx = cx + WIN_W; wy = cy + WIN_H;
  invalidate();
}
/* 1 = the 27,120 bytes (plus the old file, which gets replaced) fit on the target disk; 0 = disk full; -1 = bad drive */
static int disk_room(const char *name)
{
  struct _diskfree_t df; unsigned drv = 0; unsigned long cb, need, avail, old = 0; FILE *f;
  if (name[0] && name[1] == ':') drv = (unsigned)((name[0] & 0xDF) - 'A' + 1);
  if (_dos_getdiskfree(drv, &df)) return -1;
  cb = (unsigned long)df.sectors_per_cluster * df.bytes_per_sector;
  need = (CTY_BYTES + cb - 1) / cb * cb;
  avail = (unsigned long)df.avail_clusters * cb;
  if ((f = fopen(name, "rb")) != NULL) {
    fseek(f, 0L, SEEK_END); old = (unsigned long)ftell(f); fclose(f);
    old = (old + cb - 1) / cb * cb;
  }
  return avail + old >= need;
}
static void fail(const char *s) { say(s); snd_play(S_ERR); }
static void do_save(const char *name)
{
  char b[56]; int r;
  snd_stop(); r = disk_room(name);
  if (r < 0) { fail("Bad drive"); return; }
  if (!r) { fail("Disk full, nothing saved"); return; }
  if (!saveFile((char *)name)) { remove(name); fail(errno == ENOMEM ? "Save failed: out of memory" : "Save failed: disk full or write protected"); _fheapshrink(); return; }
  _fheapshrink();
  save_sum = tmap_sum(); have_save = 1;
  strcpy(fname, name);
  sprintf(b, "Saved %s", name); say(b); snd_play(S_CHIME);
}
static void do_load(const char *name)
{
  FILE *f; int ok;
  snd_stop();
  if ((f = fopen(name, "rb")) == NULL) { fail("File not found"); return; }
  fseek(f, 0L, SEEK_END);
  ok = ftell(f) == (long)CTY_BYTES;
  fclose(f);
  if (!ok) { _fheapshrink(); fail("Not a Micropolis city file"); return; }
  say("Loading... (about 15 s)"); status_update();
  cursor_hide();
  MapScanYield = 0;
  ResetForLoad();               /* clears the sim's arrays, seeds the RNG as at startup, clears stale scalars (s_alloc.c) */
  ok = loadFile((char *)name);
  MapScanYield = ui_tick;
  _fheapshrink();
  if (!ok) { fail("Load failed"); invalidate(); return; }
  StartingYear = 1900; spr_apply(); SetDisasters(!nodis); autoBulldoze = autobull;
  set_speed(); if (paused) setSpeed(0);          /* keep the player's speed, as at startup (the file's speed would change the sim) */
  msg[0] = simmsg[0] = 0; HaveLastMessage = 0; LastMessage[0] = 0;
  load_sum = tmap_sum(); load_t = (long)CityTime; load_f = (long)TotalFunds; have_load = 1;
  ystop_t = CityTime + ystop * 48L;     /* test hook Yn: n years after this load */
  strcpy(fname, name);
  view_after_load();
  say("Loaded"); snd_play(S_CHIME);
}

static void prompt_key(int k)
{
  int c = k & 0xFF;
  if (prompt == 2 || prompt == 3) {
    int was = prompt;
    prompt = 0;
    if (c == 'y' || c == 'Y') {
      if (was == 2) quit_now();
      else new_city(now_cs() & 0x7FFF);
    }
    return;
  }
  if (c == 27) { prompt = 0; return; }
  if (prompt == 4 || prompt == 5) {
    if (c == 13) {
      int was = prompt;
      prompt = 0;
      if (!pbuf[0]) return;
      if (was == 4) do_save(pbuf); else do_load(pbuf);
    } else if (c == 8) { pfresh = 0; if (plen) pbuf[--plen] = 0; }
    else {
      if (c >= 'a' && c <= 'z') c -= 32;
      if (((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == ':' || c == '\\' || c == '_' || c == '-') && (pfresh || plen < 30)) { if (pfresh) { plen = 0; pfresh = 0; } pbuf[plen++] = c; pbuf[plen] = 0; }
    }
    return;
  }
  if (c == 13) {
    int x = -1, y = -1; char *q = pbuf;
    if (*q >= '0' && *q <= '9') { x = atoi(q); while (*q >= '0' && *q <= '9') q++; while (*q == ' ') q++;
      if (*q >= '0' && *q <= '9') y = atoi(q); }
    prompt = 0;
    if (x >= 0 && x < WORLD_X && y >= 0 && y < WORLD_Y) { cx = x; cy = y; }
    else say("Use X Y within 0-119 0-99");
    return;
  }
  if (c == 8 && plen) { pbuf[--plen] = 0; return; }
  if (((c >= '0' && c <= '9') || c == ' ' || c == ',') && plen < 10) { pbuf[plen++] = (c == ',') ? ' ' : c; pbuf[plen] = 0; }
}

static void move_cursor(int dx, int dy)
{
  int nx = cx + dx, ny = cy + dy;
  if (nx < 0 || nx >= WORLD_X || ny < 0 || ny >= WORLD_Y) return;
  cx = nx; cy = ny;
  if (drag && tools[tool_ix].state != queryState) place();
}

static void handle_key(int k)
{
  int c, i;
  if (prompt) { prompt_key(k); return; }
  switch (k) {
  case KEY_LEFT:  move_cursor(-1, 0); return;
  case KEY_RIGHT: move_cursor(1, 0); return;
  case KEY_UP:    move_cursor(0, -1); return;
  case KEY_DOWN:  move_cursor(0, 1); return;
  }
  if (k >= 0x3B00 && k <= 0x4400) { tool_ix = fkey_tool[(k - 0x3B00) >> 8]; return; }
  c = k & 0xFF;
  if (c == 0) return;
  if (c >= 'a' && c <= 'z') c -= 32;
  for (i = 0; i < (int)NTOOLS; i++) if (tools[i].key == c) { tool_ix = i; return; }
  switch (c) {
  case 27: prompt = 2; break;
  case 13: place(); break;
  case ' ': speed = speed ? speed : 3; paused = !paused; setSpeed(paused ? 0 : speed); break;
  case 'D': drag = !drag; break;
  case 'G': prompt = 1; plen = 0; pbuf[0] = 0; break;
  case 'H': case '?': show_help(); break;
  case 'M': show_budget(0); break;
  case 'V': show_eval(); break;
  case 'J': show_graphs(); break;
  case 'Z': prompt = 3; break;
  case 'S': prompt = 4; pfresh = 1; strcpy(pbuf, fname); plen = strlen(pbuf); break;
  case 'L': prompt = 5; pfresh = 1; strcpy(pbuf, fname); plen = strlen(pbuf); break;
  case 'O': snd_on = !snd_on; if (!snd_on) snd_stop(); say(snd_on ? "Sound on" : "Sound off"); snd_play(S_CHIME); cfg_save(); break;
  case 9: case 'I': show_settings(); break;
  case 'X': nodis = !nodis; SetDisasters(!nodis); say(nodis ? "Disasters off" : "Disasters ON"); cfg_save(); break;
  case '-': case '_': if (speed > 1) speed--; paused = 0; set_speed(); break;
  case '+': case '=': if (speed < 3) speed++; paused = 0; set_speed(); break;
  }
}

static void refresh(void)
{
  scroll_to_cursor();
  cursor_hide();
  spr_prepare();
  tiles_drawn += draw_map();
  spr_draw();
  cursor_show();
  if (strcmp(simmsg, LastMessage)) { strncpy(simmsg, LastMessage, sizeof simmsg - 1); simmsg[sizeof simmsg - 1] = 0; simmsg_cs = now_cs(); }
  status_update();
  ui_draws++;
}

#define UI_CS 10          /* redraw at most every 100 ms of DOS time while the sim runs */
static unsigned long last_ui;
static unsigned minstk = 0xFFFF;
static void ui_tick(void)
{
  int k;
  { unsigned a = stackavail(); if (a < minstk) minstk = a; }
  if (snd_test) snd_testtick();
  if (ystop && CityTime >= ystop_t) quit_now();
  if (elapsed(last_ui) < UI_CS) return;
  while ((k = kbd_poll()) != -1) handle_key(k);
  refresh();
  last_ui = now_cs();
}

static unsigned long load_cs, tiles_cs, city_cs;
static int sprdemo_g;
static unsigned free0, free1, free2, free3, free4;
extern unsigned arena_left, near_left;
extern unsigned _STACKLOW, _STACKTOP;
static void stack_fill(void) { unsigned sp = (unsigned)&sp - 96; if (sp > _STACKLOW) _fmemset(MK_FP(FP_SEG(&sp), _STACKLOW), 0xAA, sp - _STACKLOW); }
static unsigned stack_used(void) { unsigned char far *p = (unsigned char far *)MK_FP(FP_SEG(&p), _STACKLOW); unsigned n = 0; while (p[n] == 0xAA && n < 8000) n++; return _STACKTOP - _STACKLOW - n; }
static void run_benchmark(void)
{
  unsigned long t0; unsigned i, n; unsigned long drawn;
  printf("LOAD total %lu ms (city/new %lu, tiles %lu)\n", load_cs * 10UL, city_cs * 10UL, tiles_cs * 10UL);
  printf("MODE %s tileseg %04x pstep %u\n", mono ? "mono" : "color", tileseg, pstep);
  printf("MEM free paras: start %u, after initMapArrays %u, after city %u; arena left %u, near left %u\n", free0, free1, free2, arena_left, near_left);
  printf("MEM free after tiles+city (before any printf): %u paras = %lu bytes\n", free3, (unsigned long)free3 * 16UL);
  printf("MEM free after sprite art: %u paras = %lu bytes (art %lu bytes, mask %02x) %s\n", free4, (unsigned long)free4 * 16UL, spr_bytes, SpriteArt, sprmsg);
  wx = 40; wy = 34; cx = wx + 22; cy = wy + 16;
  if (sprdemo_g) spr_demo();
  MapScanYield = ui_tick;
  /* full redraws */
  n = 20; drawn = 0; t0 = now_cs();
  for (i = 0; i < n; i++) { invalidate(); drawn += draw_map(); }
  printf("FULLDRAW %u redraws of %lu tiles: %lu cs total, %lu ms each\n", n, drawn / n, elapsed(t0), elapsed(t0) * 10UL / n);
  if (sprdemo_g) {   /* the same with the 7 demo sprites on screen: tile pass plus sprite merge */
    n = 20; drawn = 0; t0 = now_cs();
    for (i = 0; i < n; i++) { invalidate(); spr_prepare(); drawn += draw_map(); spr_draw(); }
    printf("FULLDRAW+SPRITES %u redraws: %lu ms each\n", n, elapsed(t0) * 10UL / n);
    n = 20; t0 = now_cs();
    for (i = 0; i < n; i++) { spr_prepare(); spr_draw(); }
    printf("SPRITE MERGE only (all sprites in the list, unchanged): %lu ms per refresh\n", elapsed(t0) * 10UL / n);
  }
  /* nothing changed: compare pass only */
  n = 60; drawn = 0; t0 = now_cs();
  for (i = 0; i < n; i++) drawn += draw_map();
  printf("CLEANPASS %u passes, %lu tiles drawn: %lu ms each\n", n, drawn, elapsed(t0) * 10UL / n);
  /* sim running: dirty-tile redraw after real sim phases */
  { unsigned long simcs = 0, drawcs = 0, d = 0; unsigned frames = 0, f;
    long c0 = CityTime;
    autoBudget = 1; autoBulldoze = 1; SetDisasters(0); setSpeed(3);
    while (CityTime - c0 < 8) {
      t0 = now_cs(); for (f = 0; f < 16; f++) { SimFrame(); MoveObjects(); } simcs += elapsed(t0); frames += 16;
      t0 = now_cs(); spr_prepare(); d += draw_map(); spr_draw(); drawcs += elapsed(t0);
    }
    printf("SIM %u phases %lu cs; %lu tiles redrawn in %lu cs (%lu ms per tile batch of 16 phases)\n",
           frames, simcs, d, drawcs, drawcs * 10UL / (frames / 16));
  }
  printf("STACK deepest use %u of %u (yield-point min free %u)\n", stack_used(), (unsigned)(_STACKTOP - _STACKLOW), minstk);
}

int main(int argc, char **argv)
{
  int i, sprdemo = 0, sprfind = 0; unsigned long t;
  const char *city = 0;
  long seed = -1;
  printf("%Fs", banner);

  cfg_load();
  for (i = 1; i < argc; i++) {
    if (!stricmp(argv[i], "M")) mono = 1;
    else if (!stricmp(argv[i], "B")) bench = 1;
    else if (!stricmp(argv[i], "C")) csum_mode = 1;
    else if (!stricmp(argv[i], "P")) start_paused = 1;
    else if (!stricmp(argv[i], "T")) snd_test = 1;
    else if (!stricmp(argv[i], "NOSPR")) spr_pref = 0;       /* sprites off / on, overriding MP.CFG (tests) */
    else if (!stricmp(argv[i], "SPR")) spr_pref = 1;
    else if (!stricmp(argv[i], "SD")) { spr_pref = 1; sprdemo = 1; }
    else if ((argv[i][0] == 'S' || argv[i][0] == 's') && (argv[i][1] == 'F' || argv[i][1] == 'f') && argv[i][2] >= '1' && argv[i][2] <= '3') { spr_pref = 1; sprfind = argv[i][2] - '0'; }   /* test hook: one of each sprite near the cursor */
    else if ((argv[i][0] == 'Y' || argv[i][0] == 'y') && argv[i][1] >= '0' && argv[i][1] <= '9') ystop = atoi(argv[i] + 1);
    else if (argv[i][0] == 'N' || argv[i][0] == 'n') seed = atol(argv[i] + 1);   /* N or N1234: new city */
    else city = argv[i];
  }
  stack_fill();
  t = now_cs();
  free0 = dos_free_paras();
  sim_srand(1);
  initMapArrays();
  free1 = dos_free_paras();
  if (city && seed >= 0) city = 0;      /* N wins over a file name (the file stays on the disk for L) */
  if (city) {
    if (!loadFile((char *)city)) { printf("cannot load %s\n", city); return 2; }
    StartingYear = 1900;
    load_sum = tmap_sum(); load_t = (long)CityTime; load_f = (long)TotalFunds; have_load = 1;
    if (strlen(city) < sizeof fname) strcpy(fname, city);
  }
  _fheapshrink(); _nheapshrink();                 /* give the file buffers back to DOS before the tiles are loaded */
  city_cs = elapsed(t);
  free2 = dos_free_paras();
  if (!load_tiles(mono ? "TILES1.BIN" : "TILES8.BIN")) {
    int h;
    if (_dos_open(mono ? "TILES1.BIN" : "TILES8.BIN", O_RDONLY, &h)) printf("Can't find %s. Run Micropolis from the disk that holds it.\n", mono ? "TILES1.BIN" : "TILES8.BIN");
    else {
      _dos_close(h);
      if (mono) printf("Not enough memory. Micropolis needs 256K of RAM in mono.\n");
      else printf("Not enough memory. Micropolis in color needs 512K of RAM.\nWith 256K, start it in mono: MONO\n");
    }
    return 3;
  }
  tiles_cs = elapsed(t) - city_cs;
  free3 = dos_free_paras();
  load_sprites(mono ? "SPR1.BIN" : "SPR8.BIN");
  free4 = dos_free_paras();
  if (city) {
    long sx = 0, sy = 0, n = 0; int x, y;           /* start on the centre of the developed area */
    SetDisasters(!nodis); autoBulldoze = autobull; setSpeed(speed);
    for (x = 0; x < WORLD_X; x++) for (y = 0; y < WORLD_Y; y++)
      if ((MapData[x * WORLD_Y + y] & LOMASK) >= 64) { sx += x; sy += y; n++; }
    cx = n ? (int)(sx / n) : WORLD_X / 2; cy = n ? (int)(sy / n) : WORLD_Y / 2;
    wx = cx + WIN_W; wy = cy + WIN_H;               /* off-window, so scroll_to_cursor recentres */
  } else {
    new_city(seed >= 0 ? (unsigned)seed : (unsigned)(now_cs() & 0x7FFF));
    setSpeed(speed);
  }
  spr_apply();
  if (start_paused) { paused = 1; setSpeed(0); }
  if (sprdemo) { spr_demo(); sprdemo_g = 1; }
  if (sprfind) spr_find(sprfind);
  load_cs = elapsed(t);
  video_init();
  invalidate();
  if (bench) { run_benchmark(); video_exit(); return 0; }
  asm_snd_init();                       /* the program must leave through quit_now(), which unhooks the timer tick */
  last_ui = now_cs(); snd_tt = last_ui; ystop_t = CityTime + ystop * 48L;
  refresh();
  MapScanYield = ui_tick;
  BudgetHook = budget_hook;
  for (;;) {
    SimFrame();
    MoveObjects();          /* the original calls it once per simulation loop (sprites off or none: returns at once) */
    ui_tick();
  }
}
