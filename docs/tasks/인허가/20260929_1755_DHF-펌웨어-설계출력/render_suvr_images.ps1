$ErrorActionPreference = 'Stop'
$qa = Join-Path $PSScriptRoot 'qa/suvr'
$plan = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qa 'direct-plan.json') | ConvertFrom-Json
$excel = $null
$book = $null
try {
    # Separate hidden instance, read-only QA copy. Never attach to the user's Excel.
    $excel = New-Object -ComObject Excel.Application
    $excel.Visible = $false
    $excel.DisplayAlerts = $false
    $excel.AskToUpdateLinks = $false
    $excel.AutomationSecurity = 3
    $book = $excel.Workbooks.Open((Join-Path $qa 'direct-candidate.xlsx'), 0, $true)
    $sheet = $book.Worksheets.Item('보고서_유닛 시험')
    $areas = @($plan.images | ForEach-Object { 'K' + $_.row + ':L' + $_.row })
    $sheet.PageSetup.PrintArea = $areas -join ','
    $sheet.PageSetup.PrintTitleRows = ''
    $sheet.PageSetup.PrintTitleColumns = ''
    $sheet.PageSetup.Orientation = 2
    $sheet.PageSetup.PaperSize = 9
    $sheet.PageSetup.Zoom = $false
    $sheet.PageSetup.FitToPagesWide = 1
    $sheet.PageSetup.FitToPagesTall = $false
    $sheet.PageSetup.LeftMargin = 18
    $sheet.PageSetup.RightMargin = 18
    $sheet.PageSetup.TopMargin = 18
    $sheet.PageSetup.BottomMargin = 18
    $sheet.ExportAsFixedFormat(0, (Join-Path $qa 'direct-images.pdf'), 0, $true, $false)
    $cover = $book.Worksheets.Item('표지')
    $cover.PageSetup.PrintArea = 'B2:O24'
    $cover.PageSetup.Zoom = $false
    $cover.PageSetup.FitToPagesWide = 1
    $cover.PageSetup.FitToPagesTall = 1
    $cover.ExportAsFixedFormat(0, (Join-Path $qa 'direct-cover.pdf'), 0, $true, $false)
    [pscustomobject]@{pictures=$sheet.Shapes.Count;readonly=$book.ReadOnly;areas=$areas;output=(Join-Path $qa 'direct-images.pdf')} | ConvertTo-Json
} finally {
    if ($null -ne $book) { $book.Close($false) }
    if ($null -ne $excel) { $excel.Quit(); [void][Runtime.InteropServices.Marshal]::ReleaseComObject($excel) }
}
