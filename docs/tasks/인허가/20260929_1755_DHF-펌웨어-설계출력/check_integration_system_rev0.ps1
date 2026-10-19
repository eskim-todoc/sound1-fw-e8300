param([string]$Kind = 'integration')
$ErrorActionPreference = 'Stop'
$qaPath = Join-Path $PSScriptRoot 'qa/integration-system-rev0'
$planInfo = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qaPath ($Kind + '-plan.json')) | ConvertFrom-Json
$candidatePath = Join-Path $qaPath ($Kind + '-candidate.xlsx')
$excelInstance = $null
$candidateBook = $null
try {
    # 현재 사용 중인 Excel에는 연결하지 않는다. 후보는 읽기 전용으로 검사한다.
    $excelInstance = New-Object -ComObject Excel.Application
    $excelInstance.Visible = $false
    $excelInstance.DisplayAlerts = $false
    $excelInstance.AskToUpdateLinks = $false
    $excelInstance.AutomationSecurity = 3
    $candidateBook = $excelInstance.Workbooks.Open($candidatePath, 0, $true)
    $mainSheet = $candidateBook.Worksheets.Item($planInfo.sheet)
    $scratch = $candidateBook.Worksheets.Add()
    $results = @()
    $markerChecks = @()
    for ($column = 2; $column -le $planInfo.cols; $column++) {
        $columnName = [string][char](64 + $column)
        $rangeName = $columnName + $planInfo.first + ':' + $columnName + $planInfo.last
        [void]$scratch.Cells.Clear()
        [void]$mainSheet.Range($rangeName).Copy($scratch.Range('A' + $planInfo.first))
        [void]$scratch.UsedRange.UnMerge()
        $scratch.Columns.Item(1).ColumnWidth = $mainSheet.Columns.Item($column).ColumnWidth
        [void]$scratch.Range('A' + $planInfo.first + ':A' + $planInfo.last).Rows.AutoFit()
        for ($row = $planInfo.first; $row -le $planInfo.last; $row++) {
            $originCell = $mainSheet.Cells.Item($row, $column)
            if ([string]::IsNullOrEmpty([string]$originCell.Value2)) { continue }
            if ($Kind -eq 'system' -and $column -eq 11) {
                foreach ($marker in @('(사진필요)', '(QCC 검증 결과 필요)')) {
                    $at = ([string]$originCell.Value2).IndexOf($marker)
                    if ($at -ge 0) {
                        $fontPart = $originCell.Characters($at+1, $marker.Length).Font
                        $expectedColor = if ($marker -eq '(사진필요)') {16711680} else {192}
                        if ($fontPart.Color -ne $expectedColor -or -not $fontPart.Bold) { throw "Marker style mismatch: $columnName$row $marker at $at color $($fontPart.Color) expected $expectedColor bold $($fontPart.Bold)" }
                        $markerChecks += [pscustomobject]@{cell=$columnName+$row; marker=$marker; color=$fontPart.Color; bold=$fontPart.Bold}
                    }
                }
            }
            $area = $originCell.MergeArea
            $results += [pscustomobject]@{
                cell = $columnName + $row
                start = [int]$area.Row
                end = [int]$area.Row + [int]$area.Rows.Count - 1
                needed = [double]$scratch.Rows.Item($row).RowHeight
                current = [double]$area.Height
            }
        }
    }
    $helperResults = @()
    if ($Kind -eq 'system') {
        $helperSheet = $candidateBook.Worksheets.Item('시험 보조자료')
        for ($column = 1; $column -le 4; $column++) {
            $columnName = [string][char](64 + $column)
            [void]$scratch.Cells.Clear()
            [void]$helperSheet.Range($columnName + '5:' + $columnName + '298').Copy($scratch.Range('A5'))
            [void]$scratch.UsedRange.UnMerge()
            $scratch.Columns.Item(1).ColumnWidth = $helperSheet.Columns.Item($column).ColumnWidth
            [void]$scratch.Range('A5:A298').Rows.AutoFit()
            for ($row = 5; $row -le 298; $row++) {
                $originCell = $helperSheet.Cells.Item($row, $column)
                if ([string]::IsNullOrEmpty([string]$originCell.Value2)) { continue }
                $area = $originCell.MergeArea
                $helperResults += [pscustomobject]@{
                    cell=$columnName+$row; start=[int]$area.Row; end=[int]$area.Row+[int]$area.Rows.Count-1
                    needed=[double]$scratch.Rows.Item($row).RowHeight; current=[double]$area.Height
                }
            }
        }
    }
    [void]$scratch.Delete()
    $output = [pscustomobject]@{key=$Kind; readOnly=$candidateBook.ReadOnly; measurements=$results; helperMeasurements=$helperResults; markerChecks=$markerChecks; hash=(Get-FileHash -Algorithm SHA256 -LiteralPath $candidatePath).Hash.ToLowerInvariant()}
    $output | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $qaPath ($Kind + '-native-heights.json'))
    [pscustomobject]@{key=$Kind; measuredCells=$results.Count; undersized=@($results | Where-Object {$_.needed -gt $_.current + 1}).Count; helperUndersized=@($helperResults | Where-Object {$_.needed -gt $_.current + 1}).Count} | ConvertTo-Json
} finally {
    if ($null -ne $candidateBook) { $candidateBook.Close($false); [void][Runtime.InteropServices.Marshal]::ReleaseComObject($candidateBook) }
    if ($null -ne $excelInstance) { $excelInstance.Quit(); [void][Runtime.InteropServices.Marshal]::ReleaseComObject($excelInstance) }
}
