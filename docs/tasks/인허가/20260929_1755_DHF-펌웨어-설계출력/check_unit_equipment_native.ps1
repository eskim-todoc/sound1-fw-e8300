$ErrorActionPreference = 'Stop'
$qaPath = Join-Path $PSScriptRoot 'qa/unit-equipment-repair'
$excelInstance = $null
$candidateBook = $null
try {
    $excelInstance = New-Object -ComObject Excel.Application
    $excelInstance.Visible = $false
    $excelInstance.DisplayAlerts = $false
    $excelInstance.AutomationSecurity = 3
    $excelInstance.AskToUpdateLinks = $false
    $candidateBook = $excelInstance.Workbooks.Open((Join-Path $qaPath 'criterion-candidate.xlsx'),0,$true)
    $sheet = $candidateBook.Worksheets.Item('기준서_유닛 검증')
    $scratch = $candidateBook.Worksheets.Add()
    $checks = @()
    for ($row = 15; $row -le 24; $row++) {
        foreach ($column in @(3,4)) {
            $cell = $sheet.Cells.Item($row,$column)
            [void]$scratch.Cells.Clear()
            [void]$cell.MergeArea.Copy($scratch.Range('A1'))
            [void]$scratch.UsedRange.UnMerge()
            $desiredWidth = [double]$cell.MergeArea.Width
            $scratch.Columns.Item(1).ColumnWidth = [Math]::Min(255,$desiredWidth/5.25)
            for ($i=0; $i -lt 2; $i++) {
                $scratch.Columns.Item(1).ColumnWidth = [Math]::Min(255,$scratch.Columns.Item(1).ColumnWidth*$desiredWidth/$scratch.Columns.Item(1).Width)
            }
            [void]$scratch.Rows.Item(1).AutoFit()
            $checks += [pscustomobject]@{
                cell=([string][char](64+$column))+$row
                needed=[double]$scratch.Rows.Item(1).RowHeight
                actual=[double]$cell.MergeArea.Height
                wrap=[bool]$cell.WrapText
                formula=[bool]$cell.HasFormula
            }
        }
    }
    $short = @($checks | Where-Object {$_.needed -gt $_.actual+1})
    [pscustomobject]@{readOnly=$candidateBook.ReadOnly; measuredCells=20; heightShortage=$short.Count; checks=$checks} |
        ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $qaPath 'native-checks.json')
    if ($short.Count) {throw ('글 높이 부족: '+($short.cell -join ', '))}
    if (@($checks | Where-Object {-not $_.wrap -or $_.formula}).Count) {throw '줄바꿈/셀 형식 오류'}
    if ($sheet.Range('B13').WrapText) {throw '제목 줄바꿈 오류'}
    [void]$scratch.Delete()
    [pscustomobject]@{readOnly=$candidateBook.ReadOnly; titleWrap=$false; measuredCells=20; heightShortage=0; checks=$checks} |
        ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $qaPath 'native-checks.json')
    [pscustomobject]@{readOnly=$candidateBook.ReadOnly; measuredCells=20; heightShortage=0; titleWrap=$false} | ConvertTo-Json
} finally {
    if ($null -ne $candidateBook) {$candidateBook.Close($false);[void][Runtime.InteropServices.Marshal]::ReleaseComObject($candidateBook)}
    if ($null -ne $excelInstance) {$excelInstance.Quit();[void][Runtime.InteropServices.Marshal]::ReleaseComObject($excelInstance)}
}
