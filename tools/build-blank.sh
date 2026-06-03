#!/usr/bin/env bash
# 빈 창 베이스라인 빌드 — raylib 소스 직접 컴파일 (검증된 최소 구성).
# 20-wiki/raylib 빌드 함정과 최소 구성.md 의 구성: rglfw.c 포함 + PLATFORM_DESKTOP 필수.
# MSYS2 prebuilt libraylib.a는 GLFW를 DLL import(__imp_)로 빌드 → 정적 단일 exe 불가 → 소스 직접 빌드.
set -e
export PATH="/c/msys64/mingw64/bin:$PATH"

ROOT="/c/Users/Admin/Desktop/1.44/50-game"
RL="$ROOT/tools/raylib-src/src"
OUT="$ROOT/build/game.exe"

# 2D 빈 창: rmodels(3D)·raudio(오디오) 제외. 미사용 모듈 소스레벨 제거.
ENGINE="$RL/rcore.c $RL/rshapes.c $RL/rtextures.c $RL/rtext.c $RL/utils.c $RL/rglfw.c"

gcc "$ROOT/src/blank.c" $ENGINE \
  -o "$OUT" \
  -I"$RL" -I"$RL/external/glfw/include" \
  -DPLATFORM_DESKTOP -DGRAPHICS_API_OPENGL_33 \
  -DSUPPORT_MODULE_RMODELS=0 -DSUPPORT_MODULE_RAUDIO=0 \
  -static -static-libgcc \
  -Os -flto -ffunction-sections -fdata-sections -Wl,--gc-sections \
  -lopengl32 -lgdi32 -lwinmm \
  -mwindows

echo "BUILD_OK $(stat -c%s "$OUT")"
