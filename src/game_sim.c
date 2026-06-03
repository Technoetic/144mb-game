// game.c 난이도 곡선 시뮬레이터 — 헤드리스(raylib 불필요).
// "완벽 회피 AI"가 game.c와 동일 물리/곡선/스폰으로 몇 초 버티는지, 난이도가 적절한지 측정.
// 핵심 질문: 천장↔바닥 이동시간 vs 장애물 회피 가능성. game.c 상수와 동기화 유지 필수.
#include <stdint.h>
#include <stdio.h>
#include <math.h>

#define SCRW 640
#define FLOOR_Y 440
#define CEIL_Y 40
#define PLAYER_X 110
#define PSIZE 22
#define MAX_OBST 16
#define PLAYZONE (FLOOR_Y-CEIL_Y)

static uint64_t g_state;
static uint64_t sm64(void){uint64_t z=(g_state+=0x9e3779b97f4a7c15ULL);z=(z^(z>>30))*0xbf58476d1ce4e5b9ULL;z=(z^(z>>27))*0x94d049bb133111ebULL;return z^(z>>31);}
static int rnd(int n){return (int)(sm64()%(uint64_t)n);}
static float frnd(void){return (float)(sm64()%10000)/10000.0f;}

typedef struct{float x,y,w,h,vy;int moving,active;}Obst;
static Obst obs[MAX_OBST];

static float curve_speed(float t){
    if(t<14.0f)return 175.0f+t*3.5f;
    else if(t<45.0f)return 224.0f+(t-14.0f)*4.0f;
    else return 348.0f+(t-45.0f)*2.0f;
}
static float curve_interval(float t){float b=1.35f-t*0.010f;if(b<0.6f)b=0.6f;return b;}
static Obst* slot(void){for(int i=0;i<MAX_OBST;i++)if(!obs[i].active)return &obs[i];return 0;}

static void spawn(float t){
    float x=SCRW+24; int maxpat=(t<10)?1:(t<24)?3:(t<42)?5:6; int pat=rnd(maxpat);
    switch(pat){
    case 0:{Obst*o=slot();if(!o)return;o->active=1;o->moving=0;o->vy=0;o->x=x;o->w=26+rnd(16);o->h=50+rnd(70);if(rnd(2))o->y=FLOOR_Y-o->h;else o->y=CEIL_Y;}break;
    case 1:{int gh=130-(int)(t*0.6f);if(gh<95)gh=95;int gy=CEIL_Y+30+rnd(PLAYZONE-gh-60);
        Obst*a=slot();if(a){a->active=1;a->moving=0;a->vy=0;a->x=x;a->w=32;a->y=CEIL_Y;a->h=gy-CEIL_Y;}
        Obst*b=slot();if(b){b->active=1;b->moving=0;b->vy=0;b->x=x;b->w=32;b->y=gy+gh;b->h=FLOOR_Y-(gy+gh);}}break;
    case 2:{Obst*o=slot();if(!o)return;o->active=1;o->moving=0;o->vy=0;o->x=x;o->w=55+rnd(35);o->h=55;if(rnd(2))o->y=FLOOR_Y-o->h;else o->y=CEIL_Y;}break;
    case 3:{Obst*a=slot();if(a){a->active=1;a->moving=0;a->vy=0;a->x=x;a->w=28;a->h=65;a->y=FLOOR_Y-65;}
        Obst*b=slot();if(b){b->active=1;b->moving=0;b->vy=0;b->x=x+185;b->w=28;b->h=65;b->y=CEIL_Y;}}break;
    case 4:{Obst*o=slot();if(!o)return;o->active=1;o->moving=1;o->x=x;o->w=28;o->h=44;o->y=CEIL_Y+40+rnd(PLAYZONE-120);o->vy=(rnd(2)?1:-1)*(60.0f+frnd()*40.0f);}break;
    case 5:{int gy1=CEIL_Y+40;int gy2=FLOOR_Y-100;
        Obst*a=slot();if(a){a->active=1;a->moving=0;a->vy=0;a->x=x;a->w=28;a->y=gy1;a->h=(gy2-50)-gy1;}
        Obst*b=slot();if(b){b->active=1;b->moving=0;b->vy=0;b->x=x;b->w=28;b->y=gy2;b->h=FLOOR_Y-gy2;}}break;
    }
}
static int aabb(float ax,float ay,float aw,float ah,float bx,float by,float bw,float bh){return ax<bx+bw&&ax+aw>bx&&ay<by+bh&&ay+ah>by;}

// "완벽 회피 AI": 플레이어 x열에 다가오는 장애물을 보고, 안전한 면(위/아래)으로 미리 이동.
// game.c와 동일 물리. 단순 휴리스틱: 다음 충돌 위험 장애물의 빈 공간으로 향함.
int main(void){
    int FPS=60; float dt=1.0f/FPS;
    int runs=200; float total=0, mn=1e9, mx=0; int died_early=0; // 5초 미만 사망 = 너무 어려움
    for(int r=0;r<runs;r++){
        g_state=0x5EEDULL+r*2654435761u+1;
        for(int i=0;i<MAX_OBST;i++)obs[i].active=0;
        float py=FLOOR_Y-PSIZE, vy=0; int grav=1;
        float speed=curve_speed(0), spawnTimer=1.4f, t=0;
        int alive=1;
        while(alive && t<120.0f){
            // --- AI 결정: 0.08초마다 위협 평가 후 목표 면 선택 ---
            // 가장 가까운(앞쪽) 장애물 찾아 빈 공간 중심으로
            float targetY=-1; float nearest=1e9;
            for(int i=0;i<MAX_OBST;i++){
                if(!obs[i].active)continue;
                float dx=obs[i].x-PLAYER_X;
                if(dx< -40 || dx>260)continue;         // 시야 범위
                if(dx<nearest){ nearest=dx;
                    // 이 장애물이 위쪽이면 아래로, 아래쪽이면 위로
                    if(obs[i].y<=CEIL_Y+2) targetY=FLOOR_Y-PSIZE;      // 천장형→바닥
                    else if(obs[i].y+obs[i].h>=FLOOR_Y-2) targetY=CEIL_Y; // 바닥형→천장
                    else { // 틈: 틈 중심으로(가까운 면)
                        float gapc=obs[i].y; // 단순화
                        targetY = (gapc<PLAYZONE/2+CEIL_Y)?FLOOR_Y-PSIZE:CEIL_Y;
                    }
                }
            }
            // 목표 면으로 가도록 중력 결정
            if(targetY>=0){
                int wantGrav = (targetY>py)?1:-1;
                if(wantGrav!=grav){ grav=-grav; vy=grav*60.0f; }
            }
            // --- 물리 (game.c 동일) ---
            vy += grav*2200.0f*dt; py += vy*dt;
            if(py>=FLOOR_Y-PSIZE){py=FLOOR_Y-PSIZE;if(grav>0)vy=0;}
            if(py<=CEIL_Y){py=CEIL_Y;if(grav<0)vy=0;}
            t+=dt; speed=curve_speed(t); spawnTimer-=dt;
            if(spawnTimer<=0){spawn(t);spawnTimer=curve_interval(t);}
            for(int i=0;i<MAX_OBST;i++){
                if(!obs[i].active)continue;
                obs[i].x-=speed*dt;
                if(obs[i].moving){obs[i].y+=obs[i].vy*dt;if(obs[i].y<CEIL_Y){obs[i].y=CEIL_Y;obs[i].vy=-obs[i].vy;}if(obs[i].y+obs[i].h>FLOOR_Y){obs[i].y=FLOOR_Y-obs[i].h;obs[i].vy=-obs[i].vy;}}
                if(obs[i].x+obs[i].w<0){obs[i].active=0;continue;}
                if(aabb(PLAYER_X,py,PSIZE,PSIZE,obs[i].x,obs[i].y,obs[i].w,obs[i].h)){alive=0;break;}
            }
        }
        total+=t; if(t<mn)mn=t; if(t>mx)mx=t; if(t<5.0f)died_early++;
    }
    printf("=== game.c 난이도 시뮬 (완벽회피 AI, %d런) ===\n", runs);
    printf("평균 생존: %.1fs / 최소 %.1fs / 최대 %.1fs\n", total/runs, mn, mx);
    printf("5초 미만 사망(너무어려움): %d/%d (%.0f%%)\n", died_early, runs, 100.0*died_early/runs);
    printf("속도곡선: 0s=%.0f 12s=%.0f 30s=%.0f 60s=%.0f\n", curve_speed(0),curve_speed(12),curve_speed(30),curve_speed(60));
    // 물리 점검: 천장↔바닥 이동 시간
    float dist=PLAYZONE-PSIZE, g=2200.0f; float tflip=sqrtf(2*dist/g);
    printf("천장↔바닥 이동시간: %.2fs (장애물 폭 통과시간 @speed300: %.2fs)\n", tflip, 40.0f/300.0f);
    return 0;
}
