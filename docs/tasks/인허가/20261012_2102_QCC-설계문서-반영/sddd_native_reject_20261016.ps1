$ErrorActionPreference = 'Stop'
$qa = Join-Path $PSScriptRoot 'qa/sddd-qcc-cleanup-20261016'
$file = Join-Path $qa 'candidate.docx'
$hash = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash
$word = $null
$doc = $null
try {
    $word = New-Object -ComObject Word.Application
    $word.Visible = $false
    $word.DisplayAlerts = 0
    $word.ScreenUpdating = $false
    $word.AutomationSecurity = 3
    $doc = $word.Documents.Open($file,$false,$true,$false)
    $doc.TrackRevisions = $false
    $originalCount = $doc.Revisions.Count
    $rejected = 0
    for ($i=$originalCount; $i -ge 1; $i--) {
        $rev = $doc.Revisions.Item($i)
        if ($rev.Author -eq '김은수' -and ([datetime]$rev.Date).Date -eq [datetime]'2026-10-16') {
            $rev.Reject()
            $rejected++
        }
        [void][System.Runtime.InteropServices.Marshal]::FinalReleaseComObject($rev)
    }
    $remaining = $doc.Revisions.Count
    Write-Output "Native Word rejected $rejected new revisions; $remaining earlier revisions remain."
    if ($remaining -ne 1800 -or $rejected -ne ($originalCount-1800)) { throw 'Unexpected native revision count' }
    $doc.Revisions.AcceptAll()
    $doc.Repaginate()
    $text = $doc.Content.Text
    $text | ConvertTo-Json | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $qa 'native-reject-word-text.json')
    $pages = $doc.ComputeStatistics(2)
    $doc.ExportAsFixedFormat((Join-Path $qa 'native-reject.pdf'),17)
    $doc.Close(0)
    [void][System.Runtime.InteropServices.Marshal]::FinalReleaseComObject($doc)
    $doc = $null
    $beforeText = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qa 'before-word-text.json') | ConvertFrom-Json
    if ($text -cne $beforeText) { throw 'Native rejection text differs from baseline' }
    if ((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ne $hash) { throw 'Candidate changed' }
    [pscustomobject]@{rejected_new=$rejected;earlier_revisions_remaining=$remaining;text_restored=$true;candidate_unchanged=$true;pages=$pages;hash=$hash} |
        ConvertTo-Json | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $qa 'native-reject.json')
    Write-Output "Native rejection verified: $pages pages; original text restored."
} finally {
    if ($null -ne $doc) { $doc.Close(0); [void][System.Runtime.InteropServices.Marshal]::FinalReleaseComObject($doc) }
    if ($null -ne $word) { $word.Quit(0); [void][System.Runtime.InteropServices.Marshal]::FinalReleaseComObject($word) }
    [GC]::Collect()
    [GC]::WaitForPendingFinalizers()
}
