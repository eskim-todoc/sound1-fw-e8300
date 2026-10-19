param([string[]]$Stems = @('before','candidate','reject-new'))
$ErrorActionPreference = 'Stop'
$qa = Join-Path $PSScriptRoot 'qa/sddd-qcc-cleanup-20261016'
$word = $null
$doc = $null
$results = [System.Collections.Generic.List[object]]::new()
try {
    $word = New-Object -ComObject Word.Application
    $word.Visible = $false
    $word.DisplayAlerts = 0
    $word.ScreenUpdating = $false
    $word.AutomationSecurity = 3
    Write-Output "Created independent hidden Word HWND=$($word.Hwnd)"
    foreach ($stem in $Stems) {
        $file = Join-Path $qa ($stem + '.docx')
        $pdf = Join-Path $qa ($stem + '.pdf')
        Write-Output "Opening read-only: $stem"
        $hashBefore = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash
        $doc = $word.Documents.Open($file, $false, $true, $false)
        $revisionCount = $doc.Revisions.Count
        $doc.TrackRevisions = $false
        $doc.Revisions.AcceptAll()
        $doc.Repaginate()
        $pages = $doc.ComputeStatistics(2)
        $text = $doc.Content.Text
        $text | ConvertTo-Json -Depth 2 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $qa ($stem + '-word-text.json'))
        $doc.ExportAsFixedFormat($pdf,17)
        $doc.Close(0)
        [void][System.Runtime.InteropServices.Marshal]::FinalReleaseComObject($doc)
        $doc = $null
        $hashAfter = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash
        if ($hashBefore -ne $hashAfter) { throw "Read-only source was modified: $stem" }
        $results.Add([pscustomobject]@{stem=$stem;revisions_before=$revisionCount;pages=$pages;hash=$hashAfter;source_unchanged=$true;pdf=$pdf})
        Write-Output "Exported $stem, pages=$pages, revisions=$revisionCount"
    }
    $results | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $qa 'word-render.json')
} finally {
    if ($null -ne $doc) { $doc.Close(0); [void][System.Runtime.InteropServices.Marshal]::FinalReleaseComObject($doc) }
    if ($null -ne $word) { $word.Quit(0); [void][System.Runtime.InteropServices.Marshal]::FinalReleaseComObject($word) }
    [GC]::Collect()
    [GC]::WaitForPendingFinalizers()
}
