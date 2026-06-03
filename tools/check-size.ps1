# 1.44MB 용량 게이트 — 빌드 산출물 폴더 전체를 압축 해제 후 기준으로 합산 검증.
# 전략 §5 Top10 #2: 이 게이트를 빌드 실패 조건으로 1일차에 삽입.
# 사용법: pwsh -File 50-game\tools\check-size.ps1 [측정폴더]   (기본: 50-game\build)

param(
    [string]$Target = "50-game\build"
)

$LIMIT = 1474560          # 1.44MB 하드캡 (바이트)
$WARN  = 1371340          # 93% 경고선

if (-not (Test-Path $Target)) {
    Write-Host "[ERROR] 측정 대상 폴더 없음: $Target" -ForegroundColor Red
    Write-Host "        빈 창 빌드를 먼저 50-game\build\ 에 출력하라." -ForegroundColor Yellow
    exit 1
}

# -Force: 숨김 파일(Thumbs.db, desktop.ini, .pdb 등)까지 포함해 누락 함정 차단
$sum = (Get-ChildItem -Path $Target -Recurse -File -Force | Measure-Object -Property Length -Sum).Sum
if ($null -eq $sum) { $sum = 0 }

$pct = [math]::Round(($sum / $LIMIT) * 100, 2)
$free = $LIMIT - $sum

Write-Host "측정 폴더 : $Target"
Write-Host "합산 크기 : $sum bytes  ($pct% of 1.44MB)"
Write-Host "여유 공간 : $free bytes"

if ($sum -gt $LIMIT) {
    Write-Host "[FAIL] 1,474,560B 초과 — 제출 시 즉시 실격. 빌드 실패 처리." -ForegroundColor Red
    exit 1
} elseif ($sum -gt $WARN) {
    Write-Host "[WARN] 93% 경고선 초과 — 안전 여유분(7~10%) 잠식. 다이어트 필요." -ForegroundColor Yellow
    exit 0
} else {
    Write-Host "[PASS] 한도 내. 안전." -ForegroundColor Green
    exit 0
}
