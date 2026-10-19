param([ValidateSet('system','criterion')][string]$Kind = 'system', [string]$QaName = 'system-readable-rev0')
$ErrorActionPreference = 'Stop'
$qaPath = Join-Path $PSScriptRoot ('qa/'+$QaName)
$planInfo = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qaPath 'plan.json') | ConvertFrom-Json
$candidatePath = Join-Path $qaPath ($Kind + '-candidate.xlsx')
$excelInstance = $null
$candidateBook = $null
$cachedMeasurements = @{}
$cachedText = @{}
$editedCells = @{}
foreach ($entry in $planInfo.changes) {$editedCells[$entry.sheet+'|'+$entry.cell] = $true}
$previousCheckPath = Join-Path $qaPath ($Kind+'-native-check.json')
$previousPlanPath = Join-Path $qaPath ($Kind+'-native-plan.json')
if (-not (Test-Path -LiteralPath $previousPlanPath) -and $Kind -eq 'system') {$previousPlanPath = Join-Path $qaPath 'native-plan-round1.json'}
if ((Test-Path -LiteralPath $previousCheckPath) -and (Test-Path -LiteralPath $previousPlanPath)) {
    $previousCheck = Get-Content -Raw -Encoding UTF8 -LiteralPath $previousCheckPath | ConvertFrom-Json
    $previousPlan = Get-Content -Raw -Encoding UTF8 -LiteralPath $previousPlanPath | ConvertFrom-Json
    foreach ($item in $previousCheck.measurements) {$cachedMeasurements[$item.sheet+'|'+$item.cell] = $item}
    $previousItems = if ($previousPlan.measure) {$previousPlan.measure} else {$previousPlan.changes}
    foreach ($item in $previousItems) {
        $name = if ($Kind -eq 'criterion' -and $item.sheet -eq '보고서_시스템 시험') {'기준서_시스템 시험'} else {$item.sheet}
        $cachedText[$name+'|'+$item.cell] = [string]$item.after
    }
}
try {
    # 사용자 Excel과 연결하지 않고 별도 숨김 인스턴스에서 후보만 읽는다.
    $excelInstance = New-Object -ComObject Excel.Application
    $excelInstance.Visible = $false
    $excelInstance.DisplayAlerts = $false
    $excelInstance.AskToUpdateLinks = $false
    $excelInstance.AutomationSecurity = 3
    $candidateBook = $excelInstance.Workbooks.Open($candidatePath, 0, $true)
    $scratch = $candidateBook.Worksheets.Add()
    $measurements = @()
    $markers = @()
    $checkItems = if ($planInfo.measure) {$planInfo.measure} else {$planInfo.changes}
    foreach ($change in $checkItems) {
        if ($Kind -eq 'criterion' -and ($change.sheet -notin @('보고서_시스템 시험','사이버보안 시험 연계') -or $change.cell -match '^[KL]')) {continue}
        $sheetName = if ($Kind -eq 'criterion' -and $change.sheet -eq '보고서_시스템 시험') {'기준서_시스템 시험'} else {$change.sheet}
        $sheet = $candidateBook.Worksheets.Item($sheetName)
        $cell = $sheet.Range($change.cell)
        $textMatches = [string]$cell.Value2 -ceq [string]$change.after
        # 미편집 원본의 XML에 xml:space가 없으면 Excel은 가장자리 공백을 생략한다.
        # 새 편집 셀은 정확히 대조하고, 원본 셀은 이러한 표시 차이만 허용한다.
        if (-not $editedCells.ContainsKey($change.sheet+'|'+$change.cell)) {
            $textMatches = ([string]$cell.Value2).Trim() -ceq ([string]$change.after).Trim()
        }
        if (-not $textMatches) {
            $detail = @{actual=[string]$cell.Value2;expected=[string]$change.after} | ConvertTo-Json -Compress
            throw "Text differs: $sheetName / $($change.cell) / $detail"
        }
        $area = $cell.MergeArea
        $key = $sheetName+'|'+$change.cell
        # 후보는 원본 글꼴/열 폭을 보존한다. 같은 문구의 필요 높이는 재사용하고
        # 최종 후보의 실제 병합 높이/문구/안내 색상은 다시 읽는다.
        $equipmentChanged = $sheetName -in @('보고서_시스템 시험','기준서_시스템 시험') -and $change.cell -in @('C18','D18')
        if (-not $equipmentChanged -and $cachedMeasurements.ContainsKey($key) -and $cachedText[$key] -ceq [string]$change.after) {
            $neededHeight = [double]$cachedMeasurements[$key].needed
        } else {
            [void]$scratch.Cells.Clear()
            [void]$cell.Copy($scratch.Range('A1'))
            [void]$scratch.UsedRange.UnMerge()
            $scratch.Columns.Item(1).ColumnWidth = [double]$sheet.Columns.Item($cell.Column).ColumnWidth
            # 가로 병합 셀은 합쳐진 실제 표시 폭으로 측정한다.
            for ($iteration=0; $iteration -lt 3; $iteration++) {
                $wantedWidth = [double]$area.Width
                $actualWidth = [double]$scratch.Columns.Item(1).Width
                $columnUnits = [double]$scratch.Columns.Item(1).ColumnWidth
                $newWidth = [Math]::Min([double]255, [double]($columnUnits * $wantedWidth / $actualWidth))
                $scratch.Columns.Item(1).ColumnWidth = [double]$newWidth
            }
            [void]$scratch.Rows.Item(1).AutoFit()
            $neededHeight = [double]$scratch.Rows.Item(1).RowHeight
        }
        $measurements += [pscustomobject]@{sheet=$sheetName;cell=$change.cell;needed=$neededHeight;current=[double]$area.Height;rowHeight=[double]$cell.EntireRow.RowHeight}
        if ($measurements.Count % 200 -eq 0) {Write-Output "$Kind checked $($measurements.Count) cells"}
        foreach ($marker in @('(사진필요)','(QCC 검증 결과 필요)')) {
            $at = ([string]$cell.Value2).IndexOf($marker)
            if ($at -ge 0) {
                $font = $cell.Characters($at+1,$marker.Length).Font
                $expectedColor = if ($marker -eq '(사진필요)') {16711680} else {192}
                if ($font.Color -ne $expectedColor -or -not $font.Bold) {throw "Marker color/bold differs: $sheetName / $($change.cell)"}
                $markers += [pscustomobject]@{sheet=$sheetName;cell=$change.cell;bold=$font.Bold;color=$font.Color}
            }
        }
    }
    [void]$scratch.Delete()
    $output = [pscustomobject]@{kind=$Kind;readOnly=$candidateBook.ReadOnly;measurements=$measurements;markers=$markers;hash=(Get-FileHash -LiteralPath $candidatePath -Algorithm SHA256).Hash.ToLowerInvariant()}
    $output | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $qaPath ($Kind+'-native-check.json'))
    $planInfo | ConvertTo-Json -Depth 10 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $qaPath ($Kind+'-native-plan.json'))
    [pscustomobject]@{kind=$Kind;cells=$measurements.Count;undersized=@($measurements | Where-Object {$_.needed -gt $_.current+1}).Count;maximumRequired=($measurements | Measure-Object -Property needed -Maximum).Maximum} | ConvertTo-Json
} finally {
    if ($null -ne $candidateBook) {$candidateBook.Close($false); [void][Runtime.InteropServices.Marshal]::ReleaseComObject($candidateBook)}
    if ($null -ne $excelInstance) {$excelInstance.Quit(); [void][Runtime.InteropServices.Marshal]::ReleaseComObject($excelInstance)}
}
