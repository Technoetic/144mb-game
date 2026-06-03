// 1.44MB 게임 본체 — 원버튼 "중력 플립" 러너 (P2 확정 메커니즘 심화).
// 메인 컨셉 ③. 단일 메커니즘 깊이 + 첫 30초 invisible 온보딩 + 닫힌 루프.
// ⛔ 아직 사운드/아트 폴리시 없음(코어 깊이 검증 단계). juice는 용량 0짜리만.
//
// 조작: SPACE / 클릭 / ↑ = 중력 반전 (바닥↔천장). 그 외 입력 없음.

#include "raylib.h"
#include <stdint.h>
#include <stdio.h>
#include <math.h>

#define SCRW 640
#define SCRH 480
#define FLOOR_Y 440
#define CEIL_Y  40
#define PLAYER_X 110
#define PSIZE 22
#define MAX_OBST 16
#define PLAYZONE (FLOOR_Y - CEIL_Y)   // 400

// ---- PRNG ----
static uint64_t g_state;
static uint64_t sm64(void){uint64_t z=(g_state+=0x9e3779b97f4a7c15ULL);z=(z^(z>>30))*0xbf58476d1ce4e5b9ULL;z=(z^(z>>27))*0x94d049bb133111ebULL;return z^(z>>31);}
static int rnd(int n){return (int)(sm64()%(uint64_t)n);}
static float frnd(void){return (float)(sm64()%10000)/10000.0f;}

// ---- 게임 상태 ----
typedef enum { ST_TITLE, ST_PLAY, ST_DEAD } GameState;
typedef struct { float x,y,w,h; float vy; int moving; int active; int passed; } Obst;

static GameState st;
static float py, vy;
static int gravDir, onSurface;
static Obst obs[MAX_OBST];
static float speed, spawnTimer, score, best=0;
static float shake, flash, deadTimer, playTime;
static int flips;             // 이번 판 중력 반전 횟수(온보딩 판정용)
static int muted=0;           // M키 음소거
// 플레이어 트레일(잔상) — 최근 위치 링버퍼
#define TRAIL 8
static float trailX[TRAIL], trailY[TRAIL]; static int trailHead=0;
// 통과 팝업("+1" 떠오름)
#define POPS 6
static struct { float x,y,life; } pops[POPS]; static int popHead=0;

// ---- 난이도 곡선: playTime(초) → 속도 ----
// 3구간: 0~14s 완만(온보딩) / 14~45s 가속 / 45s+ 고속. (난이도 하향 조정: 시작 더 느리게·가속 완만하게)
static float curve_speed(float t){
    if (t < 14.0f)       return 175.0f + t*3.5f;            // 175→224 (더 천천히, 온보딩 연장)
    else if (t < 45.0f)  return 224.0f + (t-14.0f)*4.0f;    // 224→348 (가속 완만: 5→4)
    else                 return 348.0f + (t-45.0f)*2.0f;    // 348→ 더 천천히 계속
}
static float curve_interval(float t){
    float base = 1.35f - t*0.010f;       // 시작 더 넉넉(1.25→1.35), 촘촘해지는 속도 완화
    if (base < 0.6f) base = 0.6f;        // 최소 간격 상향(0.5→0.6): 후반에도 숨 쉴 틈
    return base;
}

static void reset_play(void){
    g_state = 0x5EEDULL + (uint64_t)(score*7) + (uint64_t)(best*13) + 1;
    py = FLOOR_Y - PSIZE; vy=0; gravDir=1; onSurface=1;
    for(int i=0;i<MAX_OBST;i++) obs[i].active=0;
    speed=curve_speed(0); spawnTimer=1.4f;   // 첫 장애물 늦게(온보딩: 먼저 점프 익히게)
    score=0; playTime=0; flips=0;
    shake=0; flash=0; deadTimer=0;
    for(int k=0;k<TRAIL;k++){trailX[k]=PLAYER_X;trailY[k]=FLOOR_Y-PSIZE;} for(int k=0;k<POPS;k++)pops[k].life=0;
}

// 슬롯 확보 시 passed를 0으로 초기화 — 슬롯 재사용 버그 차단(이전 장애물의 passed=1 잔존 → +1 팝업 누락).
static Obst* slot(void){ for(int i=0;i<MAX_OBST;i++) if(!obs[i].active){ obs[i].passed=0; return &obs[i]; } return 0; }

// 장애물 패턴 — 단일 메커니즘(중력) 안에서 깊이.
static void spawn(float t){
    float x = SCRW + 24;
    // 난이도 따라 패턴 풀 확장(점진 도입 = invisible 학습). 난이도 하향: 어려운 패턴 도입 시점 연장.
    int maxpat = (t<10)?1 : (t<24)?3 : (t<42)?5 : 6;
    int pat = rnd(maxpat);
    switch(pat){
    case 0: { // 단일 기둥 (가장 쉬움 — 온보딩 첫 패턴)
        Obst*o=slot(); if(!o)return; o->active=1;o->moving=0;o->vy=0;
        o->x=x; o->w=26+rnd(16); o->h=50+rnd(70);
        if(rnd(2)) o->y=FLOOR_Y-o->h; else o->y=CEIL_Y;
    } break;
    case 1: { // 천장+바닥 틈 통과 (중력 타이밍)
        int gh = 130 - (int)(t*0.6f); if(gh<95)gh=95;     // 틈 점점 좁아짐
        int gy = CEIL_Y + 30 + rnd(PLAYZONE - gh - 60);
        Obst*a=slot(); if(a){a->active=1;a->moving=0;a->vy=0;a->x=x;a->w=32;a->y=CEIL_Y;a->h=gy-CEIL_Y;}
        Obst*b=slot(); if(b){b->active=1;b->moving=0;b->vy=0;b->x=x;b->w=32;b->y=gy+gh;b->h=FLOOR_Y-(gy+gh);}
    } break;
    case 2: { // 넓고 낮은 — 한쪽 면 강제
        Obst*o=slot(); if(!o)return; o->active=1;o->moving=0;o->vy=0;
        o->x=x; o->w=55+rnd(35); o->h=55;
        if(rnd(2)) o->y=FLOOR_Y-o->h; else o->y=CEIL_Y;
    } break;
    case 3: { // 지그재그 2연속 (바닥-천장 리듬 강제) — 간격 180으로 중력 이동시간 확보
        Obst*a=slot(); if(a){a->active=1;a->moving=0;a->vy=0;a->x=x;a->w=28;a->h=65;a->y=FLOOR_Y-65;}
        Obst*b=slot(); if(b){b->active=1;b->moving=0;b->vy=0;b->x=x+185;b->w=28;b->h=65;b->y=CEIL_Y;}
    } break;
    case 4: { // 움직이는 장애물 (위아래 왕복) — 단일 메커니즘 깊이
        Obst*o=slot(); if(!o)return; o->active=1;o->moving=1;
        o->x=x; o->w=28; o->h=44;
        o->y=CEIL_Y + 40 + rnd(PLAYZONE-120);
        o->vy = (rnd(2)?1:-1) * (60.0f + frnd()*40.0f);
    } break;
    case 5: { // 좁은 더블 틈 (어려움)
        int gh=100; int gy1=CEIL_Y+40; int gy2=FLOOR_Y-100;
        Obst*a=slot(); if(a){a->active=1;a->moving=0;a->vy=0;a->x=x;a->w=28;a->y=gy1;a->h=(gy2-gh/2)-gy1;}
        Obst*b=slot(); if(b){b->active=1;b->moving=0;b->vy=0;b->x=x;b->w=28;b->y=gy2;b->h=FLOOR_Y-gy2;}
    } break;
    }
}

static int aabb(float ax,float ay,float aw,float ah,float bx,float by,float bw,float bh){
    return ax<bx+bw && ax+aw>bx && ay<by+bh && ay+ah>by;
}

// ---- 절차 효과음 + BGM (0바이트 에셋, 자작=CC0) ----
// 짧은 파형을 코드로 합성. [[트래커 오디오 라이브러리 선택]] "AudioStream 0바이트 합성".
#define SR 22050
static Sound sfxFlip, sfxCrash, sfxScore;

// ---- 절차 BGM (AudioStream 실시간 합성, 칩튠 루프) ----
static AudioStream bgm;
#define BGM_BUF 1024
// 간단한 베이스라인 + 아르페지오 멜로디(반음계 인덱스). 0=쉼표.
// C마이너 펜타토닉 느낌: C Eb F G Bb (semitone 0,3,5,7,10)
static const int MELODY[] = { 12,15,17,19, 17,15,12,10, 12,15,19,22, 19,17,15,12 };
static const int BASS[]   = { 0,0,3,3, 5,5,3,3, 0,0,7,7, 5,5,3,3 };
#define MEL_LEN (int)(sizeof(MELODY)/sizeof(MELODY[0]))
static float bgmPhaseM=0, bgmPhaseB=0;
static int bgmStep=0; static float bgmStepT=0;
static int bgmOn=1;

static float semitone_freq(int s){ return 110.0f * powf(2.0f, s/12.0f); }  // A2 기준

static void bgm_fill(void){
    if(!IsAudioStreamProcessed(bgm)) return;
    static short buf[BGM_BUF];
    float stepDur = 0.16f;   // 한 음 길이(초)
    for(int i=0;i<BGM_BUF;i++){
        bgmStepT += 1.0f/SR;
        if(bgmStepT >= stepDur){ bgmStepT-=stepDur; bgmStep=(bgmStep+1)%MEL_LEN; }
        int mi = MELODY[bgmStep], bi = BASS[bgmStep];
        float mf = semitone_freq(mi+12), bf = semitone_freq(bi);
        float env = 1.0f - (bgmStepT/stepDur)*0.6f;   // 음마다 살짝 감쇠(타격감)
        // 멜로디=사각파, 베이스=사인
        bgmPhaseM += mf/SR; if(bgmPhaseM>1)bgmPhaseM-=1;
        bgmPhaseB += bf/SR; if(bgmPhaseB>1)bgmPhaseB-=1;
        float mel = (bgmPhaseM<0.5f?1.0f:-1.0f) * 0.10f * env;
        float bass = sinf(bgmPhaseB*6.2831853f) * 0.14f;
        float s = (bgmOn && !muted) ? (mel+bass) : 0.0f;
        int v=(int)(s*32767); if(v>32767)v=32767; if(v<-32768)v=-32768;
        buf[i]=(short)v;
    }
    UpdateAudioStream(bgm, buf, BGM_BUF);
}

// kind: 0=플립(상승 사각파), 1=크래시(노이즈 하강), 2=점수(짧은 띵)
static Sound make_sfx(int kind){
    int n; short *buf;
    if(kind==0){ n=SR/12; }          // ~0.08s
    else if(kind==1){ n=SR/4; }      // ~0.25s
    else { n=SR/10; }                // ~0.1s
    buf = (short*)MemAlloc(n*sizeof(short));
    uint64_t ns=0x1234567 + kind*99;
    for(int i=0;i<n;i++){
        float t=(float)i/SR; float prog=(float)i/n; float s=0;
        if(kind==0){ // 플립: 주파수 상승 사각파 "쀼"
            float freq = 320 + prog*480;
            s = (sinf(t*freq*6.2831853f) > 0 ? 1.0f : -1.0f) * 0.25f;
        } else if(kind==1){ // 크래시: 노이즈 + 저주파 하강
            ns = ns*6364136223846793005ULL + 1; float noise=((ns>>40)&1023)/512.0f-1.0f;
            float freq = 220*(1.0f-prog*0.7f);
            s = (noise*0.5f + sinf(t*freq*6.2831853f)*0.4f) * 0.35f;
        } else { // 점수: 짧고 밝은 띵 (사인 2화음)
            float f=660;
            s = (sinf(t*f*6.2831853f)*0.5f + sinf(t*f*1.5f*6.2831853f)*0.3f) * 0.22f;
        }
        float env = (prog<0.1f)?prog/0.1f:(1.0f-prog);  // 어택+릴리즈 엔벨로프
        s *= env;
        int v=(int)(s*32767); if(v>32767)v=32767; if(v<-32768)v=-32768;
        buf[i]=(short)v;
    }
    Wave w = { (unsigned int)n, SR, 16, 1, buf };
    Sound snd = LoadSoundFromWave(w);
    MemFree(buf);
    return snd;
}

static void load_best(void){ FILE*f=fopen("best.dat","rb"); if(f){ if(fread(&best,sizeof(best),1,f)!=1)best=0; fclose(f);} }
static void save_best(void){ FILE*f=fopen("best.dat","wb"); if(f){ fwrite(&best,sizeof(best),1,f); fclose(f);} }

static int pressed(void){
    return IsKeyPressed(KEY_SPACE)||IsKeyPressed(KEY_UP)||IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

int main(void){
    InitWindow(SCRW, SCRH, "GRAVITY FLIP");
    InitAudioDevice();
    SetTargetFPS(60);
    sfxFlip = make_sfx(0); sfxCrash = make_sfx(1); sfxScore = make_sfx(2);
    SetAudioStreamBufferSizeDefault(BGM_BUF);
    bgm = LoadAudioStream(SR, 16, 1);
    PlayAudioStream(bgm);
    load_best();
    st = ST_TITLE;
    reset_play();

    while(!WindowShouldClose()){
        float dt = GetFrameTime(); if(dt>0.05f)dt=0.05f;
        bgm_fill();                         // BGM 스트림 실시간 합성
        bgmOn = (st != ST_DEAD);            // 죽으면 BGM 멈춤(긴장감)
        if(IsKeyPressed(KEY_M)) muted=!muted;

        if(st==ST_TITLE){
            if(pressed()){ st=ST_PLAY; reset_play(); }
        }
        else if(st==ST_PLAY){
            if(pressed()){ gravDir=-gravDir; onSurface=0; vy=gravDir*60.0f; flips++; PlaySound(sfxFlip); }
            vy += gravDir*2200.0f*dt;
            py += vy*dt;
            trailX[trailHead]=PLAYER_X; trailY[trailHead]=py; trailHead=(trailHead+1)%TRAIL;
            if(py>=FLOOR_Y-PSIZE){py=FLOOR_Y-PSIZE; if(gravDir>0){vy=0;onSurface=1;}}
            if(py<=CEIL_Y){py=CEIL_Y; if(gravDir<0){vy=0;onSurface=1;}}

            playTime += dt;
            speed = curve_speed(playTime);
            int pm=(int)(score/10); score += dt; if((int)(score/10)>pm){ flash=0.4f; PlaySound(sfxScore); }
            spawnTimer -= dt;
            if(spawnTimer<=0){ spawn(playTime); spawnTimer=curve_interval(playTime); }

            for(int i=0;i<MAX_OBST;i++){
                if(!obs[i].active)continue;
                obs[i].x -= speed*dt;
                if(obs[i].moving){
                    obs[i].y += obs[i].vy*dt;
                    if(obs[i].y<CEIL_Y){obs[i].y=CEIL_Y;obs[i].vy=-obs[i].vy;}
                    if(obs[i].y+obs[i].h>FLOOR_Y){obs[i].y=FLOOR_Y-obs[i].h;obs[i].vy=-obs[i].vy;}
                }
                if(!obs[i].passed && obs[i].x+obs[i].w < PLAYER_X){ obs[i].passed=1; pops[popHead].x=PLAYER_X+20; pops[popHead].y=py; pops[popHead].life=1.0f; popHead=(popHead+1)%POPS; } if(obs[i].x+obs[i].w<0){obs[i].active=0;continue;}
                if(aabb(PLAYER_X,py,PSIZE,PSIZE,obs[i].x,obs[i].y,obs[i].w,obs[i].h)){
                    st=ST_DEAD; deadTimer=0; shake=0.4f; PlaySound(sfxCrash);
                    if(score>best){best=score;save_best();}
                }
            }
        }
        else if(st==ST_DEAD){
            deadTimer+=dt;
            if(deadTimer>0.25f && pressed()){ st=ST_PLAY; reset_play(); }
        }
        if(shake>0){shake-=dt*1.3f; if(shake<0)shake=0;}
        if(flash>0){flash-=dt*1.5f; if(flash<0)flash=0;}
        for(int k=0;k<POPS;k++) if(pops[k].life>0){ pops[k].life-=dt*1.4f; pops[k].y-=dt*40.0f; }

        // ---- 렌더 ----
        int sx=0,sy=0;
        if(shake>0){sx=(rnd(11)-5)*(int)(shake*16); sy=(rnd(11)-5)*(int)(shake*16);}
        Camera2D cam={0}; cam.target=(Vector2){-(float)sx,-(float)sy}; cam.zoom=1.0f;

        BeginDrawing();
        int bgv=16+(int)((speed-190)*0.018f); if(bgv>38)bgv=38;
        int fv=(int)(flash*55);
        ClearBackground((Color){(unsigned char)(16+fv),(unsigned char)(16+fv/2),(unsigned char)(24+fv),255});
        BeginMode2D(cam);
        DrawRectangle(0,FLOOR_Y,SCRW,SCRH-FLOOR_Y,(Color){(unsigned char)(38+bgv),38,52,255});
        DrawRectangle(0,0,SCRW,CEIL_Y,(Color){(unsigned char)(38+bgv),38,52,255});
        for(int i=0;i<MAX_OBST;i++) if(obs[i].active){
            Color oc = obs[i].moving ? (Color){240,140,60,255} : (Color){220,80,90,255};
            DrawRectangle((int)obs[i].x,(int)obs[i].y,(int)obs[i].w,(int)obs[i].h,oc);
        }
        if(st!=ST_TITLE){ for(int k=0;k<TRAIL;k++){ int idx=(trailHead+k)%TRAIL; float a=(float)k/TRAIL; Color tc=gravDir>0?(Color){90,200,255,(unsigned char)(a*70)}:(Color){255,210,90,(unsigned char)(a*70)}; DrawRectangle((int)trailX[idx]+4,(int)trailY[idx]+4,PSIZE-8,PSIZE-8,tc);} }
        if(st!=ST_TITLE){
            Color pc = gravDir>0 ? (Color){90,200,255,255} : (Color){255,210,90,255};
            DrawRectangle(PLAYER_X,(int)py,PSIZE,PSIZE,pc);
            if(gravDir>0) DrawRectangle(PLAYER_X+8,(int)py+PSIZE+2,6,6,pc);
            else          DrawRectangle(PLAYER_X+8,(int)py-8,6,6,pc);
        }
        EndMode2D();
        for(int k=0;k<POPS;k++) if(pops[k].life>0){ DrawText("+1",(int)pops[k].x,(int)pops[k].y,18,(Color){120,230,140,(unsigned char)(pops[k].life*200)}); }

        // ---- HUD / 화면 ----
        char buf[64];
        if(st==ST_TITLE){
            DrawText("GRAVITY FLIP", SCRW/2-150, 150, 44, RAYWHITE);
            DrawText("SPACE / CLICK to flip gravity", SCRW/2-160, 230, 22, (Color){180,180,200,255});
            DrawText("dodge the walls. survive.", SCRW/2-120, 262, 18, (Color){130,130,150,255});
            snprintf(buf,sizeof(buf),"BEST  %.1f", best);
            DrawText(buf, SCRW/2-55, 320, 24, (Color){90,200,255,255});
            // 깜빡이는 시작 안내
            if(((int)(GetTime()*2))%2==0)
                DrawText("> press to start <", SCRW/2-95, 380, 20, (Color){255,210,90,255});
        } else {
            snprintf(buf,sizeof(buf),"%.1f", score);
            DrawText(buf, 12, 6, 26, RAYWHITE);
            snprintf(buf,sizeof(buf),"BEST %.1f", best);
            DrawText(buf, SCRW-150, 9, 18, (Color){150,160,180,255});
            // 온보딩: 첫 판 + 아직 한 번도 안 뒤집었으면 힌트
            if(st==ST_PLAY && playTime<3.0f && flips==0)
                DrawText("press SPACE to flip!", PLAYER_X-30, (int)py-34, 18, (Color){255,230,120,255});
            if(st==ST_DEAD){
                DrawText("CRASH", SCRW/2-75, SCRH/2-50, 44, (Color){255,90,100,255});
                snprintf(buf,sizeof(buf),"score %.1f", score);
                DrawText(buf, SCRW/2-55, SCRH/2+4, 22, RAYWHITE);
                if(score>=best && best>0)
                    DrawText("NEW BEST!", SCRW/2-65, SCRH/2+32, 20, (Color){90,200,255,255});
                if(((int)(GetTime()*2))%2==0)
                    DrawText("SPACE / CLICK to retry", SCRW/2-130, SCRH/2+64, 20, (Color){180,180,200,255});
            }
        }
        EndDrawing();
    }
    CloseWindow();
    return 0;
}
