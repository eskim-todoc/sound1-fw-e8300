param([ValidateSet('integration','system')][string]$Kind)
$ErrorActionPreference = 'Stop'
$qa = Join-Path $PSScriptRoot 'qa/validation-reports'
$plan = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qa "$Kind-plan.json") | ConvertFrom-Json
$excel = $null
$book = $null
try {
    # Independent hidden instance: render a read-only candidate, never the user's workbook.
    $excel = New-Object -ComObject Excel.Application
    $excel.Visible = $false
    $excel.DisplayAlerts = $false
    $excel.AskToUpdateLinks = $false
    $excel.AutomationSecurity = 3
    $book = $excel.Workbooks.Open((Join-Path $qa "$Kind-candidate.xlsx"), 0, $true)
    $name = if ($Kind -eq 'integration') { '보고서_통합시험' } else { '보고서_시스템 시험' }
    $sheet = $book.Worksheets.Item($name)
    $areas = @($plan.images | ForEach-Object { $plan.expectedCol + $_.row + ':' + $plan.actualCol + $_.row })
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
    $sheet.ExportAsFixedFormat(0, (Join-Path $qa "$Kind-images.pdf"), 0, $true, $false)
    $cover = $book.Worksheets.Item('표지')
    $cover.PageSetup.PrintArea = 'B2:O24'
    $cover.PageSetup.Zoom = $false
    $cover.PageSetup.FitToPagesWide = 1
    $cover.PageSetup.FitToPagesTall = 1
    $cover.ExportAsFixedFormat(0, (Join-Path $qa "$Kind-cover.pdf"), 0, $true, $false)
    [pscustomobject]@{kind=$Kind;pictures=$sheet.Shapes.Count;readonly=$book.ReadOnly;areas=$areas} | ConvertTo-Json
} finally {
    if ($null -ne $book) { $book.Close($false); [void][Runtime.InteropServices.Marshal]::ReleaseComObject($book) }
    if ($null -ne $excel) { $excel.Quit(); [void][Runtime.InteropServices.Marshal]::ReleaseComObject($excel) }
}
