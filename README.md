# GRAVITY FLIP — 1.44MB Game Jam Entry

A one-button **gravity-flip runner** built in C + [raylib](https://www.raylib.com/), targeting the **2PGARCADE "1.44MB Game Development Contest"**.

Press **SPACE / Click / ↑** to flip gravity (floor ↔ ceiling). Dodge the walls. Survive. That's the whole game — single mechanic, six obstacle patterns, a three-stage difficulty curve, and invisible onboarding in the first 30 seconds.

## Hard constraint

The contest hard-caps the **uncompressed** submission at **1,474,560 bytes (1.44MB)**. One byte over = instant disqualification.

Current build (single static `.exe`, audio included):

| Stage | Size |
|---|---|
| Compiled | 1,299,448 B |
| After `strip` | 532,796 B |
| After UPX | 193,340 B |
| **Submission (extracted)** | **193,340 B — 13.1% of cap** |

No external asset files: all SFX and chiptune BGM are synthesized procedurally in code (0-byte assets, original work / CC0).

## Build (Windows, MinGW-w64 / MSYS2)

raylib is compiled from source (prebuilt MSYS2 packages hit an `__imp_glfw` link failure). Clone raylib into `tools/raylib-src/`:

```sh
git clone --depth 1 https://github.com/raysan5/raylib tools/raylib-src
bash tools/build.sh src/game.c game.exe audio
```

The build script compiles the minimal raylib module set (rcore, rshapes, rtextures, rtext, utils, rglfw, raudio) statically. Then:

```sh
strip --strip-unneeded build/game.exe
upx --best build/game.exe
```

Verify the size gate (extracted-size basis, not the UPX self-compressed figure):

```sh
pwsh -File tools/check-size.ps1
```

## Source layout

- `src/game.c` — the game (288 lines). The submitted build.
- `src/blank.c` — empty-window baseline used to measure engine overhead.
- `src/proto*.c`, `src/game_sim.c` — earlier prototypes and a headless difficulty-curve simulator (not built into the submission).
- `tools/build.sh` — source build (`audio` arg adds the raudio module).
- `tools/check-size.ps1` — size-gate check over the build folder.

## License

Game code: MIT (see below). raylib is under its own zlib/libpng license. All audio is procedurally generated original work.

---

🤖 Built with [Claude Code](https://claude.com/claude-code)
