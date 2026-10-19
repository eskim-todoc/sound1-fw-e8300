param([string]$QaName = 'unit-merged-conditions')
$ErrorActionPreference = 'Stop'
$qa = Join-Path $PSScriptRoot ('qa/' + $QaName)
$base = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qa 'baseline.json') | ConvertFrom-Json
$plan = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qa 'plan.json') | ConvertFrom-Json
$excel = $null; $book = $null; $results = @()
try {
    # 사용자 Excel에는 연결하지 않고 QA 후보를 별도 숨김 인스턴스로 읽는다.
    $excel = New-Object -ComObject Excel.Application
    $excel.Visible = $false; $excel.DisplayAlerts = $false
    $excel.AskToUpdateLinks = $false; $excel.AutomationSecurity = 3
    foreach ($b in $base.books) {
        $candidate = Join-Path $qa ($b.key + '-candidate.xlsx')
        $book = $excel.Workbooks.Open($candidate, 0, $true)
        $measure = $book.Worksheets.Add()
        $measure.Range('A1').NumberFormat = '@'
        $measure.Range('A1').WrapText = $true
        $measure.Range('A1').HorizontalAlignment = -4131
        $overflow = @(); $checked = @()
        foreach ($m in @($plan.merges | Where-Object key -eq $b.key)) {
            $sheet = $book.Worksheets.Item($m.sheet)
            $range = $sheet.Range($m.range)
            if (-not $range.MergeCells) { throw ('병합 누락: ' + $m.range) }
            $cell = $range.Cells.Item(1,1)
            $value = [string]$cell.Value2
            $measure.Columns.Item(1).ColumnWidth = $sheet.Columns.Item($cell.Column).ColumnWidth
            $face = $cell.Characters(1,1).Font.Name
            $size = $cell.Characters(1,1).Font.Size
            if ($face -is [DBNull] -or -not $face) { $face='맑은 고딕' }
            if ($size -is [DBNull] -or -not $size) { $size=11 }
            $measure.Range('A1').Font.Name = $face
            $measure.Range('A1').Font.Size = $size
            # 부분 굵은 글씨가 있는 셀은 굵은 글씨로 보수적으로 높이를 확인한다.
            $measure.Range('A1').Font.Bold = $true
            $lines = @($value -split "`n")
            $needed = 0.0
            # Excel 한 행의 최대 높이 409.5pt 때문에 8줄씩 나누어 측정한다.
            # 이 분할은 QA 메모리 안에서만 하며 산출물에는 한 셀로 남긴다.
            for ($i=0; $i -lt $lines.Count; $i+=8) {
                $end = [Math]::Min($i+7,$lines.Count-1)
                $measure.Range('A1').Value2 = ($lines[$i..$end] -join "`n")
                [void]$measure.Rows.Item(1).AutoFit()
                $h = [double]$measure.Rows.Item(1).RowHeight
                if ($h -ge 409.5) { throw ('높이 측정 상한 도달: ' + $m.range) }
                $needed += $h
            }
            $actual = 0.0
            for ($i=1; $i -le $range.Rows.Count; $i++) { $actual += [double]$range.Rows.Item($i).RowHeight }
            $row = [pscustomobject]@{sheet=$m.sheet; range=$m.range; actual=$actual; needed=$needed; characters=$value.Length}
            $checked += $row
            if ($needed -gt $actual+1) { $overflow += $row }
        }
        # 병합 대상이 아닌 중복 표기 수정 셀도 기존 단일 행 높이로 확인한다.
        $main = $book.Worksheets.Item($b.sheet)
        foreach ($p in @($plan.changes | Where-Object { $_.key -eq $b.key -and $_.sheet -eq $b.sheet -and $_.value -ne '' })) {
            $cell = $main.Range($p.cell)
            if ($cell.MergeCells) { continue }
            $measure.Columns.Item(1).ColumnWidth = $main.Columns.Item($cell.Column).ColumnWidth
            $measure.Range('A1').Font.Name = $cell.Characters(1,1).Font.Name
            $measure.Range('A1').Font.Size = $cell.Characters(1,1).Font.Size
            $measure.Range('A1').Font.Bold = $true
            $measure.Range('A1').Value2 = [string]$cell.Value2
            [void]$measure.Rows.Item(1).AutoFit()
            $need = [double]$measure.Rows.Item(1).RowHeight
            if ($need -gt [double]$cell.RowHeight+1) { $overflow += [pscustomobject]@{sheet=$b.sheet; range=$p.cell; actual=$cell.RowHeight; needed=$need} }
        }
        [void]$measure.Delete()
        $main.PageSetup.PrintTitleRows = ''; $main.PageSetup.PrintTitleColumns = ''
        $main.PageSetup.Orientation = 2; $main.PageSetup.PaperSize = 9
        $main.PageSetup.Zoom = $false; $main.PageSetup.FitToPagesWide = 1
        $main.PageSetup.FitToPagesTall = 1
        $main.PageSetup.LeftMargin = 12; $main.PageSetup.RightMargin = 12
        $main.PageSetup.TopMargin = 12; $main.PageSetup.BottomMargin = 12
        $view = 0
        foreach ($area in $b.views) {
            $view++; $main.PageSetup.PrintArea = $area
            $main.ExportAsFixedFormat(0, (Join-Path $qa ($b.key + '-native-' + $view + '.pdf')), 0, $true, $false)
        }
        $results += [pscustomobject]@{key=$b.key; readOnly=$book.ReadOnly; overflow=$overflow; checked=$checked; rowCount=$main.UsedRange.Rows.Count; candidateSha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $candidate).Hash.ToLowerInvariant()}
        $book.Close($false); [void][Runtime.InteropServices.Marshal]::ReleaseComObject($book); $book=$null
    }
    $results | ConvertTo-Json -Depth 10 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $qa 'native-checks.json')
    $results | Select-Object key,rowCount,@{n='mergedCellsChecked';e={$_.checked.Count}},@{n='overflowCells';e={$_.overflow.Count}} | ConvertTo-Json
} finally {
    if ($null -ne $book) { $book.Close($false); [void][Runtime.InteropServices.Marshal]::ReleaseComObject($book) }
    if ($null -ne $excel) { $excel.Quit(); [void][Runtime.InteropServices.Marshal]::ReleaseComObject($excel) }
}
