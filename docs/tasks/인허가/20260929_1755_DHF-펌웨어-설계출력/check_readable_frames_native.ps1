$ErrorActionPreference = 'Stop'
$qaPath = Join-Path $PSScriptRoot 'qa/integration-readable-frames'
$planInfo = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qaPath 'plan.json') | ConvertFrom-Json
$excelInstance = $null
$candidateBook = $null
$checks = @()
try {
    # 사용자의 Excel과 별도인 숨김 인스턴스로 읽기 전용 표시 검사만 수행한다.
    $excelInstance = New-Object -ComObject Excel.Application
    $excelInstance.Visible = $false
    $excelInstance.DisplayAlerts = $false
    $excelInstance.AskToUpdateLinks = $false
    $excelInstance.AutomationSecurity = 3
    foreach ($kind in @('report', 'criterion')) {
        $candidatePath = Join-Path $qaPath ($kind + '-candidate.xlsx')
        $candidateBook = $excelInstance.Workbooks.Open($candidatePath, 0, $true)
        $mainSheet = $candidateBook.Worksheets.Item($planInfo.books.$kind.sheet)
        $scratch = $candidateBook.Worksheets.Add()
        $lastColumn = if ($kind -eq 'report') {14} else {13}
        for ($column = 13; $column -le $lastColumn; $column++) {
            [void]$scratch.Cells.Clear()
            $letter = [string][char](64 + $column)
            [void]$mainSheet.Range($letter + '107:' + $letter + '110').Copy($scratch.Range('A107'))
            $scratch.Columns.Item(1).ColumnWidth = $mainSheet.Columns.Item($column).ColumnWidth
            [void]$scratch.Range('A107:A110').Rows.AutoFit()
            for ($row = 107; $row -le 110; $row++) {
                $sourceCell = $mainSheet.Cells.Item($row, $column)
                $checks += [pscustomobject]@{
                    book=$kind; cell=$letter+$row; readOnly=$candidateBook.ReadOnly
                    needed=[double]$scratch.Rows.Item($row).RowHeight
                    actual=[double]$sourceCell.MergeArea.Height
                    isFormula=[bool]$sourceCell.HasFormula
                    text=[string]$sourceCell.Value2
                }
            }
        }
        [void]$scratch.Delete()
        $candidateBook.Close($false)
        [void][Runtime.InteropServices.Marshal]::ReleaseComObject($candidateBook)
        $candidateBook = $null
    }
    $checks | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $qaPath 'native-checks.json')
    $short = @($checks | Where-Object {$_.needed -gt $_.actual+1})
    if ($short.Count -gt 0) {throw ('내용 높이 부족: ' + ($short.cell -join ', '))}
    if (@($checks | Where-Object {$_.isFormula}).Count -gt 0) {throw '텍스트가 수식으로 저장됨'}
    [pscustomobject]@{cells=$checks.Count; heightShortage=$short.Count; formulas=0; maxNeeded=($checks.needed | Measure-Object -Maximum).Maximum} | ConvertTo-Json
} finally {
    if ($null -ne $candidateBook) {$candidateBook.Close($false); [void][Runtime.InteropServices.Marshal]::ReleaseComObject($candidateBook)}
    if ($null -ne $excelInstance) {$excelInstance.Quit(); [void][Runtime.InteropServices.Marshal]::ReleaseComObject($excelInstance)}
}
