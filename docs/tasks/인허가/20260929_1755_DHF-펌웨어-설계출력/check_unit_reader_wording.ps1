param([string]$QaName = 'unit-reader-wording')
$ErrorActionPreference = 'Stop'
$taskQa = Join-Path $PSScriptRoot ('qa/' + $QaName)
$taskPlan = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $taskQa 'plan.json') | ConvertFrom-Json
$taskExcel = $null; $taskBook = $null
try {
    # 사용자 Excel과 분리된 숨김 인스턴스에서 검사하고 저장하지 않는다.
    $taskExcel = New-Object -ComObject Excel.Application
    $taskExcel.Visible = $false; $taskExcel.DisplayAlerts = $false
    $taskExcel.AskToUpdateLinks = $false; $taskExcel.AutomationSecurity = 3
    $taskBook = $taskExcel.Workbooks.Open((Join-Path $taskQa 'unit-candidate.xlsx'), 0, $true)
    $taskProbeSheet = $taskBook.Worksheets.Add()
    $taskProbe = $taskProbeSheet.Range('A1')
    $taskProbe.WrapText = $true
    $taskSamples = @($taskPlan.changes | Where-Object {
        ($QaName -in @('unit-first-edition','unit-first-results') -and $_.sheet -eq '보고서_유닛 시험') -or
        ($_.sheet -eq '보고서_유닛 시험' -and ($_.cell -match '^G\d+$' -or $_.reason -match '검증 번호와 RTT 연결')) -or
        ($_.sheet -eq 'RTT 결과 대조' -and $_.cell -match '^B\d+$')
    })
    $taskResults = @()
    foreach ($taskItem in $taskSamples) {
        $taskSource = $taskBook.Worksheets.Item($taskItem.sheet).Range($taskItem.cell)
        $taskValue = ([string]$taskSource.Value2).Replace("`r`n", "`n")
        if ($taskValue -cne $taskItem.value) { throw ('Saved text differs: ' + $taskItem.cell) }
        $taskStart = [Math]::Max(1,$taskValue.Length-$taskValue.TrimStart().Length+1)
        $taskProbe.Font.Name = $taskSource.Characters($taskStart,1).Font.Name
        $taskProbe.Font.Size = $taskSource.Characters($taskStart,1).Font.Size
        $taskProbe.Font.Bold = $taskSource.Characters($taskStart,1).Font.Bold
        $taskProbeSheet.Columns.Item(1).ColumnWidth = $taskSource.ColumnWidth
        if ($taskSource.MergeCells -and $taskSource.MergeArea.Columns.Count -gt 1) {
            # 가로 병합 안내 칸은 첫 열이 아닌 실제 병합 폭으로 측정한다.
            $taskRatio = [double]$taskSource.MergeArea.Width / [double]$taskProbe.Width
            $taskMergedWidth = [double][Math]::Min([double]255.0, [double]$taskProbe.ColumnWidth * $taskRatio)
            $taskProbeSheet.Columns.Item(1).ColumnWidth = $taskMergedWidth
        }
        $taskLines = $taskValue -split "`n"
        $taskNeeded = 0.0
        for ($taskIndex=0; $taskIndex -lt $taskLines.Count; $taskIndex+=8) {
            $taskEnd = [Math]::Min($taskIndex+7,$taskLines.Count-1)
            $taskProbe.Value2 = ($taskLines[$taskIndex..$taskEnd] -join "`n")
            [void]$taskProbe.EntireRow.AutoFit()
            $taskNeeded += [double]$taskProbe.RowHeight
        }
        $taskAvailable = if ($taskSource.MergeCells) { [double]$taskSource.MergeArea.Height } else { [double]$taskSource.RowHeight }
        $taskResults += [pscustomobject]@{sheet=$taskItem.sheet;cell=$taskItem.cell;needed=$taskNeeded;available=$taskAvailable;fits=($taskNeeded -le $taskAvailable+1)}
    }
    $taskResult = [pscustomobject]@{readOnly=$taskBook.ReadOnly;checked=$taskResults.Count;insufficient=@($taskResults | Where-Object {-not $_.fits});checks=$taskResults}
    $taskResult | ConvertTo-Json -Depth 7 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $taskQa 'native-checks.json')
    [pscustomobject]@{checked=$taskResults.Count;insufficient=$taskResult.insufficient.Count} | ConvertTo-Json
} finally {
    if ($null -ne $taskBook) { $taskBook.Close($false); [void][Runtime.InteropServices.Marshal]::ReleaseComObject($taskBook) }
    if ($null -ne $taskExcel) { $taskExcel.Quit(); [void][Runtime.InteropServices.Marshal]::ReleaseComObject($taskExcel) }
}
