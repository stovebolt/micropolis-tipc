/* headless stubs: no X11/Tk views; sprites are real (s_sprite.c) */
#include <errno.h>
struct SimSprite { struct SimSprite SPRNEAR *next; short type, frame, x, y, x_offset, y_offset, x_hot, y_hot, orig_x, orig_y, dest_x, dest_y,
  count, sound_count, dir, new_dir, step, flag, control; };
struct SimView { struct SimView *next; int map_state, tool_state, tool_state_save; };
struct Sim { SimView *editor, *map; SimSprite SPRNEAR *sprite; };
extern Sim *sim;
#define ckalloc(n) malloc(n)
#define ckfree(p) free(p)
int sim_rand(void); QUAD sim_rand32(void); unsigned QUAD sim_entropy(void); short Rand(short); int Rand16(void); int Rand16Signed(void); void sim_srand(unsigned int);
int SetFunds(QUAD); int Spend(QUAD);
int loadFile(char *filename);
/* TIPC speed pass: whole 120x100 map in one block, column-major: MapBase[x*WORLD_Y+y] == Map[x][y] */
extern short MapData[];
extern short PowerMapData[];
#define MapBase MapData
/* MapCol[x] == x * WORLD_Y; MAPXY(x,y) is Map[x][y] without the far row pointer */
extern short MapCol[];
#define MAPXY(x, y) MapData[MapCol[x] + (y)]

/* TIPC speed pass: tables replace variable shifts (8086 shl r,cl is 4 clocks per bit).
   PwrXOff[x] = byte offset of the power-map word for column x, PwrXMask[x] = its bit,
   PwrYOff[y] = byte offset of row y. Filled in initMapArrays. */
extern short PwrXOff[], PwrXMask[], PwrYOff[];

/* RNG state as two words, and an inline (no call) 16-bit RNG for the 8086 build. */
#if defined(__WATCOMC__) && !defined(NOASM) && !defined(SIMTRACE) && !defined(NOINL)
extern unsigned short rlo, rhi;
unsigned short sim_rand16(void);
#pragma aux sim_rand16 = \
  "mov ax, ss:rlo" "mov bx, 4E6Dh" "mul bx" "mov cx, ax" "mov si, dx" \
  "mov ax, ss:rhi" "mul bx" "add si, ax" "mov ax, ss:rlo" "mov bx, 41C6h" "mul bx" "add si, ax" \
  "add cx, 3039h" "adc si, 0" "mov ss:rlo, cx" "mov ss:rhi, si" "mov al, ch" "mov bx, si" "mov ah, bl" \
  parm [] value [ax] modify [ax bx cx dx si];
#define FASTRAND() sim_rand16()
#define FASTRAND_INLINE
#define Rand16() ((int)sim_rand16())
#else
#define FASTRAND() ((unsigned short)sim_rand32())
#endif

/* s_sprite.c (prototypes needed on 16-bit: far pointer returns) */
SimSprite SPRNEAR *GetSprite(int type);
SimSprite SPRNEAR *MakeSprite(int type, int x, int y);
SimSprite SPRNEAR *MakeNewSprite(int type, int x, int y);
SimSprite SPRNEAR *NewSprite(int type, int x, int y);
int InitSprite(SimSprite SPRNEAR *sprite, int x, int y);
int DestroyAllSprites(void);
int DestroySprite(SimSprite SPRNEAR *sprite);
int MoveObjects(void);
void SpritesSet(int on);
void SetDisasters(int on);
extern short DisasterSprites;
extern short SpritesOn;               /* trains, ships, planes, helicopter (and their explosions) */
extern unsigned short SpriteArt;      /* bit (1<<type) set: art for that sprite type is available */
extern SimSprite SPRNEAR *GlobalSprites[];
extern unsigned short SpriteMade[];
