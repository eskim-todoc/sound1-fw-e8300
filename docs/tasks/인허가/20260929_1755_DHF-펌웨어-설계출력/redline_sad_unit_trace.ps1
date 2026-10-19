$ErrorActionPreference = 'Stop'
$qa = Join-Path $PSScriptRoot 'qa\sad-unit-trace-20261012'
$app = New-Object -ComObject Word.Application
$app.Visible = $false
$app.DisplayAlerts = 0
$app.AutomationSecurity = 3
$app.ScreenUpdating = $false
$oldUserName = $app.UserName
$oldUserInitials = $app.UserInitials
$doc = $null
try {
    $app.UserName = '김은수'
    $app.UserInitials = '은수'
    $doc = $app.Documents.Open((Join-Path $qa 'before.docx'), $false, $true, $false)
    $doc.TrackRevisions = $true
    $doc.TrackFormatting = $true
    # 81개 요구사항이 들어 있는 기존 추적성 표만 교체한다.
    $found = @()
    for ($i = 1; $i -le $doc.Tables.Count; $i++) {
        $t = $doc.Tables.Item($i)
        if ($t.Range.Text.Contains('SW-FW-RID-1') -and $t.Range.Text.Contains('SW-FW-RID-81')) {
            $found += $i
        }
    }
    if ($found.Count -ne 1) { throw "Trace table matches: $($found.Count)" }
    $oldTable = $doc.Tables.Item($found[0])
    # 삭제 추적 중인 첫 셀 안에 새 표가 중첩되지 않도록, 기존 표 다음의
    # 빈 본문 문단에 삽입한 뒤 기존 표를 삭제한다.
    $anchor = $doc.Range($oldTable.Range.End, $oldTable.Range.End + 1)
    if ($anchor.Information(12)) { throw 'Insertion anchor is inside a table' }
    $anchor.InsertXML((Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qa 'trace-fragment.xml')))
    $oldTable.Delete()
    $doc.SaveAs2((Join-Path $qa 'candidate.docx'), 16)
    Write-Output ('candidate revisions=' + $doc.Revisions.Count + ' tables=' + $doc.Tables.Count)
    $doc.Close(0)
    $doc = $null
} finally {
    if ($null -ne $doc) { $doc.Close(0) }
    $app.UserName = $oldUserName
    $app.UserInitials = $oldUserInitials
    $app.Quit(0)
    $null = [Runtime.InteropServices.Marshal]::ReleaseComObject($app)
}
