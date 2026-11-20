# openWatchOS firmware parts

Parts of the future own firmware for the M78, built and tested on the PC. Nothing here is flashed to the watch yet.
The goal: look and function like the stock firmware, compatible with its watch faces, with own watch faces, LVGL
as the interface library and JS apps – but rounder and better overall.

![Preview of the interface](../docs/img/ui-preview.png)

*From the host simulator: every picture is the firmware's own code, driven by a scripted finger.*

## What is there

| Part | Folder | Status |
| --- | --- | --- |
| Watch face engine | `owf/` | Draws stock watch face files (`JL` format) in bands, without allocation. Same pixels as our reference renderer for all test faces, hands included. |
| Interface | `ui/` | LVGL 9.6. Watch face, apps, control centre, activity, notifications; swiping like the stock firmware; tap areas of the stock faces open the matching app. |
| App frame | `apps/ow_app.*` | Built-in and JS apps open on top, swipe right goes back; settings saved in `sys/settings.txt`. |
| Built-in apps | `apps/builtin/` | Pulse, weather, music remote, stopwatch with laps, timer, alarms, lamp, watch face picker with previews, app store, my apps, settings with "about". |
| JS apps | `js/`, `apps/builtin/js_app.c` | The same `.oapps` apps as on the patched stock firmware run unchanged: MicroQuickJS and our JS layer, drawing into an LVGL canvas. |
| Packages | `apps/ow_pkg.*`, `apps/ow_json.*` | Installs `.oapps` (tar) into `apps/<id>/` after checking every file name, `app.json` and the permissions; list, update, remove, pin to the main list. |
| App store | `apps/builtin/store.c` | Catalogue and packages come from the phone (the watch has no internet); detail page with the permissions in plain words. Protocol: [docs/app-store.md](../docs/app-store.md). |
| Platform layer | `sys/ow_sys.h` | Time, storage, vibration, brightness, battery, phone link. PC version: `sim/ow_sys_sim.c`. The watch version comes with the SDK build. |
| Fonts | `ui/fonts/` | Inter with umlauts plus Font Awesome symbols (licence: `FONTS-LICENSE.txt`). |
| Host simulator | `sim/` | Runs everything with the watch's display settings, a simulated phone and app store, and writes each step as a picture. |

Still missing for the watch: the platform layer on the SDK (display, touch, keys, storage, Bluetooth link to the
phone app), sensors (pulse, steps), real notifications and the always-on mode.

## Build and try

    git clone --depth 1 --branch v9.6.0 https://github.com/lvgl/lvgl.git ../lvgl
    make -C fw/sim LVGL=$PWD/../lvgl
    mkdir -p storage/faces && cp MY_FACES/*.di storage/faces/
    python3 fw/sim/make_store.py store APPS/*.oapps          # store/catalog.json + packages
    OW_ROOT=storage OW_STORE=store fw/sim/sim out/           # out/01_zifferblatt.ppm ... 26_js_aus_menue.ppm

Tests:

    cc -D_GNU_SOURCE -Ifw -o test_pkg fw/apps/test_pkg.c fw/apps/ow_pkg.c fw/apps/ow_json.c fw/sys/ow_time.c fw/sim/ow_sys_sim.c
    ./test_pkg SOME_APP.oapps       # JSON, versions, install/list/pin/remove, refused packages

    gcc -O2 -o owf_cli fw/owf/owf.c fw/owf/owf_cli.c
    ./owf_cli FACE.di face.ppm hour=23 minute=59 battery=5

## How the watch face reaches LVGL

The face is an LVGL image whose decoder is the engine: LVGL asks for the rows it is drawing, the decoder renders
bands of 40 full-width rows with `owf_render`. There is no screen-sized buffer, so a face costs one band of RAM,
and the stock faces work unchanged.

## Storage layout

| Path | Content |
| --- | --- |
| `faces/*.di` | watch faces (stock format) |
| `apps/<id>/` | installed packages: `app.json`, `watch.txt` (the script), `icon.png`, `i18n/`, `data.txt` (the app's `save()`) |
| `sys/settings.txt` | settings, `key=value` |
| `sys/alarms.txt` | alarms, `HH:MM on` per line |
| `sys/pinned.txt` | apps in the main list |
| `sys/store.json` | last store catalogue from the phone |

## Licences

Our code: GPL-2.0. MicroQuickJS (`js/mquickjs`) and `js/hw3_stdlib.c`: MIT (see the files). Fonts: SIL OFL 1.1.
LVGL is not part of the repository (MIT).
