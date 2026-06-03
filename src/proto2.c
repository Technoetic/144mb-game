// 프로토타입 P2 — 원버튼 "중력 플립" 러너.
// 메인 컨셉 ③(원버튼 미니멀 아케이드). 첫 30초 손맛 + 즉시 재시작 중독성 목표.
// ⛔ 폴리시 없음(원시 도형만). abagames식 즉시 손맛 검증용.
//
// 조작: SPACE / 마우스클릭 / ↑ = 중력 반전 (바닥↔천장). 그 외 입력 없음.
// 플레이어는 화면 좌측 고정, 장애물이 우→좌로 흘러옴. 부딪히면 죽고 즉시 재시작.
// 점수 = 버틴 시간. 시간이 갈수록 가속. 로컬 베스트 표시.

#include "raylib.h"
#include <stdint.h>
#include <stdio.h>

#define SCRW 640
#define SCRH 480
#define FLOOR_Y 440
#define CEIL_Y  40
#define PLAYER_X 110
#define PSIZE 22
#define MAX_OBST 8

static uint64_t g_state;
static uint64_t sm64(void){uint64_t z=(g_state+=0x9e3779b97f4a7c15ULL);z=(z^(z>>30))*0xbf58476d1ce4e5b9ULL;z=(z^(z>>27))*0x94d049bb133111ebULL;return z^(z>>31);}
static int rnd(int n){return (int)(sm64()%(uint64_t)n);}

typedef struct { float x, y, w, h; int active; } Obst;

static float py;          // 플레이어 y (위치)
static float vy;          // 수직 속도
static int gravDir;       // +1=아래로, -1=위로
static int onSurface;     // 바닥/천장에 붙어있나
static Obst obs[MAX_OBST];
static float speed;       // 스크롤 속도(가속)
static float spawnTimer;
static float score;
static float best = 0;
static int dead;
static float deadTimer;
static float shake = 0;   // 화면 흔들림(죽을 때, 용량 0 juice)
static float flash = 0;   // 마일스톤 번쩍임

static void reset_game(void) {
    g_state = 0x5EED1234ULL + (uint64_t)(best*1000);  // 매판 약간 다른 시드
    py = FLOOR_Y - PSIZE;
    vy = 0; gravDir = 1; onSurface = 1;
    for (int i=0;i<MAX_OBST;i++) obs[i].active = 0;
    speed = 200.0f;
    spawnTimer = 0;
    score = 0;
    dead = 0; deadTimer = 0;
    shake = 0; flash = 0;
}

static Obst* free_obst(void) {
    for (int i=0;i<MAX_OBST;i++) if (!obs[i].active) return &obs[i];
    return 0;
}

static void spawn_obst(void) {
    float x = SCRW + 20;
    int pat = rnd(10);
    if (pat < 6) {
        // 단일 바닥/천장 기둥
        Obst* o = free_obst(); if(!o) return;
        o->active=1; o->x=x; o->w=24+rnd(20); o->h=40+rnd(90);
        if (rnd(2)==0) o->y = FLOOR_Y - o->h; else o->y = CEIL_Y;
    } else if (pat < 9) {
        // 천장+바닥 동시 (가운데 틈으로 통과) — 중력 타이밍 압박
        int gapY = 150 + rnd(120);      // 틈 시작 y
        int gapH = 110 + rnd(40);       // 틈 높이(통과 가능)
        Obst* a = free_obst(); if(a){ a->active=1; a->x=x; a->w=30; a->y=CEIL_Y; a->h=gapY-CEIL_Y; }
        Obst* b = free_obst(); if(b){ b->active=1; b->x=x; b->w=30; b->y=gapY+gapH; b->h=FLOOR_Y-(gapY+gapH); }
    } else {
        // 넓고 낮은 양쪽 — 한쪽 면에 붙어있게 강제
        Obst* o = free_obst(); if(!o) return;
        o->active=1; o->x=x; o->w=50+rnd(30); o->h=55;
        if (rnd(2)==0) o->y = FLOOR_Y - o->h; else o->y = CEIL_Y;
    }
}

static int aabb(float ax,float ay,float aw,float ah, float bx,float by,float bw,float bh){
    return ax < bx+bw && ax+aw > bx && ay < by+bh && ay+ah > by;
}

static void load_best(void){ FILE*f=fopen("best.dat","rb"); if(f){ if(fread(&best,sizeof(best),1,f)!=1) best=0; fclose(f);} }
static void save_best(void){ FILE*f=fopen("best.dat","wb"); if(f){ fwrite(&best,sizeof(best),1,f); fclose(f);} }

int main(void) {
    InitWindow(SCRW, SCRH, "PROTO2: One-Button Gravity Flip");
    SetTargetFPS(60);
    load_best();
    reset_game();

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        if (!dead) {
            // --- 입력: 한 버튼 = 중력 반전 ---
            if (IsKeyPressed(KEY_SPACE)||IsKeyPressed(KEY_UP)||IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                gravDir = -gravDir;
                onSurface = 0;
                vy = gravDir * 60.0f;   // 살짝 튕김(반응 체감)
            }
            // --- 물리: 중력 가속, 바닥/천장에 붙음 ---
            vy += gravDir * 1400.0f * dt;
            py += vy * dt;
            if (py >= FLOOR_Y - PSIZE) { py = FLOOR_Y - PSIZE; if(gravDir>0){vy=0;onSurface=1;} }
            if (py <= CEIL_Y)          { py = CEIL_Y;          if(gravDir<0){vy=0;onSurface=1;} }

            // --- 가속 + 점수 ---
            speed += 8.0f * dt;          // 점진 가속
            int prevMile = (int)(score/10);
            score += dt;
            if ((int)(score/10) > prevMile) flash = 0.4f;   // 10점마다 번쩍(마일스톤)
            spawnTimer -= dt;
            float interval = 1.1f - (speed-200)*0.0009f; if(interval<0.45f)interval=0.45f;
            if (spawnTimer <= 0) { spawn_obst(); spawnTimer = interval; }

            // --- 장애물 이동 + 충돌 ---
            for (int i=0;i<MAX_OBST;i++) {
                if (!obs[i].active) continue;
                obs[i].x -= speed * dt;
                if (obs[i].x + obs[i].w < 0) { obs[i].active = 0; continue; }
                if (aabb(PLAYER_X, py, PSIZE, PSIZE, obs[i].x, obs[i].y, obs[i].w, obs[i].h)) {
                    dead = 1; deadTimer = 0;
                    shake = 0.35f;                 // 죽을 때 화면 흔들림(juice, 용량 0)
                    if (score > best) { best = score; save_best(); }
                }
            }
        } else {
            // 죽음 → 즉시(0.2초) 재시작 가능
            deadTimer += dt;
            if (deadTimer > 0.2f &&
                (IsKeyPressed(KEY_SPACE)||IsKeyPressed(KEY_UP)||IsMouseButtonPressed(MOUSE_BUTTON_LEFT)))
                reset_game();
        }
        // juice 감쇠
        if (shake > 0) shake -= dt*1.2f; if (shake<0) shake=0;
        if (flash > 0) flash -= dt*1.5f; if (flash<0) flash=0;

        // --- 렌더 (원시 도형) ---
        // 화면 흔들림 오프셋(죽을 때). Camera2D로 전체 이동.
        int sx = 0, sy = 0;
        if (shake > 0) { sx = (rnd(11)-5) * (int)(shake*18); sy = (rnd(11)-5) * (int)(shake*18); }
        Camera2D cam = {0}; cam.target=(Vector2){-(float)sx,-(float)sy}; cam.zoom=1.0f;

        BeginDrawing();
        // 가속 체감: 배경이 속도 따라 미세하게 밝아짐 + 마일스톤 번쩍
        int bgv = 16 + (int)((speed-200)*0.02f); if(bgv>40)bgv=40;
        int fv = (int)(flash*60);
        ClearBackground((Color){(unsigned char)(16+fv),(unsigned char)(16+fv/2),(unsigned char)(24+fv),255});
        BeginMode2D(cam);
        // 바닥/천장 라인
        DrawRectangle(0, FLOOR_Y, SCRW, SCRH-FLOOR_Y, (Color){(unsigned char)(40+bgv),40,55,255});
        DrawRectangle(0, 0, SCRW, CEIL_Y, (Color){(unsigned char)(40+bgv),40,55,255});
        // 장애물
        for (int i=0;i<MAX_OBST;i++) if (obs[i].active)
            DrawRectangle((int)obs[i].x,(int)obs[i].y,(int)obs[i].w,(int)obs[i].h,(Color){220,80,90,255});
        // 플레이어 (중력 방향 표시: 위쪽이면 색 변화)
        Color pc = gravDir>0 ? (Color){90,200,255,255} : (Color){255,210,90,255};
        DrawRectangle(PLAYER_X, (int)py, PSIZE, PSIZE, pc);
        // 중력 방향 화살표(작은 삼각형 대용 사각형)
        if (gravDir>0) DrawRectangle(PLAYER_X+8, (int)py+PSIZE+2, 6, 6, pc);
        else           DrawRectangle(PLAYER_X+8, (int)py-8, 6, 6, pc);
        EndMode2D();

        // HUD (흔들림 영향 X)
        char buf[64];
        snprintf(buf,sizeof(buf),"SCORE %.1f", score);
        DrawText(buf, 10, 6, 22, RAYWHITE);
        snprintf(buf,sizeof(buf),"BEST %.1f", best);
        DrawText(buf, SCRW-150, 8, 18, (Color){180,180,200,255});
        if (dead) {
            DrawText("CRASH!", SCRW/2-70, SCRH/2-40, 40, (Color){255,90,100,255});
            DrawText("SPACE / CLICK to retry", SCRW/2-130, SCRH/2+10, 22, (Color){200,200,210,255});
        } else if (score < 2.0f) {
            DrawText("SPACE / CLICK = flip gravity", SCRW/2-160, SCRH-70, 20, (Color){150,150,170,255});
        }
        EndDrawing();
    }
    CloseWindow();
    return 0;
}
