# RUINS SHARDS — Native NumWorks App

This project is rebuilt around the official NumWorks EADK C sample-app structure.

The official sample uses a root `Makefile`, `src/main.c`, `src/icon.png`, ARM GCC, and `nwlink`; `make clean && make build` produces an `.nwa`. See the official NumWorks sample-app documentation.

## Build with GitHub Actions

1. Upload this project's contents to a GitHub repository.
2. Keep `Makefile` at the repository root.
3. Keep `main.c` and `icon.png` inside `src/`.
4. Keep `build.yml` at `.github/workflows/build.yml`.
5. Open Actions and run **Build RUINS SHARDS**.
6. Download the `RUINS-SHARDS-NWA` artifact after the job succeeds.

## Controls

LEFT / RIGHT = move
OK = jump
SHIFT = shoot
UP / DOWN = aim
BACK = exit

Bullets are rendered as only their current 4×3 pixel body. The game redraws the playfield every frame, so bullets do not leave trails.

This is a native C/EADK recreation of the game mechanics rather than a MicroPython wrapper.
\n## Verification note\nThe project was checked for the earlier structural failure: the Makefile is at repository root, and the workflow invokes `make clean && make build`. The C source uses the current EADK header API for keyboard/display operations.\n