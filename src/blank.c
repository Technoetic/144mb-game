// 1주차 베이스라인 실측용 빈 창 — raylib 정적 링크 exe 크기 측정 전용.
// 전략 §2: "너의 실제 빌드 구성으로 빈 창 exe + UPX 후 크기를 바이트 단위로 측정."
#include "raylib.h"

int main(void)
{
    InitWindow(640, 480, "1.44MB baseline");
    SetTargetFPS(60);

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(BLACK);
        DrawText("1.44MB", 270, 220, 40, RAYWHITE);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
