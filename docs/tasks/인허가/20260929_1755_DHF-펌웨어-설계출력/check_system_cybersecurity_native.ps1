param([string]$Kind = 'system')
$ErrorActionPreference = 'Stop'
$qaPath = Join-Path $PSScriptRoot 'qa/system-cybersecurity-rev0'
$planInfo = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qaPath 'author-plan.json') | ConvertFrom-Json
$candidatePath = Join-Path $qaPath ($Kind + '-candidate.xlsx')
$excelInstance = $null
$candidateBook = $null
try {
    # 사용자 Excel에 연결하지 않고 새 숨김 인스턴스로 후보만 읽는다.
    $excelInstance = New-Object -ComObject Excel.Application
    $excelInstance.Visible = $false
    $excelInstance.DisplayAlerts = $false
    $excelInstance.AskToUpdateLinks = $false
    $excelInstance.AutomationSecurity = 3
    $candidateBook = $excelInstance.Workbooks.Open($candidatePath, 0, $true)
    $scratch = $candidateBook.Worksheets.Add()
    $measurements = @()
    $markerChecks = @()
    $mainName = if ($Kind -eq 'system') {'보고서_시스템 시험'} else {'기준서_시스템 시험'}
    $mainSheet = $candidateBook.Worksheets.Item($mainName)
    if ($Kind -eq 'system') {
        for ($r=34; $r -le 281; $r++) {
            $cell=$mainSheet.Cells.Item($r,11)
            $value=[string]$cell.Value2
            foreach ($marker in @('(사진필요)','(QCC 검증 결과 필요)')) {
                $at=$value.IndexOf($marker)
                if ($at -ge 0) {
                    $part=$cell.Characters($at+1,$marker.Length).Font
                    $expectedColor=if($marker -eq '(사진필요)'){16711680}else{192}
                    if ($part.Color -ne $expectedColor -or -not $part.Bold) {throw "Marker font mismatch: K$r / $marker"}
                    $markerChecks += [pscustomobject]@{cell='K'+$r;marker=$marker;color=$part.Color;bold=$part.Bold}
                }
            }
        }
    }
    foreach ($change in $planInfo.changes) {
        if ($Kind -eq 'criterion' -and $change.cell.StartsWith('K')) {continue}
        $cell = $mainSheet.Range($change.cell)
        $area = $cell.MergeArea
        [void]$scratch.Cells.Clear()
        [void]$cell.Copy($scratch.Range('A1'))
        [void]$scratch.UsedRange.UnMerge()
        $scratch.Columns.Item(1).ColumnWidth = $mainSheet.Columns.Item($cell.Column).ColumnWidth
        [void]$scratch.Rows.Item(1).AutoFit()
        $measurements += [pscustomobject]@{sheet=$mainName;cell=$change.cell;row=[int]$cell.Row;needed=[double]$scratch.Rows.Item(1).RowHeight;current=[double]$area.Height}
    }
    $trace = $candidateBook.Worksheets.Item('사이버보안 시험 연계')
    if ($Kind -eq 'system') {
        for($r=5;$r -le 39;$r++) {
            $cell=$trace.Cells.Item($r,11)
            $traceMarker='(QCC 검증 결과 필요)'
            $at=([string]$cell.Value2).IndexOf($traceMarker)
            if($at -ge 0) {
                $font=$cell.Characters($at+1,$traceMarker.Length).Font
                if($font.Color -ne 192 -or -not $font.Bold){throw "Trace marker font mismatch: K$r"}
                $markerChecks += [pscustomobject]@{sheet=$trace.Name;cell='K'+$r;marker='(QCC 검증 결과 필요)';color=$font.Color;bold=$font.Bold}
            }
        }
    }
    $endColumn = if ($Kind -eq 'system') {12} else {10}
    for ($column=1; $column -le $endColumn; $column++) {
        $letter = [string][char](64+$column)
        [void]$scratch.Cells.Clear()
        [void]$trace.Range($letter+'5:'+$letter+'39').Copy($scratch.Range('A5'))
        $scratch.Columns.Item(1).ColumnWidth = $trace.Columns.Item($column).ColumnWidth
        [void]$scratch.Range('A5:A39').Rows.AutoFit()
        for ($row=5; $row -le 39; $row++) {
            $measurements += [pscustomobject]@{sheet=$trace.Name;cell=$letter+$row;row=$row;needed=[double]$scratch.Rows.Item($row).RowHeight;current=[double]$trace.Rows.Item($row).RowHeight}
        }
    }
    [void]$scratch.Delete()
    $output = [pscustomobject]@{kind=$Kind;readOnly=$candidateBook.ReadOnly;measurements=$measurements;markerChecks=$markerChecks;hash=(Get-FileHash -LiteralPath $candidatePath -Algorithm SHA256).Hash.ToLowerInvariant()}
    $output | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $qaPath ($Kind+'-native-heights.json'))
    [pscustomobject]@{kind=$Kind;cells=$measurements.Count;undersized=@($measurements | Where-Object {$_.needed -gt $_.current+1}).Count;maximumRequired=($measurements | Measure-Object -Property needed -Maximum).Maximum} | ConvertTo-Json
} finally {
    if ($null -ne $candidateBook) {$candidateBook.Close($false); [void][Runtime.InteropServices.Marshal]::ReleaseComObject($candidateBook)}
    if ($null -ne $excelInstance) {$excelInstance.Quit(); [void][Runtime.InteropServices.Marshal]::ReleaseComObject($excelInstance)}
}
