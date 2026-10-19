param([string]$ProbeRoot = $PSScriptRoot)
# Run only by the root agent's serialized native Word worker. No G writes.
$ErrorActionPreference = 'Stop'
$qa = Join-Path $ProbeRoot 'native-trace-probe'
$expectedPath = Join-Path $qa 'expected-cells.json'
$expected = Get-Content -Raw -Encoding UTF8 -LiteralPath $expectedPath | ConvertFrom-Json
$candidate = Join-Path $qa 'SAD-native-probe-author.docx'
$recordPath = Join-Path $qa 'native-cell-proof.json'
$word = $null
$doc = $null
$oldUser = $null
$oldInitials = $null
$result = [ordered]@{status='FAIL'; candidate=$candidate; source=$expected.source; beforeSha256=$expected.source_sha256; deletion_count=0; operations=@()}

function Get-Compact([string]$Text) {
    return [regex]::Replace($Text,'[\s\x07]','')
}
function Get-TraceCells($Document) {
    # Called after AcceptAllRevisions: this checks real Word table geometry,
    # avoiding the former OOXML-only projection that missed removed cells.
    $found = @()
    foreach ($table in $Document.Tables) {
        if ($table.Rows.Count -ne 83) { continue }
        $rows = @()
        for ($ri=1; $ri -le $table.Rows.Count; $ri++) {
            $row = $table.Rows.Item($ri)
            $cells = @()
            foreach ($cell in $row.Cells) { $cells += [string]$cell.Range.Text }
            $rows += [pscustomobject]@{cells=$cells}
        }
        if ($rows[1].cells.Count -eq 3 -and (Get-Compact $rows[1].cells[0]) -eq 'SW-FW-RID-1' -and (Get-Compact $rows[82].cells[0]) -eq 'SW-FW-RID-83') {
            $found += [pscustomobject]@{rows=$rows; row_count=$table.Rows.Count; column_count=$table.Columns.Count}
        }
    }
    if ($found.Count -ne 1) { throw "Expected one native 83-row requirement table; found $($found.Count)" }
    return $found[0]
}
function Assert-TraceCells($Actual, [string]$Mode) {
    if ($Actual.rows.Count -ne $expected.rows.Count) { throw "$Mode requirement row count differs" }
    for ($ri=0; $ri -lt $expected.rows.Count; $ri++) {
        if ($Actual.rows[$ri].cells.Count -ne 3) { throw "$Mode row $ri has $($Actual.rows[$ri].cells.Count) cells" }
        for ($ci=0; $ci -lt 3; $ci++) {
            if ((Get-Compact $Actual.rows[$ri].cells[$ci]) -cne (Get-Compact $expected.rows[$ri].cells[$ci])) {
                throw "$Mode cell mismatch row=$ri column=$ci ID=$($expected.rows[$ri].id)"
            }
        }
    }
}
function Close-ProbeDocument {
    if ($script:doc) {
        $script:doc.Close(0)
        [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($script:doc)
        $script:doc = $null
    }
}

try {
    if ((Get-FileHash -Algorithm SHA256 -LiteralPath $expected.source).Hash -ne $expected.source_sha256) { throw 'Exact 8AF recovery source changed' }
    if ((Get-FileHash -Algorithm SHA256 -LiteralPath $expected.prepared).Hash -ne $expected.prepared_sha256) { throw 'Prepared bookmark source changed' }
    Copy-Item -LiteralPath $expected.prepared -Destination $candidate -Force
    $word = New-Object -ComObject Word.Application
    $word.Visible = $false
    $word.DisplayAlerts = 0
    $oldUser = $word.UserName
    $oldInitials = $word.UserInitials
    # Distinct QA identity prevents Word from cancelling historical insertions
    # made by 김은수. Only new native-generated revisions are renamed after the
    # independent native accept/selective-reject proof passes.
    $word.UserName = [string]$expected.probe_author
    $word.UserInitials = 'QA'

    $doc = $word.Documents.Open($expected.source,$false,$true,$false)
    $doc.AcceptAllRevisions()
    $result.baseline = Get-TraceCells $doc
    Assert-TraceCells $result.baseline 'baseline'
    Close-ProbeDocument

    $doc = $word.Documents.Open($candidate,$false,$false,$false)
    $doc.TrackRevisions = $true
    $positions = @()
    foreach ($target in $expected.targets) {
        if (-not $doc.Bookmarks.Exists($target.bookmark)) { throw "Missing QA bookmark $($target.bookmark)" }
        $positions += [pscustomobject]@{target=$target; start=$doc.Bookmarks.Item($target.bookmark).Range.Start}
    }
    foreach ($item in ($positions | Sort-Object start -Descending)) {
        $target = $item.target
        if (-not $doc.Bookmarks.Exists($target.bookmark)) { throw "QA bookmark lost before deletion $($target.bookmark)" }
        $bookmarkRange = $doc.Bookmarks.Item($target.bookmark).Range.Duplicate
        $paragraphRange = $bookmarkRange.Paragraphs.Item(1).Range.Duplicate
        $raw = [string]$paragraphRange.Text
        $terminal = $raw.EndsWith("`r$([char]7)")
        if ($terminal) {
            # A terminal Word table paragraph contains cell end \r\a. Preserve
            # it; remove ONLY the preceding paragraph separator instead.
            $start = $paragraphRange.Start - 1
            $end = $paragraphRange.Start
        } else {
            $start = $paragraphRange.End - 1
            $end = $paragraphRange.End
        }
        if ($start -lt 0 -or $end -ne $start+1) { throw "Invalid deletion boundary $($target.bookmark)" }
        $deleteRange = $doc.Range($start,$end)
        if ([string]$deleteRange.Text -cne "`r") {
            throw "Refusing non-paragraph-mark deletion $($target.bookmark): char=$([int][char]$deleteRange.Text[0])"
        }
        # No Range ending in the table end marker is ever passed to Delete.
        $result.operations += [pscustomobject]@{bookmark=$target.bookmark; paragraph_id=$target.paragraph_id; requirement=$target.requirement; column=$target.column; start=$start; end=$end; cell_end_guard=$terminal; deleted_character=13}
        [void]$deleteRange.Delete()
        $result.deletion_count++
    }
    if ($result.deletion_count -ne 183) { throw '183 native deletion operations expected' }
    $doc.Save()
    Close-ProbeDocument
    $result.candidateSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $candidate).Hash

    $doc = $word.Documents.Open($candidate,$false,$true,$false)
    $doc.AcceptAllRevisions()
    $result.accept = Get-TraceCells $doc
    Assert-TraceCells $result.accept 'accept'
    Close-ProbeDocument

    $doc = $word.Documents.Open($candidate,$false,$true,$false)
    $rejected = 0
    for ($i=$doc.Revisions.Count; $i -ge 1; $i--) {
        $rev = $doc.Revisions.Item($i)
        # Historical revisions are left intact; only this unique probe author's
        # actual native deletion revisions are rejected in this check.
        if ([string]$rev.Author -eq [string]$expected.probe_author) {
            $rev.Reject()
            $rejected++
        }
    }
    if ($rejected -eq 0) { throw 'No new native revision found to reject' }
    $result.native_new_revisions_rejected = $rejected
    $doc.AcceptAllRevisions()
    $result.reject_new = Get-TraceCells $doc
    Assert-TraceCells $result.reject_new 'reject_new'
    Close-ProbeDocument
    if ((Get-FileHash -Algorithm SHA256 -LiteralPath $expected.source).Hash -ne $expected.source_sha256) { throw 'Source changed during native probe' }
    $result.status = 'PASS'
} catch {
    $result.error = $_.Exception.Message
    throw
} finally {
    try { Close-ProbeDocument } catch {}
    if ($word) {
        try { if($null -ne $oldUser){$word.UserName=$oldUser}; if($null -ne $oldInitials){$word.UserInitials=$oldInitials} } catch {}
        try { $word.Quit() } catch {}
        [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($word)
    }
    $result | ConvertTo-Json -Depth 12 | Set-Content -Encoding UTF8 -LiteralPath $recordPath
    [GC]::Collect(); [GC]::WaitForPendingFinalizers()
}
Write-Output "Native probe PASS. Evidence: $recordPath"
Write-Output 'Next: root runs bundled Python sad_native_trace_probe.py inspect native-cell-proof.json; no G publication until final guard.'
