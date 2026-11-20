# App store

The watch has no internet of its own. The phone app fetches the store catalogue and the packages and hands them
to the watch over Bluetooth; the watch checks every package itself before it installs anything.

## Messages

| Direction | Channel | Text | Meaning |
| --- | --- | --- | --- |
| watch → phone | `store` | `catalog` | send the current catalogue |
| watch → phone | `store` | `get <id>` | send this package |
| phone → watch | – | catalogue (JSON) | the firmware calls `ow_store_catalog()`; kept as `sys/store.json` |
| phone → watch | – | package (`.oapps` bytes) | the firmware calls `ow_store_package()` |
| phone → watch | – | open store page | `ow_store_show(id)` |

## Catalogue

```json
{"apps": [
  {"id": "de.jan.hallo", "name": "Hallo", "version": "1.0.1", "author": "Jan",
   "description": "Zählt Tipps und zeigt die letzte Position.", "size": 20480, "permissions": []}
]}
```

`fw/sim/make_store.py` builds it from a folder of `.oapps` packages.

## What the watch checks before installing

- The package is a valid tar archive (header checksums, sizes inside the file), at most 512 KB.
- Only plain files and folders, no absolute paths, no `..`, at most 24 files.
- `app.json` exists; `id` (lower case, dots), `name` and `version` are set; every permission is a known one.
- An app has its script (`watch.txt`).
- The package is the one that was asked for (same `id`).

Only then are the files written to `apps/<id>/`; an older version is replaced. The detail page lists the
permissions in plain words before installing.
