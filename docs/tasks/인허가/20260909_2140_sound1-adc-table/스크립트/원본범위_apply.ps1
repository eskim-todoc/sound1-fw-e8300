$ErrorActionPreference = 'Stop'
$path = 'C:/Users/EUNSU/.claude/jobs/00571323/tmp/work2.xlsx'
$SHEET = 'Sound1 시료 측정 데이터'

# dB, PCB상한(+5), PCB하한(-5), 기구상한(+5), 기구하한(-5)
$vals = @(
  @(45,54,17,31,10),
  @(50,96,30,56,17),
  @(55,172,54,100,31),
  @(60,306,96,179,56),
  @(65,544,172,319,100),
  @(70,969,306,567,179),
  @(75,1723,544,1008,319),
  @(80,3064,969,1794,567),
  @(85,5449,1723,3190,1008),
  @(90,9690,3064,5673,1794),
  @(95,17232,5449,10089,3190),
  @(100,30643,9690,17942,5673),
  @(105,54492,17232,31907,10089),
  @(110,96903,30643,56739,17942)
)
$note = '1.5세대와 Sound1 은 같은 스펙의 마이크를 쓴다. 그래서 1.5세대 테이블의 생성 규칙을 그대로 두고 Sound1 의 감쇠만 반영했다.' + [char]10 + 'ADC = floor(262143 x 10^((dB SPL - 120 + 감쇠) / 20)),  262143 = 2^18 - 1  (1.5세대 표가 내림으로 만들어져 있다)' + [char]10 + '감쇠는 고음압 평탄 구간(80~110 dB SPL) 실측에서 구했다.  PCB 상태 -3.644 dB = 5 시료 평균 (참조용, 시험 대상이 아니다)' + [char]10 + '기구 조립 -8.293 dB = 실측 6 시료의 상·하한 중앙 (판정용).  시험 허용 오차 +-5 dB 는 이 표에서 정확히 한 칸 위/아래다.'

$xl = New-Object -ComObject Excel.Application
$xl.Visible = $false
$xl.DisplayAlerts = $false
try {
  $wb = $xl.Workbooks.Open($path)

  # ---------- (1) 복사해 둔 테이블 시트의 머리 설명문 ----------
  $wsT = $wb.Worksheets.Item(1)
  $before = [string]$wsT.Range('B4').Value2
  if ($before -like '*round(262143*') {
    $wsT.Range('B4').Value2 = $note
    $wsT.Rows.Item(4).RowHeight = 68
    Write-Output '  테이블 시트 B4: round -> floor 정정, 시료 수 문구 정정'
  } elseif ($before -like '*floor(262143*') {
    Write-Output '  테이블 시트 B4: 이미 floor 다. 건드리지 않았다'
  } else {
    throw "테이블 시트 B4 가 예상과 다르다"
  }

  # ---------- (2) 측정 시트에 +-5 dB 범위 열 ----------
  $ws = $wb.Worksheets.Item($SHEET)
  # 서식을 기준 열(AD:AE)에서 가져온다. 병합까지 딸려오므로 풀고 다시 잡는다.
  $ws.Range('AD3:AE18').Copy() | Out-Null
  $ws.Range('AF3').PasteSpecial(-4122) | Out-Null
  $ws.Range('AH3').PasteSpecial(-4122) | Out-Null
  $ws.Range('AF3:AI3').UnMerge()

  $ws.Range('AF3:AG3').Merge()
  $ws.Range('AF3').Value2 = '허용 범위 (PCB 상태)'
  $ws.Range('AF4').Value2 = '+5 dB'
  $ws.Range('AG4').Value2 = '-5 dB'
  $ws.Range('AH3:AI3').Merge()
  $ws.Range('AH3').Value2 = '허용 범위 (기구 조립)'
  $ws.Range('AH4').Value2 = '+5 dB'
  $ws.Range('AI4').Value2 = '-5 dB'

  $r = 5
  foreach ($v in $vals) {
    $ws.Range("AF$r").Value2 = [int]$v[1]
    $ws.Range("AG$r").Value2 = [int]$v[2]
    $ws.Range("AH$r").Value2 = [int]$v[3]
    $ws.Range("AI$r").Value2 = [int]$v[4]
    $r++
  }
  Write-Output "  범위 열 기입: AF5:AI$($r-1)"

  # ---------- (3) 차트에 상·하한 계열 ----------
  $report = @()
  foreach ($co in $ws.ChartObjects()) {
    $ch = $co.Chart
    $names = @()
    foreach ($s in $ch.SeriesCollection()) { $names += [string]$s.Name }
    $joined = $names -join '|'
    if ($joined -like '*PCB 상태*') { $cols = @('AF','AG') }
    elseif ($joined -like '*기구 조립*') { $cols = @('AH','AI') }
    else { $report += "  $($co.Name): 기준 계열이 없어 건너뜀 [$joined]"; continue }

    $labels = @('허용 상한 (+5 dB)', '허용 하한 (-5 dB)')
    for ($i = 0; $i -lt 2; $i++) {
      $col = $cols[$i]
      $ns = $ch.SeriesCollection().NewSeries()
      $ns.XValues = "='$SHEET'!`$B`$4:`$B`$17"
      $ns.Values  = "='$SHEET'!`$$col`$5:`$$col`$18"
      $ns.Name    = $labels[$i]
      $ns.MarkerStyle = -4142
      $ns.Format.Line.Visible = -1
      $ns.Format.Line.ForeColor.RGB = 8421504
      $ns.Format.Line.Weight = 1.25
      $ns.Format.Line.DashStyle = 3
      $report += "  $($co.Name) <- $col  '$($ns.Name)'"
    }
  }

  $wb.Save()
  $wb.Close($true)
  Write-Output '--- 차트 ---'
  $report | ForEach-Object { Write-Output $_ }
  Write-Output '저장 완료'
}
finally {
  $xl.Quit()
  [System.Runtime.InteropServices.Marshal]::ReleaseComObject($xl) | Out-Null
}
