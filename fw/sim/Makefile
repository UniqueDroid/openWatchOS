# openWatchOS host simulator. LVGL v8.3.6 (the version of the watch SDK) is not part of this repository:
#   git clone --depth 1 --branch v8.3.6 https://github.com/lvgl/lvgl.git ../../../lvgl   (or set LVGL=...)
#   make && ./sim FACE.di out/    (writes activity, face, apps, control, notes as PPM)
LVGL ?= ../../../lvgl
CC ?= gcc
CFLAGS ?= -O2 -g -Wall -Wno-unused-parameter
SRC := sim.c ../ui/ow_ui.c ../owf/owf.c $(wildcard ../ui/fonts/*.c) $(shell find $(LVGL)/src -name '*.c')

sim: $(SRC) lv_conf.h ../ui/ow_ui.h ../owf/owf.h
	$(CC) $(CFLAGS) -DLV_CONF_INCLUDE_SIMPLE -DLV_LVGL_H_INCLUDE_SIMPLE -I. -I$(LVGL) -I$(LVGL)/.. -o $@ $(SRC)

clean:
	rm -f sim
