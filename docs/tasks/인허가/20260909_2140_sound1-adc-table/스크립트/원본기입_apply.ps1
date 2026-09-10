$ErrorActionPreference = 'Stop'
$path = 'C:\Users\EUNSU\.claude\jobs\00571323\tmp\work.xlsx'
$SHEET = 'Sound1 시료 측정 데이터'

$vals = @(
  @(45,30,17), @(50,54,31), @(55,96,56), @(60,172,100), @(65,306,179),
  @(70,544,319), @(75,969,567), @(80,1723,1008), @(85,3064,1794),
  @(90,5449,3190), @(95,9690,5673), @(100,17232,10089),
  @(105,30643,17942), @(110,54492,31907)
)

$xl = New-Object -ComObject Excel.Application
$xl.Visible = $false
$xl.DisplayAlerts = $false
try {
  $wb = $xl.Workbooks.Open($path)
  $ws = $wb.Worksheets.Item($SHEET)

  # 서식을 기존 시료 열(AB:AC)에서 그대로 가져온다
  $ws.Range('AB3:AC18').Copy() | Out-Null
  $ws.Range('AD3').PasteSpecial(-4122) | Out-Null

  $ws.Range('AD3:AE3').Merge()
  $ws.Range('AD3').Value2 = 'Sound1 산출 기준'
  $ws.Range('AD4').Value2 = 'PCB 상태'
  $ws.Range('AE4').Value2 = '기구 조립'

  # 측정값은 헤더보다 한 행 아래다 (45 dBSPL -> r5)
  $r = 5
  foreach ($v in $vals) {
    $ws.Cells.Item($r, 30).Value2 = $v[1]
    $ws.Cells.Item($r, 31).Value2 = $v[2]
    $r++
  }

  $report = @()
  foreach ($co in $ws.ChartObjects()) {
    $ch = $co.Chart
    $names = @()
    foreach ($s in $ch.SeriesCollection()) { $names += [string]$s.Name }
    $joined = $names -join '|'
    if ($joined -match '501C|501D|501E|501F|5020') {
      $col = 'AD'; $title = 'Sound1 산출 기준 (PCB 상태)'
    } elseif ($joined -match '500A|500C|500D|500E|500F') {
      $col = 'AE'; $title = 'Sound1 산출 기준 (기구 조립)'
    } else {
      $report += "  $($co.Name): 판별 불가, 건너뜀 [$joined]"
      continue
    }

    $ns = $ch.SeriesCollection().NewSeries()
    # 값을 먼저 잡고 이름을 마지막에 준다.
    # 이름을 먼저 주면 XValues 를 설정할 때 Excel 이 이름을 다시 추론해 덮어쓴다.
    $ns.XValues = "='$SHEET'!`$B`$4:`$B`$17"
    $ns.Values  = "='$SHEET'!`$$col`$5:`$$col`$18"
    $ns.Name    = $title
    $ns.MarkerStyle = -4142
    $ns.Format.Line.Visible = -1
    $ns.Format.Line.ForeColor.RGB = 0
    $ns.Format.Line.Weight = 2.25
    $ns.Format.Line.DashStyle = 4
    $report += "  $($co.Name) <- $col  '$($ns.Name)'  (기존 $($names.Count) 계열: $joined)"
  }

  $wb.Save()
  $wb.Close($true)
  Write-Output '--- 차트 계열 추가 ---'
  $report | ForEach-Object { Write-Output $_ }
  Write-Output '저장 완료'
}
finally {
  $xl.Quit()
  [System.Runtime.InteropServices.Marshal]::ReleaseComObject($xl) | Out-Null
}
