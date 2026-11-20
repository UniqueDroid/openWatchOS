# openWatchOS host simulator. LVGL v9.6.0 is not part of this repository:
#   git clone --depth 1 --branch v9.6.0 https://github.com/lvgl/lvgl.git ../../../lvgl   (or set LVGL=...)
#   make && OW_ROOT=storage OW_STORE=store ./sim out/
LVGL ?= ../../../lvgl
CC ?= gcc
CFLAGS ?= -O2 -g -Wall -Wno-unused-parameter
FW := sim.c ow_sys_sim.c ../sys/ow_time.c ../ui/ow_ui.c ../ui/ow_widgets.c ../owf/owf.c $(wildcard ../ui/fonts/*.c) \
      ../apps/ow_app.c ../apps/ow_json.c ../apps/ow_pkg.c $(wildcard ../apps/builtin/*.c)
JS := ../js/hw3_js.c ../js/mquickjs/mquickjs.c ../js/mquickjs/cutils.c ../js/mquickjs/dtoa.c ../js/mquickjs/libm.c
LV := $(shell find $(LVGL)/src -name '*.c')
INC := -I. -I$(LVGL) -I$(LVGL)/.. -I../js/mquickjs -I../js/generated/host64
DEFS := -DLV_CONF_INCLUDE_SIMPLE -DLV_LVGL_H_INCLUDE_SIMPLE -D_GNU_SOURCE -DHW3_JS_HOST -DHW3_NO_REGEXP

sim: build/liblvgl.a $(FW) $(JS) lv_conf.h $(wildcard ../*/*.h)
	$(CC) $(CFLAGS) $(DEFS) $(INC) -o $@ $(FW) $(JS) build/liblvgl.a -lm

build/liblvgl.a: $(LV) lv_conf.h
	mkdir -p build/lv
	for f in $(LV); do o=build/lv/$$(echo $$f | tr '/.' '__').o; $(CC) $(CFLAGS) $(DEFS) $(INC) -c $$f -o $$o || exit 1; done
	ar rcs $@ build/lv/*.o

clean:
	rm -rf sim build
