param([string]$ProbeRoot=$PSScriptRoot)
# Root runs this only after the prior native COM session is closed.
# Never publishes or mutates any G file. Native scratch is unsaved and closed.
$ErrorActionPreference='Stop'
$qa=Join-Path $ProbeRoot 'native-trace-probe-v2'
$helper=Join-Path $ProbeRoot 'sad_native_trace_probe_v2.py'
$python='C:\Users\EUNSU\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
$expected=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qa 'expected-cells.json') | ConvertFrom-Json
$candidate=Join-Path $qa 'SAD-native-probe-author.docx'
$progress=Join-Path $qa 'progress.txt'
$phasePath=Join-Path $qa 'phase.json'
$optionsPath=Join-Path $qa 'options-before.json'
$recordPath=Join-Path $qa 'native-cell-proof.json'
$word=$null; $doc=$null; $oldUser=$null; $oldInitials=$null; $oldLocal=$null; $oldScreen=$null
$stableCandidateSha=$null
$ownedWordPids=@()
$result=[ordered]@{status='FAIL'; candidate=$candidate; source=$expected.source; beforeSha256=$expected.source_sha256; deletion_count=0; operations=@(); selective_rejection_method='Microsoft Document.RejectAllRevisionsShown / RevisionsFilter.Reviewers.Visible'}

function Log([string]$Text) {
    $line="$(Get-Date -Format o) $Text"
    Write-Host $line
    Add-Content -Encoding UTF8 -LiteralPath $progress -Value $line
    [ordered]@{phase=$Text;at=(Get-Date -Format o);source=$expected.source;sourceSha256=$expected.source_sha256;candidate=$candidate;lastStableCandidateSha256=$script:stableCandidateSha;rootCreatedWordPIDCandidates=$script:ownedWordPids;optionsBefore=$optionsPath;GModified=$false} | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 -LiteralPath $phasePath
}
function Close-ProbeDocument {
    if($script:doc){$script:doc.Close(0);[void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($script:doc);$script:doc=$null}
}
function Native-XML([string]$Name,[bool]$Cells) {
    Log "${Name}: native Content.WordOpenXML single call"
    $xmlPath=Join-Path $qa "$Name.xml"
    [IO.File]::WriteAllText($xmlPath,[string]$script:doc.Content.WordOpenXML,[Text.UTF8Encoding]::new($false))
    $jsonPath=Join-Path $qa "$Name.json"
    $mode=if($Cells){'xml-cells'}else{'xml-history'}
    & $python $helper $mode $xmlPath $jsonPath | ForEach-Object { Write-Host $_ }
    if($LASTEXITCODE -ne 0){throw "$Name native XML / cells failed"}
    return (Get-Content -Raw -Encoding UTF8 -LiteralPath $jsonPath | ConvertFrom-Json)
}
function Signatures-Equal($Left,$Right) {
    $keys=@($Left.PSObject.Properties.Name)+@($Right.PSObject.Properties.Name) | Sort-Object -Unique
    foreach($key in $keys){if([string]$Left.$key -cne [string]$Right.$key){return $false}}
    return $true
}

try {
    if((Get-FileHash -Algorithm SHA256 -LiteralPath $expected.source).Hash -ne $expected.source_sha256){throw 'Exact 8AF source changed'}
    if((Get-FileHash -Algorithm SHA256 -LiteralPath $expected.prepared).Hash -ne $expected.prepared_sha256){throw 'Prepared v2 source changed'}
    Log 'start hidden Word, save options'
    $wordPidsBefore=@((Get-Process -Name WINWORD -ErrorAction SilentlyContinue).Id)
    $word=New-Object -ComObject Word.Application
    $ownedWordPids=@((Get-Process -Name WINWORD -ErrorAction SilentlyContinue).Id | Where-Object {$_ -notin $wordPidsBefore})
    $oldUser=$word.UserName; $oldInitials=$word.UserInitials; $oldLocal=$word.Options.UseLocalUserInfo; $oldScreen=$word.ScreenUpdating
    # Persist exact starting values BEFORE changing any global user information;
    # root can restore them even if Ctrl+C prevents PowerShell finally execution.
    [ordered]@{at=(Get-Date -Format o);UserName=$oldUser;UserInitials=$oldInitials;UseLocalUserInfo=$oldLocal;ScreenUpdating=$oldScreen;rootCreatedWordPIDCandidates=$ownedWordPids;sourceSHA256=$expected.source_sha256;scope='Exact v2 starting values only; does not claim recovery of unrecorded pre-v1 profile values'} | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 -LiteralPath $optionsPath
    $word.Visible=$false; $word.DisplayAlerts=0
    $word.Options.UseLocalUserInfo=$true
    $word.UserName=[string]$expected.probe_author
    $word.UserInitials='QA'
    $word.ScreenUpdating=$false

    Log 'scratch one-character revision author preflight'
    $doc=$word.Documents.Add()
    $doc.TrackRevisions=$false; $doc.Content.Text='a'
    $doc.TrackRevisions=$true
    $doc.Range(0,0).InsertBefore('b')
    $scratchAuthor=[string]$doc.Revisions.Item(1).Author
    $result.scratch_author=$scratchAuthor
    if($scratchAuthor -cne [string]$expected.probe_author){throw "Scratch author mismatch: $scratchAuthor"}
    Close-ProbeDocument

    Log 'baseline read-only open'
    $doc=$word.Documents.Open($expected.source,$false,$true,$false)
    $beforeHistory=Native-XML 'baseline-history' $false
    Log 'baseline native AcceptAll'
    $doc.AcceptAllRevisions()
    $result.baseline=Native-XML 'baseline-accepted' $true
    Close-ProbeDocument

    Copy-Item -LiteralPath $expected.prepared -Destination $candidate -Force
    $stableCandidateSha=(Get-FileHash -Algorithm SHA256 -LiteralPath $candidate).Hash
    Log 'editable QA candidate open / bookmark ranges'
    $doc=$word.Documents.Open($candidate,$false,$false,$false)
    $doc.TrackRevisions=$true
    $positions=@()
    foreach($target in $expected.targets){
        if(-not $doc.Bookmarks.Exists($target.bookmark)){throw "Missing QA bookmark $($target.bookmark)"}
        $positions += [pscustomobject]@{target=$target; start=$doc.Bookmarks.Item($target.bookmark).Range.Start}
    }
    foreach($item in ($positions | Sort-Object start -Descending)){
        $target=$item.target
        if(-not $doc.Bookmarks.Exists($target.bookmark)){throw "Lost bookmark $($target.bookmark)"}
        $paragraphRange=$doc.Bookmarks.Item($target.bookmark).Range.Paragraphs.Item(1).Range.Duplicate
        $terminal=([string]$paragraphRange.Text).EndsWith("`r$([char]7)")
        if($terminal){$start=$paragraphRange.Start-1;$end=$paragraphRange.Start}else{$start=$paragraphRange.End-1;$end=$paragraphRange.End}
        if($start -lt 0 -or $end -ne $start+1){throw "Invalid boundary $($target.bookmark)"}
        $deleteRange=$doc.Range($start,$end)
        if([string]$deleteRange.Text -cne "`r"){throw "Refusing non-paragraph character $($target.bookmark)"}
        $result.operations += [pscustomobject]@{bookmark=$target.bookmark; paragraph_id=$target.paragraph_id; requirement=$target.requirement; column=$target.column; start=$start; end=$end; cell_end_guard=$terminal; deleted_character=13}
        [void]$deleteRange.Delete(); $result.deletion_count++
        if(($result.deletion_count % 10) -eq 0){Log "tracked Delete $($result.deletion_count)/183"}
    }
    if($result.deletion_count -ne 183){throw 'Expected 183 actual native Delete calls'}
    Log 'native candidate Save'
    $doc.Save()
    $savedHistory=Native-XML 'candidate-history' $false
    if($savedHistory.probe_revision_count -eq 0){throw 'No distinct native probe-author revision'}
    $result.native_new_revision_nodes=$savedHistory.probe_revision_count
    Close-ProbeDocument
    $result.candidateSha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $candidate).Hash
    $stableCandidateSha=$result.candidateSha256
    Log 'saved candidate stable / ready for accepted and selective-rejected snapshots'

    Log 'candidate read-only native AcceptAll'
    $doc=$word.Documents.Open($candidate,$false,$true,$false)
    $doc.AcceptAllRevisions()
    $result.accept=Native-XML 'candidate-accepted' $true
    Close-ProbeDocument

    Log 'candidate native selective RejectAllRevisionsShown'
    $doc=$word.Documents.Open($candidate,$false,$true,$false)
    $view=$doc.Windows.Item(1).View
    $view.ShowRevisionsAndComments=$true; $view.ShowFormatChanges=$true; $view.ShowInsertionsAndDeletions=$true
    $reviewers=$view.RevisionsFilter.Reviewers
    for($i=1;$i -le $reviewers.Count;$i++){$reviewers.Item($i).Visible=$false}
    $reviewers.Item([string]$expected.probe_author).Visible=$true
    $doc.RejectAllRevisionsShown()
    $rejectedHistory=Native-XML 'candidate-new-rejected-history' $false
    if($rejectedHistory.probe_revision_count -ne 0){throw 'Native selective rejection left new probe revisions'}
    if(-not (Signatures-Equal $beforeHistory.historical_revision_text_signatures $rejectedHistory.historical_revision_text_signatures)){throw 'Native selective rejection altered historical revision text'}
    $result.native_new_revisions_rejected=$savedHistory.probe_revision_count
    $result.native_historical_revision_text_preserved=$true
    Log 'selective rejection then native AcceptAll historical changes'
    $doc.AcceptAllRevisions()
    $result.reject_new=Native-XML 'candidate-new-rejected-accepted' $true
    Close-ProbeDocument
    if((Get-FileHash -Algorithm SHA256 -LiteralPath $expected.source).Hash -ne $expected.source_sha256){throw '8AF source changed during QA'}
    $result.status='PASS'
    Log 'all native cells / native selective rejection PASS; no G publication'
}catch{
    $result.error=$_.Exception.Message
    Log "FAIL $($result.error)"
    throw
}finally{
    try{Close-ProbeDocument}catch{}
    if($word){
        try{
            if($null -ne $oldUser){$word.UserName=$oldUser};if($null -ne $oldInitials){$word.UserInitials=$oldInitials};if($null -ne $oldLocal){$word.Options.UseLocalUserInfo=$oldLocal};if($null -ne $oldScreen){$word.ScreenUpdating=$oldScreen}
            $result.options_restored=($word.UserName -ceq $oldUser -and $word.UserInitials -ceq $oldInitials -and $word.Options.UseLocalUserInfo -eq $oldLocal -and $word.ScreenUpdating -eq $oldScreen)
            if(-not $result.options_restored){$result.status='FAIL';$result.options_restore_error='Restored Office option values differ'}
        }catch{$result.status='FAIL';$result.options_restore_error=$_.Exception.Message}
        try{$word.Quit()}catch{}
        [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($word)
    }
    $result | ConvertTo-Json -Depth 12 | Set-Content -Encoding UTF8 -LiteralPath $recordPath
    [GC]::Collect();[GC]::WaitForPendingFinalizers()
}
