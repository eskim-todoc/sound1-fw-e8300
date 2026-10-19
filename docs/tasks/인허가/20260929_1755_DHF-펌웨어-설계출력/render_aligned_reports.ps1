$ErrorActionPreference = 'Stop'
$qa = Join-Path $PSScriptRoot 'qa/criterion-report-alignment'
$plans = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qa 'plan.json') | ConvertFrom-Json
$excel = $null
$book = $null
$results = @()
try {
    # Use a separate hidden instance and read-only QA candidates.
    $excel = New-Object -ComObject Excel.Application
    $excel.Visible = $false
    $excel.DisplayAlerts = $false
    $excel.AskToUpdateLinks = $false
    $excel.AutomationSecurity = 3
    foreach ($plan in $plans) {
        $book = $excel.Workbooks.Open((Join-Path $qa ($plan.kind + '-candidate.xlsx')), 0, $true)
        $built = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qa ($plan.kind + '-built.json')) | ConvertFrom-Json
        $images = $book.Worksheets.Item('예상 그림')
        $areas = @($built.imageRows | ForEach-Object { 'A' + $_.row + ':C' + ($_.row + 1) })
        $images.PageSetup.PrintArea = $areas -join ','
        $images.PageSetup.PrintTitleRows = ''
        $images.PageSetup.PrintTitleColumns = ''
        $images.PageSetup.Orientation = 2
        $images.PageSetup.PaperSize = 9
        $images.PageSetup.Zoom = $false
        $images.PageSetup.FitToPagesWide = 1
        $images.PageSetup.FitToPagesTall = $false
        $images.PageSetup.LeftMargin = 12
        $images.PageSetup.RightMargin = 12
        $images.PageSetup.TopMargin = 12
        $images.PageSetup.BottomMargin = 12
        $images.ExportAsFixedFormat(0, (Join-Path $qa ($plan.kind + '-native-images.pdf')), 0, $true, $false)
        $main = $book.Worksheets.Item($plan.reportSheet)
        $actual = [string][char](65 + [int]$plan.commonEnd)
        $verdict = [string][char](66 + [int]$plan.commonEnd)
        $picks = @($plan.records | Sort-Object -Property height -Descending | Select-Object -First 3)
        $main.PageSetup.PrintArea = (@($picks | ForEach-Object { $actual + $_.row + ':' + $verdict + $_.row })) -join ','
        $main.PageSetup.PrintTitleRows = ''
        $main.PageSetup.PrintTitleColumns = ''
        $main.PageSetup.Orientation = 2
        $main.PageSetup.PaperSize = 9
        $main.PageSetup.Zoom = $false
        $main.PageSetup.FitToPagesWide = 1
        $main.PageSetup.FitToPagesTall = $false
        $main.ExportAsFixedFormat(0, (Join-Path $qa ($plan.kind + '-native-long-results.pdf')), 0, $true, $false)
        $results += [pscustomobject]@{kind=$plan.kind; readonly=$book.ReadOnly; mainRows=$main.UsedRange.Rows.Count; mainPictures=$main.Shapes.Count; expectedPictures=$images.Shapes.Count; imageAreas=$areas; longIds=@($picks | ForEach-Object { $_.id })}
        $book.Close($false)
        [void][Runtime.InteropServices.Marshal]::ReleaseComObject($book)
        $book = $null
    }
    $results | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $qa 'native-checks.json')
    $results | ConvertTo-Json -Depth 6
} finally {
    if ($null -ne $book) { $book.Close($false); [void][Runtime.InteropServices.Marshal]::ReleaseComObject($book) }
    if ($null -ne $excel) { $excel.Quit(); [void][Runtime.InteropServices.Marshal]::ReleaseComObject($excel) }
}
