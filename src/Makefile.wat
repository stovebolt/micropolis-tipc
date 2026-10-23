# Open Watcom build for 8086 DOS (large model). Run: make -f Makefile.wat
SRC = s_alloc.c s_disast.c s_eval.c s_fileio.c s_gen.c s_init.c s_msg.c s_power.c s_scan.c s_sim.c s_traf.c s_zone.c s_sprite.c glue.c driver.c
CFLAGS = -q -bt=dos -ml -0 -fpi -ox -w1 -zp2 -DMSDOS -Iheaders -fe=SIMTEST.EXE -fm=SIMTEST.MAP -k8192
SIMTEST.EXE: $(SRC) headers/*.h
	wcl $(CFLAGS) $(SRC)
