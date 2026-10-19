param([string]$QaName = 'report-result-format')
$ErrorActionPreference = 'Stop'
$qa = Join-Path $PSScriptRoot ('qa/' + $QaName)
$base = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qa 'baseline.json') | ConvertFrom-Json
$plan = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qa 'plan.json') | ConvertFrom-Json
$excel = $null
$book = $null
$results = @()
try {
    # 사용자 Excel에 연결하지 않는다. 후보 파일만 별도 숨김 인스턴스로 연다.
    $excel = New-Object -ComObject Excel.Application
    $excel.Visible = $false
    $excel.DisplayAlerts = $false
    $excel.AskToUpdateLinks = $false
    $excel.AutomationSecurity = 3
    foreach ($b in $base.books) {
        $book = $excel.Workbooks.Open((Join-Path $qa ($b.key + '-candidate.xlsx')), 0, $true)
        $sheet = $book.Worksheets.Item($b.sheet)
        $groups = @($plan.groups.($b.key))
        $last = [int]$groups[-1].end
        $rc = [string][char](64 + $b.resultcol)
        $vc = [string][char](64 + $b.verdictcol)
        $region = $sheet.Range($rc + $b.first + ':' + $rc + $last)
        $measure = $book.Worksheets.Add()
        $region.Copy($measure.Range('A' + $b.first))
        $measure.Columns.Item(1).ColumnWidth = $sheet.Columns.Item($b.resultcol).ColumnWidth
        $measure.Range('A' + $b.first + ':A' + $last).Rows.AutoFit()
        $overflow = @()
        for ($r = $b.first; $r -le $last; $r++) {
            $current = [double]$sheet.Rows.Item($r).RowHeight
            $needed = [double]$measure.Rows.Item($r).RowHeight
            if ($needed -gt ($current + 1)) {
                $overflow += [pscustomobject]@{row=$r; current=$current; needed=$needed}
            }
        }
        $measure.Delete()
        $sheet.PageSetup.PrintTitleRows = ''
        $sheet.PageSetup.PrintTitleColumns = ''
        $sheet.PageSetup.Orientation = 2
        $sheet.PageSetup.PaperSize = 9
        $sheet.PageSetup.Zoom = $false
        $sheet.PageSetup.FitToPagesWide = 1
        $sheet.PageSetup.FitToPagesTall = $false
        $sheet.PageSetup.LeftMargin = 12
        $sheet.PageSetup.RightMargin = 12
        $sheet.PageSetup.TopMargin = 12
        $sheet.PageSetup.BottomMargin = 12
        $view = 0
        foreach ($area in $b.views) {
            $view++
            $sheet.PageSetup.PrintArea = $area
            $sheet.ExportAsFixedFormat(0, (Join-Path $qa ($b.key + '-native-' + $view + '.pdf')), 0, $true, $false)
        }
        $results += [pscustomobject]@{key=$b.key; readOnly=$book.ReadOnly; overflow=$overflow; rowCount=$sheet.UsedRange.Rows.Count; candidateSha256=(Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $qa ($b.key + '-candidate.xlsx'))).Hash.ToLowerInvariant()}
        $book.Close($false)
        [void][Runtime.InteropServices.Marshal]::ReleaseComObject($book)
        $book = $null
    }
    $results | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $qa 'native-checks.json')
    $results | Select-Object key,rowCount,@{n='overflowRows';e={$_.overflow.Count}} | ConvertTo-Json
} finally {
    if ($null -ne $book) { $book.Close($false); [void][Runtime.InteropServices.Marshal]::ReleaseComObject($book) }
    if ($null -ne $excel) { $excel.Quit(); [void][Runtime.InteropServices.Marshal]::ReleaseComObject($excel) }
}
