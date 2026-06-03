// 프로토타입 P1 — 한 화면 절차적 로그라이트 + 시그니처 메커니즘:
//   "죽음이 다음 런의 맵이 된다" (Death Becomes the Map).
// 전략 §3-①. 와 모먼트 검증 전용 — ⛔ 폴리시(아트/juice/음악) 일절 없음. 코어만.
// 지식: [[절차생성 도달가능성 보장]](constructive + flood-fill) + [[결정론적 PRNG 선택]](SplitMix64).
//
// 코어 루프: 시드로 한 화면 그리드 생성 → 플레이어가 출구(E)까지 이동 → 적(추적) 회피.
//   죽으면 그 칸이 영구 '뼈벽'으로 각인되어 다음 런의 맵에 누적된다.
//   도달가능성은 flood-fill로 보장(출구 못 가면 reroll).

#include "raylib.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>   // abs
#include <stdio.h>    // snprintf

#define GW 20          // 그리드 폭
#define GH 15          // 그리드 높이
#define CELL 32        // 셀 픽셀
#define SCRW (GW*CELL)
#define SCRH (GH*CELL)
#define MAXEN 6

// ---- 결정론적 PRNG: SplitMix64 (지식노트 검증된 9연산) ----
static uint64_t g_state;
static uint64_t splitmix64(void) {
    uint64_t z = (g_state += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}
static int rnd(int n) { return (int)(splitmix64() % (uint64_t)n); }

// ---- 맵 ----
// tile: 0=빈칸, 1=벽, 2=뼈벽(죽음 각인, 영구)
static int tile[GH][GW];
static int bone[GH][GW];   // 런 간 누적되는 죽음의 흔적
static int px, py;         // 플레이어
static int ex, ey;         // 출구
static int enx[MAXEN], eny[MAXEN], encount;
static int run = 1;
static int alive = 1;
static int won = 0;
static int turn = 0;       // 적 속도 조절용
static int hp = 3;         // 체력(즉사 방지)
static int invuln = 0;     // 피격 후 무적 턴(연속 피격 방지)
#define HP_MAX 3

// flood-fill로 (sx,sy)에서 (tx,ty) 도달 가능?
static int reachable(int sx, int sy, int tx, int ty) {
    static int seen[GH][GW];
    memset(seen, 0, sizeof(seen));
    int qx[GW*GH], qy[GW*GH], h=0, t=0;
    qx[t]=sx; qy[t]=sy; t++; seen[sy][sx]=1;
    while (h < t) {
        int cx=qx[h], cy=qy[h]; h++;
        if (cx==tx && cy==ty) return 1;
        int dx[4]={1,-1,0,0}, dy[4]={0,0,1,-1};
        for (int i=0;i<4;i++) {
            int nx=cx+dx[i], ny=cy+dy[i];
            if (nx<0||ny<0||nx>=GW||ny>=GH) continue;
            if (seen[ny][nx]) continue;
            if (tile[ny][nx]==1 || tile[ny][nx]==2) continue;
            seen[ny][nx]=1; qx[t]=nx; qy[t]=ny; t++;
        }
    }
    return 0;
}

static void gen_map(uint64_t seed) {
    g_state = seed;
    for (;;) {
        // 1) 테두리 벽 + 내부 랜덤 벽 30% (constructive 후 flood-fill 검증)
        for (int y=0;y<GH;y++) for (int x=0;x<GW;x++) {
            if (x==0||y==0||x==GW-1||y==GH-1) tile[y][x]=1;
            else tile[y][x] = (rnd(100) < 20) ? 1 : 0;   // 벽 20%(통로 확보)
            // 죽음 각인(뼈벽) 누적
            if (bone[y][x]) tile[y][x] = 2;
        }
        // 2) 시작/출구 배치 (대각 코너 근처, 빈칸 강제)
        px=1; py=1;  ex=GW-2; ey=GH-2;
        tile[py][px]=0; tile[ey][ex]=0;
        if (bone[py][px]||bone[ey][ex]) { /* 코너가 막히면 재생성 */ continue; }
        // 3) 도달가능성 보장 — 못 가면 reroll (한 화면이라 flood-fill 거의 공짜)
        if (!reachable(px,py,ex,ey)) continue;
        // 4) 적 배치 (빈칸, 플레이어와 떨어진 곳)
        encount = 1 + (run/3 < 2 ? run/3 : 2);  // 시작 1마리, 런마다 천천히 증가, 최대 3
        if (encount>MAXEN) encount=MAXEN;
        int placed=0, guard=0;
        while (placed<encount && guard<500) {
            guard++;
            int rx=1+rnd(GW-2), ry=1+rnd(GH-2);
            if (tile[ry][rx]!=0) continue;
            if (abs(rx-px)+abs(ry-py) < 9) continue;  // 시작에서 9칸+ (여유)
            enx[placed]=rx; eny[placed]=ry; placed++;
        }
        encount=placed;
        return;
    }
}

static void enemy_step(void) {
    // 적: 50%만 추적, 50%는 랜덤 배회 → 무자비함 완화(즉사 스트레스 ↓)
    for (int i=0;i<encount;i++) {
        int bx=enx[i], by=eny[i];
        int dx, dy;
        if (rnd(100) < 50) {                 // 추적
            dx = (px>bx)-(px<bx);
            dy = (py>by)-(py<by);
        } else {                              // 배회
            int r = rnd(4);
            int rdx[4]={1,-1,0,0}, rdy[4]={0,0,1,-1};
            dx=rdx[r]; dy=rdy[r];
        }
        // 우선 x, 막히면 y
        int nx=bx+dx, ny=by;
        if (dx!=0 && tile[ny][nx]==0) { enx[i]=nx; eny[i]=ny; }
        else { nx=bx; ny=by+dy; if (dy!=0 && tile[ny][nx]==0) { enx[i]=nx; eny[i]=ny; } }
    }
}

static int enemy_hit(void) {
    for (int i=0;i<encount;i++) if (enx[i]==px && eny[i]==py) return 1;
    return 0;
}

static void die_imprint(void) {
    bone[py][px] = 1;            // ★ 시그니처: 죽은 자리가 다음 런의 뼈벽
    alive = 0;
}

int main(void) {
    InitWindow(SCRW, SCRH, "PROTO: Death Becomes the Map");
    SetTargetFPS(60);
    memset(bone, 0, sizeof(bone));
    gen_map(0xC0FFEEULL + run);

    float moveCooldown = 0;          // 누르고 있을 때 연속 이동 간격(초)

    while (!WindowShouldClose()) {
        // --- 입력/업데이트 (격자 턴제: 이동 시 적도 절반 속도) ---
        if (alive && !won) {
            if (moveCooldown > 0) moveCooldown -= GetFrameTime();
            int mvx=0, mvy=0;
            // 막 눌렀으면 즉시 반응, 누르고 있으면 쿨다운 간격으로 연속 이동
            int fresh = IsKeyPressed(KEY_RIGHT)||IsKeyPressed(KEY_LEFT)||IsKeyPressed(KEY_UP)||IsKeyPressed(KEY_DOWN)
                      ||IsKeyPressed(KEY_D)||IsKeyPressed(KEY_A)||IsKeyPressed(KEY_W)||IsKeyPressed(KEY_S);
            if (fresh || moveCooldown <= 0) {
                if (IsKeyDown(KEY_RIGHT)||IsKeyDown(KEY_D)) mvx=1;
                else if (IsKeyDown(KEY_LEFT)||IsKeyDown(KEY_A)) mvx=-1;
                else if (IsKeyDown(KEY_DOWN)||IsKeyDown(KEY_S)) mvy=1;
                else if (IsKeyDown(KEY_UP)||IsKeyDown(KEY_W)) mvy=-1;
                if (mvx||mvy) moveCooldown = 0.12f;   // 연속 이동 간격
            }
            if (mvx||mvy) {
                int nx=px+mvx, ny=py+mvy;
                if (tile[ny][nx]==0) { px=nx; py=ny; }
                turn++;
                if (invuln > 0) invuln--;
                if (turn % 3 == 0) enemy_step();   // 적은 3턴당 1회 → 플레이어가 3배 빠름
                if (enemy_hit() && invuln==0) {     // 피격 = 체력 1 깎임 + 잠깐 무적
                    hp--; invuln = 4;
                    if (hp <= 0) die_imprint();     // 체력 0일 때만 죽음→뼈벽
                }
                if (px==ex && py==ey) won=1;
            }
        }
        // --- 리셋/다음 런 ---
        if ((!alive || won) && IsKeyPressed(KEY_SPACE)) {
            run++;
            alive=1; won=0; turn=0; hp=HP_MAX; invuln=0;
            gen_map(0xC0FFEEULL + run);
        }

        // --- 렌더 (원시 도형만, 폴리시 없음) ---
        BeginDrawing();
        ClearBackground((Color){18,18,22,255});
        for (int y=0;y<GH;y++) for (int x=0;x<GW;x++) {
            Color c;
            if (tile[y][x]==1) c=(Color){55,55,70,255};        // 벽
            else if (tile[y][x]==2) c=(Color){150,40,40,255};   // 뼈벽(죽음 각인)
            else c=(Color){30,30,38,255};                       // 바닥
            DrawRectangle(x*CELL, y*CELL, CELL-1, CELL-1, c);
        }
        DrawRectangle(ex*CELL, ey*CELL, CELL-1, CELL-1, (Color){60,200,90,255}); // 출구
        for (int i=0;i<encount;i++)
            DrawRectangle(enx[i]*CELL+4, eny[i]*CELL+4, CELL-9, CELL-9, (Color){220,170,40,255}); // 적
        // 플레이어 — 무적 중엔 깜빡임(피격 피드백)
        if (invuln==0 || (turn/1)%2==0)
            DrawRectangle(px*CELL+3, py*CELL+3, CELL-7, CELL-7, RAYWHITE);

        // HUD: RUN + 체력 하트
        char hud[64];
        snprintf(hud, sizeof(hud), "RUN %d", run);
        DrawText(hud, 6, 4, 20, (Color){200,200,210,255});
        for (int i=0;i<HP_MAX;i++)
            DrawRectangle(SCRW-26-i*22, 8, 16, 16, i<hp ? (Color){230,70,70,255} : (Color){70,50,50,255});
        if (won)   DrawText("ESCAPED!  [SPACE] next", SCRW/2-130, SCRH/2-10, 24, (Color){60,220,90,255});
        if (!alive) DrawText("DIED -> here becomes wall  [SPACE]", SCRW/2-200, SCRH/2-10, 22, (Color){230,80,80,255});
        EndDrawing();
    }
    CloseWindow();
    return 0;
}
