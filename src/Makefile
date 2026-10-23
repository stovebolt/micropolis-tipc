SRC = s_alloc.c s_disast.c s_eval.c s_fileio.c s_gen.c s_init.c s_msg.c s_power.c s_scan.c s_sim.c s_traf.c s_zone.c s_sprite.c glue.c driver.c
simtest: $(SRC) headers/*.h
	gcc -std=gnu89 -O1 -w -fwrapv -Iheaders -o simtest $(SRC) -lm
