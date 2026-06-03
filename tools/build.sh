#!/usr/bin/env bash
# 범용 빌드 — raylib 소스 직접 컴파일 (검증된 최소 구성).
# 사용법: bash tools/build.sh <src상대경로> [out이름]
#   예) bash tools/build.sh src/proto.c game.exe
# 2D 빈 창 기준 모듈(rmodels·raudio 제외). 오디오 추가 시 SUPPORT_MODULE_RAUDIO=1 + raudio.c 추가.
set -e
export PATH="/c/msys64/mingw64/bin:$PATH"

ROOT="/c/Users/Admin/Desktop/1.44/50-game"
RL="$ROOT/tools/raylib-src/src"
SRC="${1:-src/blank.c}"
OUT="$ROOT/build/${2:-game.exe}"
AUDIO="${3:-}"   # 세 번째 인자 "audio" 주면 raudio 포함

ENGINE="$RL/rcore.c $RL/rshapes.c $RL/rtextures.c $RL/rtext.c $RL/utils.c $RL/rglfw.c"
AUDIOFLAGS=""
EXTRALIB=""
if [ "$AUDIO" = "audio" ]; then
  ENGINE="$ENGINE $RL/raudio.c"
  AUDIOFLAGS="-DSUPPORT_MODULE_RAUDIO=1"
  EXTRALIB="-lole32"   # miniaudio(WASAPI) 의존
fi

gcc "$ROOT/$SRC" $ENGINE \
  -o "$OUT" \
  -I"$RL" -I"$RL/external/glfw/include" \
  -DPLATFORM_DESKTOP -DGRAPHICS_API_OPENGL_33 $AUDIOFLAGS \
  -static -static-libgcc \
  -Os -flto -ffunction-sections -fdata-sections -Wl,--gc-sections \
  -lopengl32 -lgdi32 -lwinmm $EXTRALIB \
  -mwindows

echo "BUILD_OK $(stat -c%s "$OUT") $OUT"
