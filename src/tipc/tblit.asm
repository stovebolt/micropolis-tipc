; tblit.asm: TIPC map blitter for Micropolis for TIPC (NASM, OMF, Open Watcom large model, __cdecl).
; Code by Shurr Nuff (AI assistant), produced by Will (stovebolt), Flying Horse Labs. GPL v3.
;
; Draws the 45x32 tile window.  Tiles are 16x9 pixels = one word per row, so every tile lands on a
; word boundary and a movsw copies a whole pixel row of the tile into one plane (bytes are swapped
; inside each word on the board, which the word-wide store already accounts for).
; Plane stride 92 bytes per pixel row; screen row of tile row ty is ty*9, so ty*828 bytes.
;
; unsigned asm_draw_map(unsigned mapoff, unsigned shownoff, unsigned tileseg, unsigned pstep)
;   mapoff   near offset (DGROUP) of MapData[wx*100+wy]; column-major, 100 shorts per column
;   shownoff near offset of unsigned shown[32*45]; a tile is redrawn only when it differs
;   tileseg  segment of the loaded TILES file (header+table at 0, table at +12: u16 offsets)
;   pstep    bytes between plane A data and plane B data, minus one tile (18)
; Returns the number of tiles drawn.  asm_draw_map1 does the same for one plane (mono).
; DGROUP data is addressed through SS (SS = DGROUP here); DS holds the tile segment inside the loop.

cpu 8086
bits 16
segment TBLIT_TEXT class=CODE public use16
global _asm_draw_map
global _asm_draw_map1

%define COLS 45
%define ROWS 32

v_rowmap dw 0
v_shown  dw 0
v_rowdst dw 0
v_rows   dw 0
v_pstep  dw 0
v_cnt    dw 0

%macro PLANE 1
	mov ax, %1
	mov es, ax
	mov di, dx
	%rep 8
	movsw
	add di, 90
	%endrep
	movsw
%endmacro

%macro DRAWMAP 2          ; %1 = label base, %2 = 3 for colour, 1 for mono
	push bp
	mov bp, sp
	push si
	push di
	push ds
	push es
	mov ax, [bp+6]
	mov [cs:v_rowmap], ax
	mov ax, [bp+8]
	mov [cs:v_shown], ax
	mov ax, [bp+12]
	mov [cs:v_pstep], ax
	mov ds, [bp+10]
	xor ax, ax
	mov [cs:v_rowdst], ax
	mov [cs:v_cnt], ax
	mov word [cs:v_rows], ROWS
	cld
%1_row:
	mov bx, [cs:v_rowmap]
	mov bp, [cs:v_shown]
	mov dx, [cs:v_rowdst]
	mov cx, COLS
%1_tile:
	mov ax, [ss:bx]
	and ax, 03FFh
	cmp ax, [ss:bp]
	jne %1_draw
	jmp %1_skip
%1_draw:
	mov [ss:bp], ax
	inc word [cs:v_cnt]
	mov si, ax
	shl si, 1
	mov si, [si+12]
	PLANE 0C000h
%if %2 == 3
	add si, [cs:v_pstep]
	PLANE 0C800h
	add si, [cs:v_pstep]
	PLANE 0D000h
%endif
%1_skip:
	add bx, 200
	add bp, 2
	add dx, 2
	dec cx
	jz %1_rowend
	jmp %1_tile
%1_rowend:
	mov [cs:v_shown], bp
	add word [cs:v_rowmap], 2
	add word [cs:v_rowdst], 828
	dec word [cs:v_rows]
	jz %1_done
	jmp %1_row
%1_done:
	mov ax, [cs:v_cnt]
	pop es
	pop ds
	pop di
	pop si
	pop bp
	retf
%endmacro

_asm_draw_map:
	DRAWMAP c3, 3
_asm_draw_map1:
	DRAWMAP c1, 1

; ------------------------------------------------------------------------------------------------
; Sprites: masked blit of one pre-scaled sprite frame at any pixel position.
;
; void asm_spr_blit(unsigned srcseg, unsigned srcoff, unsigned ww, unsigned rows, unsigned psz,
;                   int x, int y, unsigned nplanes)
;   srcseg:srcoff  the frame: mask array, then one array per plane; an array is rows*ww words (row-major, pixel 0 = bit 15)
;   ww rows        frame size in words per row and rows
;   psz            bytes of one array (rows*ww*2)
;   x y            screen pixel of the frame's top-left corner in the 720x288 map window (may be negative / partly outside)
;   nplanes        3 (colour) or 1 (mono)
; Per row: the mask row is scanned for its first and last non-zero word (an empty row costs almost nothing, and the words
; outside are never touched), the n words in between are shifted right by x&15 into n+1 words (mask, then each plane),
; clipped to the window (45 words x 288 rows) and merged: dest = (dest & ~mask) | (pixels & mask).
; CL = x&15 and CH = 16-(x&15) stay in CX for the whole call (a shift by 16 clears the word: the x&15 = 0 case needs no code).
; ------------------------------------------------------------------------------------------------
global _asm_spr_blit
s_ww    dw 0
s_rows  dw 0
s_psz   dw 0
s_np    dw 0
s_wc0   dw 0           ; x >> 4 (arithmetic): destination word column of source word 0, may be negative
s_y     dw 0           ; screen row of the current sprite row
s_row   dw 0           ; source offset of the current row's mask words
s_k0    dw 0           ; byte offset of the first non-zero mask word of the row
s_n     dw 0           ; number of source words from there to the last non-zero one
s_j0    dw 0           ; first output word to merge (bytes into the temp buffers)
s_cnt   dw 0           ; number of output words to merge
s_di0   dw 0           ; destination offset of the first merged word (plane row start + 2*column)
s_plane dw 0
s_seg   dw 0
s_tm    times 5 dw 0   ; shifted mask (n+1 <= 4 words)
s_tp    times 5 dw 0   ; shifted plane pixels
s_planeseg dw 0C000h, 0C800h, 0D000h

; ds:si = n source words (n in s_n), -> n+1 words at cs:%1.  Uses ax bx dx di bp; keeps cx.
%macro SHIFTN 1
	mov bp, [cs:s_n]
	xor dx, dx
	xor bx, bx
%%l:
	lodsw
	mov di, ax
	shr ax, cl
	or ax, dx
	mov [cs:%1+bx], ax
	mov ax, di
	xchg cl, ch
	shl ax, cl
	xchg cl, ch
	mov dx, ax
	add bx, 2
	dec bp
	jnz %%l
	mov [cs:%1+bx], dx
%endmacro

_asm_spr_blit:
	push bp
	mov bp, sp
	push si
	push di
	push ds
	push es
	mov ax, [bp+6]
	mov [cs:s_seg], ax
	mov ax, [bp+8]
	mov [cs:s_row], ax
	mov ax, [bp+10]
	mov [cs:s_ww], ax
	mov ax, [bp+12]
	mov [cs:s_rows], ax
	mov ax, [bp+14]
	mov [cs:s_psz], ax
	mov ax, [bp+20]
	mov [cs:s_np], ax
	mov ax, [bp+18]
	mov [cs:s_y], ax
	mov ax, [bp+16]
	mov cl, 4
	mov bx, ax
	sar ax, cl
	mov [cs:s_wc0], ax
	and bl, 15
	mov cl, bl                  ; cl = x & 15
	mov ch, 16
	sub ch, cl                  ; ch = 16 - (x & 15)
	cld
.row:
	mov ax, [cs:s_y]
	cmp ax, 288
	jb .inrow
	cmp ax, 8000h               ; negative rows are >= 8000h: skip them
	jae .skiprow
	jmp .done                   ; below the window: all later rows too
.inrow:
	; first and last non-zero mask word of this row
	mov ds, [cs:s_seg]
	mov si, [cs:s_row]
	mov bx, -1
	xor dx, dx
	xor di, di
	mov bp, [cs:s_ww]
.scan:
	lodsw
	or ax, ax
	jz .sz
	cmp bx, -1
	jne .s1
	mov bx, di
.s1:
	mov dx, di
.sz:
	add di, 2
	dec bp
	jnz .scan
	cmp bx, -1
	je .skiprow
	mov [cs:s_k0], bx
	sub dx, bx
	shr dx, 1
	inc dx
	mov [cs:s_n], dx            ; n source words, n+1 output words
	; output words j = 0..n sit in word column col+j, col = wc0 + k0/2; keep those with 0 <= col+j < 45
	mov ax, bx
	shr ax, 1
	add ax, [cs:s_wc0]          ; col
	mov bx, 0                   ; j0 (words)
	or ax, ax
	jns .c1
	mov bx, ax
	neg bx                      ; j0 = -col
.c1:
	mov dx, 44
	sub dx, ax                  ; last allowed j = 44 - col (as signed)
	mov di, [cs:s_n]            ; last output j = n
	cmp dx, di
	jge .c2
	mov di, dx
.c2:
	sub di, bx
	jl .skiprow                 ; nothing of this row is inside the window
	inc di
	mov [cs:s_cnt], di
	add ax, bx                  ; first merged column
	shl ax, 1
	mov dx, ax                  ; 2*column
	shl bx, 1
	mov [cs:s_j0], bx
	mov ax, [cs:s_y]
	push dx
	mov dx, 92
	mul dx
	pop dx
	add ax, dx
	mov [cs:s_di0], ax
	; shifted mask
	mov si, [cs:s_row]
	add si, [cs:s_k0]
	SHIFTN s_tm
	mov word [cs:s_plane], 0
.plane:
	mov ax, [cs:s_plane]
	inc ax
	mul word [cs:s_psz]
	add ax, [cs:s_row]
	add ax, [cs:s_k0]
	mov si, ax                  ; the plane array's words for this row
	SHIFTN s_tp
	mov bx, [cs:s_plane]
	shl bx, 1
	mov es, [cs:s_planeseg+bx]
	mov bx, [cs:s_j0]
	mov di, [cs:s_di0]
	mov bp, [cs:s_cnt]
.m:
	mov dx, [cs:s_tm+bx]
	mov ax, [cs:s_tp+bx]
	and ax, dx
	not dx
	and dx, [es:di]
	or ax, dx
	stosw
	add bx, 2
	dec bp
	jnz .m
	inc word [cs:s_plane]
	mov ax, [cs:s_plane]
	cmp ax, [cs:s_np]
	jb .plane
.skiprow:
	mov ax, [cs:s_ww]
	shl ax, 1
	add [cs:s_row], ax
	inc word [cs:s_y]
	dec word [cs:s_rows]
	jz .done
	jmp .row
.done:
	pop es
	pop ds
	pop di
	pop si
	pop bp
	retf
