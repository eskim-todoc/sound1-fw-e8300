$ErrorActionPreference = 'Stop'
$qa = Join-Path $PSScriptRoot 'qa/unit-inline-acceptance'
$plan = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qa 'plan.json') | ConvertFrom-Json
$excel = $null
$book = $null
$results = @()
try {
    # A separate hidden Excel instance: never save or close user workbooks.
    $excel = New-Object -ComObject Excel.Application
    $excel.Visible = $false
    $excel.DisplayAlerts = $false
    $excel.AskToUpdateLinks = $false
    $excel.AutomationSecurity = 3
    foreach ($b in $plan.baseline.books) {
        $book = $excel.Workbooks.Open((Join-Path $qa ($b.key + '-candidate.xlsx')), 0, $true)
        $sheet = $book.Worksheets.Item($b.sheet)
        $last = if ($b.key -eq 'report') { 'L' } else { 'K' }
        # Native AutoFit validates unmerged acceptance/result cells. Merged
        # metadata is deliberately not used to size the numerical subrows.
        $region = $sheet.Range('K30:' + $last + $plan.endNew)
        $before = @{}
        for ($n = 30; $n -le $plan.endNew; $n++) { $before[$n] = [double]$sheet.Rows.Item($n).RowHeight }
        # Native Excel refuses AutoFit for rows containing a merged verdict.
        # Copy ONLY unmerged payload columns to a temporary QA worksheet.
        $measure = $book.Worksheets.Add()
        $region.Copy($measure.Range('A30'))
        $measure.Columns.Item(1).ColumnWidth = $sheet.Columns.Item(11).ColumnWidth
        if ($b.key -eq 'report') { $measure.Columns.Item(2).ColumnWidth = $sheet.Columns.Item(12).ColumnWidth }
        $measure.Range('A30:' + $(if ($b.key -eq 'report') {'B'} else {'A'}) + $plan.endNew).Rows.AutoFit()
        $overflow = @()
        for ($n = 30; $n -le $plan.endNew; $n++) {
            $needed = [double]$measure.Rows.Item($n).RowHeight
            if ($needed -gt ($before[$n] + 1)) { $overflow += [pscustomobject]@{row=$n; current=$before[$n]; needed=$needed} }
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
        # Entire criterion/result payload for extraction/clipping checks.
        $sheet.PageSetup.PrintArea = 'K30:' + $last + $plan.endNew
        $sheet.ExportAsFixedFormat(0, (Join-Path $qa ($b.key + '-all.pdf')), 0, $true, $false)
        foreach ($uid in @('CFX-001','CFX-020','CFX-023','HEAR-002','CM3-028','CM3-051','BOOT-018')) {
            $rec = $plan.records | Where-Object id -eq $uid
            $sheet.PageSetup.PrintArea = 'K' + $rec.start + ':' + $last + $rec.end
            $sheet.ExportAsFixedFormat(0, (Join-Path $qa ($b.key + '-' + $uid + '-native.pdf')), 0, $true, $false)
        }
        $shapeCounts = @()
        foreach ($s in $book.Worksheets) { $shapeCounts += [pscustomobject]@{sheet=$s.Name; shapes=$s.Shapes.Count} }
        $results += [pscustomobject]@{key=$b.key; readonly=$book.ReadOnly; rows=$sheet.UsedRange.Rows.Count; overflow=$overflow; sheets=$shapeCounts}
        $book.Close($false)
        [void][Runtime.InteropServices.Marshal]::ReleaseComObject($book)
        $book = $null
    }
    $results | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $qa 'native-checks.json')
    $results | Select-Object key,rows,@{n='overflowRows';e={$_.overflow.Count}} | ConvertTo-Json
} finally {
    if ($null -ne $book) { $book.Close($false); [void][Runtime.InteropServices.Marshal]::ReleaseComObject($book) }
    if ($null -ne $excel) { $excel.Quit(); [void][Runtime.InteropServices.Marshal]::ReleaseComObject($excel) }
}
