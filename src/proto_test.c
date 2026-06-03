// 프로토타입 로직 자가검증 (렌더/입력 없이) — 절차생성·도달가능성·시그니처 메커니즘 정합 확인.
// raylib 불필요, 순수 C. gcc src/proto_test.c -o build/proto_test.exe 로 빌드.
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define GW 20
#define GH 15
#define MAXEN 6

static uint64_t g_state;
static uint64_t splitmix64(void){ uint64_t z=(g_state+=0x9e3779b97f4a7c15ULL); z=(z^(z>>30))*0xbf58476d1ce4e5b9ULL; z=(z^(z>>27))*0x94d049bb133111ebULL; return z^(z>>31);}
static int rnd(int n){ return (int)(splitmix64()%(uint64_t)n);}

static int tile[GH][GW], bone[GH][GW];
static int px,py,ex,ey,enx[MAXEN],eny[MAXEN],encount,run=1;

static int reachable(int sx,int sy,int tx,int ty){
    static int seen[GH][GW]; memset(seen,0,sizeof(seen));
    int qx[GW*GH],qy[GW*GH],h=0,t=0; qx[t]=sx;qy[t]=sy;t++;seen[sy][sx]=1;
    while(h<t){int cx=qx[h],cy=qy[h];h++; if(cx==tx&&cy==ty)return 1;
        int dx[4]={1,-1,0,0},dy[4]={0,0,1,-1};
        for(int i=0;i<4;i++){int nx=cx+dx[i],ny=cy+dy[i];
            if(nx<0||ny<0||nx>=GW||ny>=GH)continue; if(seen[ny][nx])continue;
            if(tile[ny][nx]==1||tile[ny][nx]==2)continue; seen[ny][nx]=1;qx[t]=nx;qy[t]=ny;t++;}}
    return 0;
}
static void gen_map(uint64_t seed){
    g_state=seed; int tries=0;
    for(;;){ tries++;
        for(int y=0;y<GH;y++)for(int x=0;x<GW;x++){
            if(x==0||y==0||x==GW-1||y==GH-1)tile[y][x]=1;
            else tile[y][x]=(rnd(100)<30)?1:0;
            if(bone[y][x])tile[y][x]=2;}
        px=1;py=1;ex=GW-2;ey=GH-2; tile[py][px]=0;tile[ey][ex]=0;
        if(bone[py][px]||bone[ey][ex])continue;
        if(!reachable(px,py,ex,ey))continue;
        encount=3+(run/2<3?run/2:3); if(encount>MAXEN)encount=MAXEN;
        int placed=0,guard=0;
        while(placed<encount&&guard<500){guard++; int rx=1+rnd(GW-2),ry=1+rnd(GH-2);
            if(tile[ry][rx]!=0)continue; if(abs(rx-px)+abs(ry-py)<6)continue;
            enx[placed]=rx;eny[placed]=ry;placed++;}
        encount=placed;
        printf("  gen: tries=%d enemies=%d reachable=YES\n", tries, encount);
        return;
    }
}

int main(void){
    int fails=0;
    printf("=== 프로토타입 로직 자가검증 ===\n");

    // T1: 같은 시드 → 같은 맵 (결정론)
    memset(bone,0,sizeof(bone)); gen_map(12345);
    int snap[GH][GW]; memcpy(snap,tile,sizeof(tile)); int sp_px=px,sp_ex=ex;
    memset(bone,0,sizeof(bone)); gen_map(12345);
    int same = (memcmp(snap,tile,sizeof(tile))==0 && px==sp_px && ex==sp_ex);
    printf("T1 결정론(같은 시드=같은 맵): %s\n", same?"PASS":"FAIL"); if(!same)fails++;

    // T2: 다른 시드 → 다른 맵
    memset(bone,0,sizeof(bone)); gen_map(99999);
    int diff = (memcmp(snap,tile,sizeof(tile))!=0);
    printf("T2 다양성(다른 시드=다른 맵): %s\n", diff?"PASS":"FAIL"); if(!diff)fails++;

    // T3: 100개 시드 전부 출구 도달 가능 (재미없는 시드 0)
    int unreach=0;
    for(int s=0;s<100;s++){ memset(bone,0,sizeof(bone)); gen_map(s*2654435761u);
        if(!reachable(px,py,ex,ey))unreach++; }
    printf("T3 도달가능성(100 시드): 불가 %d개 → %s\n", unreach, unreach==0?"PASS":"FAIL"); if(unreach)fails++;

    // T4: 시그니처 — 죽은 자리가 다음 런의 뼈벽
    memset(bone,0,sizeof(bone)); gen_map(777);
    int dx_=5,dy_=5; while(tile[dy_][dx_]!=0){dx_++;if(dx_>=GW-1){dx_=1;dy_++;}}
    bone[dy_][dx_]=1;                       // 죽음 각인
    gen_map(778);
    int imprinted = (tile[dy_][dx_]==2);    // 다음 맵에서 뼈벽(2)인가
    printf("T4 시그니처(죽음→뼈벽 누적): %s\n", imprinted?"PASS":"FAIL"); if(!imprinted)fails++;

    // T5: 적은 항상 시작점에서 6칸 이상 (즉사 방지)
    int tooClose=0;
    for(int s=0;s<50;s++){ memset(bone,0,sizeof(bone)); gen_map(s*40503u+1);
        for(int i=0;i<encount;i++) if(abs(enx[i]-px)+abs(eny[i]-py)<6)tooClose++; }
    printf("T5 적 배치(시작점 6칸+): 위반 %d개 → %s\n", tooClose, tooClose==0?"PASS":"FAIL"); if(tooClose)fails++;

    printf("\n결과: %s (%d fail)\n", fails==0?"전부 PASS ✅":"실패 있음 ❌", fails);
    return fails;
}
