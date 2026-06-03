// passed 슬롯재사용 버그 회귀 테스트 (헤드리스, raylib 불필요).
// 검증: 슬롯이 재사용될 때 passed가 0으로 초기화되는가.
// 버그 시나리오: 장애물 통과(passed=1)→비활성화→같은 슬롯 재사용 시 passed=1 잔존
//   → 새 장애물이 "이미 통과됨"으로 오인되어 +1 팝업 누락.
#include <stdio.h>

#define MAX_OBST 16
typedef struct { float x,y,w,h; float vy; int moving; int active; int passed; } Obst;
static Obst obs[MAX_OBST];

// game.c와 동일한 (수정된) slot()
static Obst* slot(void){ for(int i=0;i<MAX_OBST;i++) if(!obs[i].active){ obs[i].passed=0; return &obs[i]; } return 0; }

int main(void){
    int fail=0;

    // 1) 전체 초기화 (reset_play 모사: active=0만)
    for(int i=0;i<MAX_OBST;i++) obs[i].active=0;

    // 2) 슬롯0에 장애물 생성 → 통과(passed=1) → 비활성화
    Obst* a = slot();
    if(a != &obs[0]){ printf("FAIL: 첫 slot이 obs[0] 아님\n"); fail++; }
    a->active=1;
    a->passed=1;          // 플레이어가 통과했다고 표시
    a->active=0;          // 화면 밖으로 나가 비활성화

    // 3) 같은 슬롯0 재사용 — 여기서 passed가 0으로 리셋돼야 함
    Obst* b = slot();
    if(b != &obs[0]){ printf("FAIL: 재사용 slot이 obs[0] 아님\n"); fail++; }
    if(b->passed != 0){
        printf("FAIL: 슬롯 재사용 후 passed=%d (버그: 1 잔존 → +1 팝업 누락)\n", b->passed);
        fail++;
    } else {
        printf("PASS: 슬롯 재사용 시 passed=0 초기화 확인 (새 장애물이 +1 팝업 정상 발생)\n");
    }

    // 4) 여러 슬롯 연속 재사용 시나리오 (실전: 30초+ 플레이)
    for(int round=0; round<5; round++){
        for(int i=0;i<MAX_OBST;i++) obs[i].active=0;
        for(int k=0;k<MAX_OBST;k++){ Obst*o=slot(); if(o){o->active=1;o->passed=1;} }
        // 전부 비활성화 후 재확보
        for(int i=0;i<MAX_OBST;i++) obs[i].active=0;
        Obst*o=slot();
        if(o && o->passed!=0){ printf("FAIL: round %d 재사용 passed 잔존\n",round); fail++; }
    }
    if(!fail) printf("PASS: 5라운드 연속 슬롯 재사용 모두 passed=0\n");

    printf(fail? "\n=== %d FAIL ===\n" : "\n=== ALL PASS ===\n", fail);
    return fail?1:0;
}
