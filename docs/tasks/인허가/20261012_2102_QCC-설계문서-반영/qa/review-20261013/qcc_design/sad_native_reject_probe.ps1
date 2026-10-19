param([string]$ProbeRoot=$PSScriptRoot)
# ROOT ONLY: read-only native verification of the already saved v2 candidate.
# No repeated paragraph deletions, no author preference change, no Save, no G write.
$ErrorActionPreference='Stop'
$v2=Join-Path $ProbeRoot 'native-trace-probe-v2'
$qa=Join-Path $v2 'reject-allmarkup'
[void][IO.Directory]::CreateDirectory($qa)
$helper=Join-Path $ProbeRoot 'sad_native_trace_probe_v2.py'
$python='C:\Users\EUNSU\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
$expected=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $v2 'expected-cells.json') | ConvertFrom-Json
$v2record=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $v2 'native-cell-proof.json') | ConvertFrom-Json
$candidate=[string]$v2record.candidate
$candidateSha=[string]$v2record.candidateSha256
$beforeHistory=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $v2 'baseline-history.json') | ConvertFrom-Json
$expectedOpen=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qa 'expected-open-history.json') | ConvertFrom-Json
$progress=Join-Path $qa 'progress.txt'
$recordPath=Join-Path $qa 'native-cell-proof.json'
$word=$null; $doc=$null; $view=$null; $oldView=$null; $oldScreen=$null
$result=[ordered]@{status='FAIL';candidate=$candidate;candidateSha256=$candidateSha;source=$expected.source;beforeSha256=$expected.source_sha256;baseline=$v2record.baseline;accept=$v2record.accept;deletion_count=$v2record.deletion_count;native_new_revision_nodes=$v2record.native_new_revision_nodes;v2_evidence=(Join-Path $v2 'native-cell-proof.json');rounds=@();GModified=$false;candidate_saved=$false;personalization_changed=$false;method='Read-only RevisionsFilter.Markup=2 / View=0 / ShowAll / ShowParagraphs / reviewer filter / RejectAllRevisionsShown, at most three diagnostic rounds'}

function Log([string]$Text){
    $line="$(Get-Date -Format o) $Text";Write-Host $line
    Add-Content -Encoding UTF8 -LiteralPath $progress -Value $line
    [ordered]@{phase=$Text;at=(Get-Date -Format o);candidate=$candidate;candidateSHA256=$candidateSha;sourceSHA256=$expected.source_sha256;GModified=$false;personalization_changed=$false} | ConvertTo-Json | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $qa 'phase.json')
}
function Native-XML([string]$Name,[bool]$Cells){
    Log "$Name native WordOpenXML single call"
    $xmlPath=Join-Path $qa "$Name.xml"; $jsonPath=Join-Path $qa "$Name.json"
    [IO.File]::WriteAllText($xmlPath,[string]$script:doc.Content.WordOpenXML,[Text.UTF8Encoding]::new($false))
    $mode=if($Cells){'xml-cells'}else{'xml-history'}
    & $python $helper $mode $xmlPath $jsonPath | ForEach-Object{Write-Host $_}
    if($LASTEXITCODE -ne 0){throw "$Name native XML / cells failed"}
    return (Get-Content -Raw -Encoding UTF8 -LiteralPath $jsonPath | ConvertFrom-Json)
}
function Signatures-Equal($Left,$Right){
    $keys=@($Left.PSObject.Properties.Name)+@($Right.PSObject.Properties.Name) | Sort-Object -Unique
    foreach($key in $keys){if([string]$Left.$key -cne [string]$Right.$key){return $false}}
    return $true
}
function All-QA-Markup {
    $script:view.RevisionsFilter.Markup=2 # wdRevisionsMarkupAll
    $script:view.RevisionsFilter.View=0 # wdRevisionsViewFinal
    $script:view.ShowAll=$true; $script:view.ShowParagraphs=$true
    $script:view.ShowRevisionsAndComments=$true
    $script:view.ShowFormatChanges=$true; $script:view.ShowInsertionsAndDeletions=$true
    $reviewers=$script:view.RevisionsFilter.Reviewers
    for($i=1;$i -le $reviewers.Count;$i++){$reviewers.Item($i).Visible=$false}
    $reviewers.Item([string]$expected.probe_author).Visible=$true
}

try{
    if((Get-FileHash -Algorithm SHA256 -LiteralPath $candidate).Hash -ne $candidateSha){throw 'Stable v2 candidate changed'}
    if((Get-FileHash -Algorithm SHA256 -LiteralPath $expected.source).Hash -ne $expected.source_sha256){throw 'Exact 8AF source changed'}
    Log 'start hidden Word / read-only candidate'
    $pidsBefore=@((Get-Process -Name WINWORD -ErrorAction SilentlyContinue).Id)
    $word=New-Object -ComObject Word.Application
    $pidsOwned=@((Get-Process -Name WINWORD -ErrorAction SilentlyContinue).Id | Where-Object {$_ -notin $pidsBefore})
    $oldScreen=$word.ScreenUpdating
    $options=[ordered]@{at=(Get-Date -Format o);UserName=$word.UserName;UserInitials=$word.UserInitials;UseLocalUserInfo=$word.Options.UseLocalUserInfo;ScreenUpdating=$oldScreen;rootCreatedWordPIDCandidates=$pidsOwned;personalization_changed=$false}
    $options | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $qa 'options-before.json')
    $word.Visible=$false; $word.DisplayAlerts=0; $word.ScreenUpdating=$false
    $doc=$word.Documents.Open($candidate,$false,$true,$false)
    $view=$doc.Windows.Item(1).View
    $reviewerBefore=@()
    for($i=1;$i -le $view.RevisionsFilter.Reviewers.Count;$i++){
        $r=$view.RevisionsFilter.Reviewers.Item($i)
        $reviewerBefore += [pscustomobject]@{Index=$i;Visible=[bool]$r.Visible}
    }
    $oldView=[ordered]@{Markup=$view.RevisionsFilter.Markup;View=$view.RevisionsFilter.View;ShowAll=$view.ShowAll;ShowParagraphs=$view.ShowParagraphs;ShowRevisionsAndComments=$view.ShowRevisionsAndComments;ShowFormatChanges=$view.ShowFormatChanges;ShowInsertionsAndDeletions=$view.ShowInsertionsAndDeletions;Reviewers=$reviewerBefore}
    $oldView | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $qa 'view-before.json')
    $initial=Native-XML 'initial-history' $false
    # Word drops 125 redundant pPrChange snapshots on reopening. Their current
    # properties and paragraph-mark deletions are unchanged, demonstrated in
    # open-normalization-format-detail.json. Guard this exact opened state,
    # rather than equating a saved-package revision count with the native view.
    if($initial.probe_revision_count -ne 235 -or $initial.probe_revision_types.del -ne 183 -or $initial.probe_revision_types.pPrChange -ne 52 -or @($initial.probe_revision_types.PSObject.Properties).Count -ne 2){throw 'Native reopened candidate differs from proven del183 / pPrChange52 state'}
    if($initial.paragraph_count -ne 1435 -or $initial.paragraph_format_sha256 -cne $expectedOpen.paragraph_format_sha256 -or $initial.style_semantic_sha256 -cne $expectedOpen.style_semantic_sha256){throw 'Native reopened paragraph formats / styles differ from independently inspected opened state'}
    if(-not (Signatures-Equal $beforeHistory.historical_revision_text_signatures $initial.historical_revision_text_signatures)){throw 'Native reopened candidate altered historical revision text'}
    $result.opened_QA_revision_nodes=$initial.probe_revision_count
    $result.opened_state_guard='del183 / pPrChange52 / 1435 paragraphs / exact current paragraph formatting and style semantics / historical revision text'
    for($round=1;$round -le 3;$round++){
        All-QA-Markup
        Log "round $round RejectAllRevisionsShown"
        $doc.RejectAllRevisionsShown()
        $history=Native-XML "round-$round-history" $false
        $oldTextSame=Signatures-Equal $beforeHistory.historical_revision_text_signatures $history.historical_revision_text_signatures
        $result.rounds += [pscustomobject]@{round=$round;remaining_QA_nodes=$history.probe_revision_count;historical_revision_text_preserved=$oldTextSame;history=(Join-Path $qa "round-$round-history.json")}
        if(-not $oldTextSame){throw 'Selective rejection altered historical revision text'}
        Log "round $round remaining QA nodes $($history.probe_revision_count)"
        if($history.probe_revision_count -eq 0){break}
    }
    if($history.probe_revision_count -ne 0){throw "Selective bulk rejection left $($history.probe_revision_count) new QA nodes"}
    $result.saved_QA_revision_nodes=$v2record.native_new_revision_nodes
    $result.normalized_redundant_format_nodes=$v2record.native_new_revision_nodes-$initial.probe_revision_count
    $result.native_new_revisions_rejected=$initial.probe_revision_count
    $result.native_historical_revision_text_preserved=$true
    Log 'new-only rejection complete / AcceptAll historical revisions'
    $doc.AcceptAllRevisions()
    $result.reject_new=Native-XML 'new-rejected-accepted' $true
    if((Get-FileHash -Algorithm SHA256 -LiteralPath $candidate).Hash -ne $candidateSha){throw 'Read-only probe changed stable candidate'}
    if((Get-FileHash -Algorithm SHA256 -LiteralPath $expected.source).Hash -ne $expected.source_sha256){throw 'Read-only probe changed 8AF source'}
    $result.status='PASS'
    Log 'native accepted and new-only rejected 83x3 cell proof PASS'
}catch{
    $result.error=$_.Exception.Message; Log "FAIL $($result.error)";throw
}finally{
    if($view -and $oldView){
        try{
            foreach($name in @('ShowAll','ShowParagraphs','ShowRevisionsAndComments','ShowFormatChanges','ShowInsertionsAndDeletions')){$view.$name=$oldView[$name]}
            $view.RevisionsFilter.Markup=$oldView.Markup; $view.RevisionsFilter.View=$oldView.View
            foreach($r in $oldView.Reviewers){try{$view.RevisionsFilter.Reviewers.Item([int]$r.Index).Visible=$r.Visible}catch{}}
            $result.view_options_restored=($view.RevisionsFilter.Markup -eq $oldView.Markup -and $view.RevisionsFilter.View -eq $oldView.View -and $view.ShowAll -eq $oldView.ShowAll -and $view.ShowParagraphs -eq $oldView.ShowParagraphs)
            if(-not $result.view_options_restored){$result.status='FAIL';$result.view_restore_error='Restored view options differ from starting values'}
        }catch{$result.status='FAIL';$result.view_restore_error=$_.Exception.Message}
    }
    if($doc){try{$doc.Close(0)}catch{};[void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($doc)}
    if($word){
        try{$word.ScreenUpdating=$oldScreen;$result.options_restored=($word.UserName -ceq $options.UserName -and $word.UserInitials -ceq $options.UserInitials -and $word.Options.UseLocalUserInfo -eq $options.UseLocalUserInfo -and $word.ScreenUpdating -eq $oldScreen);if(-not $result.options_restored){$result.status='FAIL';$result.options_restore_error='Read-only probe changed starting personalization or screen options'}}catch{$result.status='FAIL';$result.options_restore_error=$_.Exception.Message}
        try{$word.Quit()}catch{};[void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($word)
    }
    $result | ConvertTo-Json -Depth 15 | Set-Content -Encoding UTF8 -LiteralPath $recordPath
    [GC]::Collect();[GC]::WaitForPendingFinalizers()
}
