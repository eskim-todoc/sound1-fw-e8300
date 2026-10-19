param([ValidateSet('system','criterion')][string]$Kind = 'system')
$ErrorActionPreference = 'Stop'
$qaPath = Join-Path $PSScriptRoot 'qa/system-advertisement-gate'
$planInfo = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qaPath 'plan.json') | ConvertFrom-Json
$candidatePath = Join-Path $qaPath ($Kind + '-candidate.xlsx')
$excelInstance = $null
$candidateBook = $null
try {
    # 사용자 Excel과 연결하지 않는다. 후보만 별도 숨김/읽기 전용으로 연다.
    $excelInstance = New-Object -ComObject Excel.Application
    $excelInstance.Visible = $false
    $excelInstance.DisplayAlerts = $false
    $excelInstance.AskToUpdateLinks = $false
    $excelInstance.AutomationSecurity = 3
    $candidateBook = $excelInstance.Workbooks.Open($candidatePath, 0, $true)
    $scratch = $candidateBook.Worksheets.Add()
    $measurements = @()
    $markers = @()
    foreach ($change in $planInfo.changes) {
        if ($Kind -eq 'criterion' -and ($change.sheet -ne '보고서_시스템 시험' -or $change.cell -match '^[KL]')) {continue}
        $sheetName = if ($Kind -eq 'criterion') {'기준서_시스템 시험'} else {$change.sheet}
        $sheet = $candidateBook.Worksheets.Item($sheetName)
        $cell = $sheet.Range($change.cell)
        if ([string]$cell.Value2 -ne [string]$change.after) {throw "Text differs: $sheetName / $($change.cell)"}
        $area = $cell.MergeArea
        [void]$scratch.Cells.Clear()
        [void]$cell.Copy($scratch.Range('A1'))
        [void]$scratch.UsedRange.UnMerge()
        $scratch.Columns.Item(1).ColumnWidth = $sheet.Columns.Item($cell.Column).ColumnWidth
        [void]$scratch.Rows.Item(1).AutoFit()
        $measurements += [pscustomobject]@{sheet=$sheetName;cell=$change.cell;needed=[double]$scratch.Rows.Item(1).RowHeight;current=[double]$area.Height}
        $marker = '(사진필요)'
        $at = ([string]$cell.Value2).IndexOf($marker)
        if ($at -ge 0) {
            $font = $cell.Characters($at+1,$marker.Length).Font
            if ($font.Color -ne 16711680 -or -not $font.Bold) {throw "Blue/bold photo marker differs: $($change.cell)"}
            $markers += [pscustomobject]@{sheet=$sheetName;cell=$change.cell;bold=$font.Bold;color=$font.Color}
        }
    }
    [void]$scratch.Delete()
    $output = [pscustomobject]@{kind=$Kind;readOnly=$candidateBook.ReadOnly;measurements=$measurements;markers=$markers;hash=(Get-FileHash -LiteralPath $candidatePath -Algorithm SHA256).Hash.ToLowerInvariant()}
    $output | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $qaPath ($Kind+'-native-check.json'))
    [pscustomobject]@{kind=$Kind;cells=$measurements.Count;undersized=@($measurements | Where-Object {$_.needed -gt $_.current+1}).Count;maximumRequired=($measurements | Measure-Object -Property needed -Maximum).Maximum} | ConvertTo-Json
} finally {
    if ($null -ne $candidateBook) {$candidateBook.Close($false); [void][Runtime.InteropServices.Marshal]::ReleaseComObject($candidateBook)}
    if ($null -ne $excelInstance) {$excelInstance.Quit(); [void][Runtime.InteropServices.Marshal]::ReleaseComObject($excelInstance)}
}
