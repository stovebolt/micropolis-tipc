; snd.asm: TIPC speaker sound for Micropolis for TIPC (NASM, OMF, Open Watcom large model, __cdecl, 8086 only).
; Code by Shurr Nuff (AI assistant), produced by Will (stovebolt), Flying Horse Labs. GPL v3.
;
; The sequencer runs from the ROM's 25 ms timer tick: the ROM calls INT 58H on every tick (TechRef: "58H hook every
; tick; hooks may use AX,BX,DI,ES", default = dummy).  We chain a hook there, so a sound steps on in the background
; and never waits for the sim, the disk or the screen.  The speaker itself is the 8253 counter 0 (divisor = 1.25 MHz /
; Hz, port 17h control, port 14h data) gated by port 00h bit 0 (SPKEN); the ROM keeps a shadow of port 00h at
; [ROMDS:0000] (ROMDS = ES from INT 48H AH=9), which we keep in step.
;
; void asm_snd_init(void)     hook INT 58H (also reads ROMDS)       void asm_snd_exit(void)   silence and unhook
; void asm_snd_play(unsigned id)  start effect id (0..11), queue it behind a playing one, or drop clicks if busy
; void asm_snd_stop(void)     silence now and clear the queue       unsigned asm_snd_busy(void)  nonzero while sounding/queued
;
; An effect is a list of 8-byte segments: dw d0, d1 (divisor sweep start/end, 0 = rest), db n (steps, 0 ends the
; effect), db ticks (25 ms per step), db jit (random wobble, 1/256 units of the divisor), db 0.

cpu 8086
bits 16
segment SND_TEXT class=CODE public use16
global _asm_snd_init
global _asm_snd_exit
global _asm_snd_play
global _asm_snd_stop
global _asm_snd_busy

%define F(h) (1250000 / (h))

romds  dw 0
oldv   dw 0, 0
inst   db 0
spk    db 0                     ; speaker currently gated on
cid    db 0                     ; id of the effect playing
pend   db 0FFh                  ; queue of two
pend2  db 0FFh
p      dw 0                     ; next segment, 0 = idle
d      dw 0                     ; current divisor of the sweep
delta  dw 0
nleft  db 0                     ; steps left in this segment
tleft  db 0                     ; ticks left in this step
curt   db 0
jit    db 0
rnd    dw 1

; ---- effect table: ids 0 click, 1 bulldoze, 2 error, 3 chime, 4-6 honk honk low/med/high, 7 siren, 8 monster, 9 explosion high, 10 low, 11 helicopter (traffic report)
fxtab  dw fx_click, fx_doze, fx_err, fx_chime, fx_honkl, fx_honkm, fx_honkh, fx_siren, fx_monster, fx_exph, fx_expl, fx_chop
%macro SEG 5
	dw %1, %2
	db %3, %4, %5, 0
%endmacro
%macro END 0
	dw 0, 0
	db 0, 0, 0, 0
%endmacro
fx_click:   SEG F(1500), F(1500), 1, 1, 0
            SEG F(2200), F(2200), 1, 1, 0
            END
fx_doze:    SEG F(180), F(110), 6, 1, 70
            END
fx_err:     SEG F(260), F(260), 1, 4, 0
            SEG 0, 0, 1, 2, 0
            SEG F(260), F(260), 1, 4, 0
            END
fx_chime:   SEG F(660), F(660), 1, 3, 0
            SEG F(880), F(880), 1, 3, 0
            SEG F(1320), F(1320), 1, 6, 0
            END
fx_honkl:   SEG F(280), F(280), 1, 4, 0
            SEG 0, 0, 1, 2, 0
            SEG F(280), F(280), 1, 4, 0
            END
fx_honkm:   SEG F(380), F(380), 1, 4, 0
            SEG 0, 0, 1, 2, 0
            SEG F(380), F(380), 1, 4, 0
            END
fx_honkh:   SEG F(500), F(500), 1, 4, 0
            SEG 0, 0, 1, 2, 0
            SEG F(500), F(500), 1, 4, 0
            END
fx_siren:   SEG F(500), F(900), 6, 2, 0
            SEG F(900), F(500), 6, 2, 0
            SEG F(500), F(900), 6, 2, 0
            SEG F(900), F(500), 6, 2, 0
            END
fx_monster: SEG F(110), F(70), 10, 2, 90
            SEG F(70), F(150), 6, 2, 90
            SEG F(150), F(50), 8, 2, 120
            END
fx_exph:    SEG F(900), F(120), 16, 1, 150
            END
fx_expl:    SEG F(220), F(40), 24, 1, 160
            END
fx_chop:    SEG F(95), F(80), 2, 1, 60
            SEG 0, 0, 1, 1, 0
            SEG F(95), F(80), 2, 1, 60
            SEG 0, 0, 1, 1, 0
            SEG F(95), F(80), 2, 1, 60
            SEG 0, 0, 1, 1, 0
            SEG F(95), F(80), 2, 1, 60
            SEG 0, 0, 1, 1, 0
            SEG F(95), F(80), 2, 1, 60
            SEG 0, 0, 1, 1, 0
            SEG F(95), F(80), 2, 1, 60
            END

; ---- speaker gate (shadow of port 00h bit 0)
gate_on:
	push ax
	push es
	mov es, [cs:romds]
	pushf
	cli
	mov al, [es:0]
	or al, 1
	mov [es:0], al
	out 0, al
	popf
	mov byte [cs:spk], 1
	pop es
	pop ax
	ret
gate_off:
	cmp byte [cs:spk], 0
	je .r
	push ax
	push es
	mov es, [cs:romds]
	pushf
	cli
	mov al, [es:0]
	and al, 0FEh
	mov [es:0], al
	out 0, al
	popf
	mov byte [cs:spk], 0
	pop es
	pop ax
.r:	ret

; ---- one step of the sequencer (interrupts may be on or off; clobbers AX BX CX DX SI)
adv:
	cmp byte [cs:nleft], 0
	jz .newseg
	dec byte [cs:nleft]
	mov ax, [cs:d]
	add ax, [cs:delta]
	mov [cs:d], ax
	jmp .apply
.newseg:
	mov si, [cs:p]
	cmp byte [cs:si+4], 0
	jne .load
	mov word [cs:p], 0              ; end of effect
	call gate_off
	mov al, [cs:pend]
	cmp al, 0FFh
	je .ret
	mov ah, [cs:pend2]
	mov [cs:pend], ah
	mov byte [cs:pend2], 0FFh
	jmp start
.load:
	mov ax, [cs:si]
	mov [cs:d], ax
	mov bx, [cs:si+2]
	mov cl, [cs:si+4]
	mov dl, [cs:si+5]
	mov [cs:curt], dl
	mov dl, [cs:si+6]
	mov [cs:jit], dl
	add word [cs:p], 8
	dec cl
	mov [cs:nleft], cl
	mov word [cs:delta], 0
	or cl, cl
	jz .apply
	sub bx, ax                      ; (d1 - d0) / (n - 1)
	mov ax, bx
	cwd
	xor ch, ch
	idiv cx
	mov [cs:delta], ax
.apply:
	mov al, [cs:curt]
	mov [cs:tleft], al
	mov ax, [cs:d]
	or ax, ax
	jnz .tone
	call gate_off
.ret:	ret
.tone:
	mov cl, [cs:jit]
	or cl, cl
	jz .set
	mov bx, [cs:rnd]                ; rnd = rnd * 25173 + 13849 (a different generator from the sim's)
	mov ax, 25173
	mul bx
	add ax, 13849
	mov [cs:rnd], ax
	mov al, ah
	sub al, 128                     ; -128..127
	cbw
	xor ch, ch
	imul cx                         ; * jit
	mov cl, 7
	sar ax, cl
	mov bx, [cs:d]
	mov cl, 8
	shr bx, cl
	imul bx                         ; wobble = (x * jit >> 7) * (d >> 8)
	add ax, [cs:d]
	cmp ax, 100
	jae .ok
	mov ax, 100
	jmp .ok
.set:	mov ax, [cs:d]
.ok:	mov bx, ax
	pushf
	cli
	mov al, 36h                     ; counter 0, LSB then MSB, mode 3
	out 17h, al
	mov al, bl
	out 14h, al
	mov al, bh
	out 14h, al
	popf
	cmp byte [cs:spk], 0
	jne .r2
	call gate_on
.r2:	ret

start:                                  ; AL = id
	mov [cs:cid], al
	xor ah, ah
	mov bx, ax
	shl bx, 1
	mov ax, [cs:fxtab+bx]
	mov [cs:p], ax
	mov byte [cs:nleft], 0
	jmp adv

; ---- INT 58H, every 25 ms
hook:
	cmp word [cs:p], 0
	je .chain
	push ax
	push bx
	push cx
	push dx
	push si
	dec byte [cs:tleft]
	jnz .out
	call adv
.out:	pop si
	pop dx
	pop cx
	pop bx
	pop ax
.chain:	jmp far [cs:oldv]

; ---- C interface
_asm_snd_init:
	push bp
	push es
	cmp byte [cs:inst], 0
	jne .done
	mov ah, 9                       ; ROM: ES:BX = config word, ES = the ROM's data segment
	int 48h
	mov [cs:romds], es
	xor ax, ax
	mov es, ax
	mov ax, [es:160h]
	mov [cs:oldv], ax
	mov ax, [es:162h]
	mov [cs:oldv+2], ax
	mov byte [cs:inst], 1
	cli
	mov word [es:160h], hook
	mov [es:162h], cs
	sti
.done:	pop es
	pop bp
	retf

_asm_snd_exit:
	push es
	cmp byte [cs:inst], 0
	je .r
	call _asm_snd_stop_near
	xor ax, ax
	mov es, ax
	cli
	mov ax, [cs:oldv]
	mov [es:160h], ax
	mov ax, [cs:oldv+2]
	mov [es:162h], ax
	mov byte [cs:inst], 0
	sti
.r:	pop es
	retf

_asm_snd_stop_near:
	pushf
	cli
	mov word [cs:p], 0
	mov byte [cs:pend], 0FFh
	mov byte [cs:pend2], 0FFh
	call gate_off
	popf
	ret
_asm_snd_stop:
	cmp byte [cs:inst], 0
	je .r
	call _asm_snd_stop_near
.r:	retf

_asm_snd_busy:
	xor ax, ax
	cmp word [cs:p], 0
	jne .y
	cmp byte [cs:pend], 0FFh
	je .r
.y:	inc ax
.r:	retf

_asm_snd_play:
	push bp
	mov bp, sp
	push si
	push bx
	push cx
	push dx
	pushf
	cli
	cmp byte [cs:inst], 0
	je .ret
	mov ax, [bp+6]
	cmp word [cs:p], 0
	je .go
	cmp al, 1
	jbe .ret                        ; clicks never interrupt or queue
	cmp byte [cs:cid], 1
	jbe .go                         ; a click is playing: the real effect takes over
	cmp byte [cs:pend], 0FFh
	jne .q2
	mov [cs:pend], al
	jmp .ret
.q2:	cmp byte [cs:pend2], 0FFh
	jne .ret
	mov [cs:pend2], al
	jmp .ret
.go:	call start
.ret:	popf
	pop dx
	pop cx
	pop bx
	pop si
	pop bp
	retf
