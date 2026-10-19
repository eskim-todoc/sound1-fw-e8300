$ErrorActionPreference='Stop'
$taskPath=$PSScriptRoot
$dhfPath='G:\공유 드라이브\20_프로젝트별\01_project sound1\08_DHF\03_설계 출력\SW\외부기'
$current=(Get-ChildItem -LiteralPath $dhfPath -Filter '*SAD*.docx' | Where-Object Name -Like '[[]E8300-QCC]*').FullName
$cases=@(@{name='before_targeted';path=(Join-Path $taskPath 'qa/qcc_design/SAD-before-final-targeted.docx')},@{name='current';path=$current},@{name='diagnostic_without_tail_format_revision';path=(Join-Path $taskPath 'qa/qcc_design/SAD-diagnostic-no-tail-format-track.docx')})
$app=New-Object -ComObject Word.Application
$app.Visible=$false
$app.DisplayAlerts=0
$app.AutomationSecurity=3
$results=New-Object System.Collections.Generic.List[object]
try {
 foreach($case in $cases){
  $doc=$null
  try {
   $beforeHash=(Get-FileHash -LiteralPath $case.path -Algorithm SHA256).Hash
   $doc=$app.Documents.Open($case.path,$false,$true,$false)
   $count=$doc.Revisions.Count
   $nulls=New-Object System.Collections.Generic.List[object]
   $near=New-Object System.Collections.Generic.List[object]
   for($i=1;$i -le $count;$i++){
    $rev=$doc.Revisions.Item($i)
    if($null -eq $rev.Range -or $null -eq $rev.Type -or $null -eq $rev.Author){$nulls.Add([pscustomobject]@{index=$i;type=$rev.Type;author=$rev.Author;range_is_null=($null -eq $rev.Range)})}
    if($i -ge 32 -and $i -le 44){$near.Add([pscustomobject]@{index=$i;type=$rev.Type;author=$rev.Author;date=[string]$rev.Date;start=$rev.Range.Start;end=$rev.Range.End;text=$rev.Range.Text})}
   }
   $results.Add([pscustomobject]@{case=$case.name;path=$case.path;sha256=$beforeHash;revision_count=$count;null_items=$nulls;items_32_44=$near;read_only=$doc.ReadOnly;saved=$false})
  } finally {if($null -ne $doc){$doc.Close(0);[void][Runtime.InteropServices.Marshal]::ReleaseComObject($doc)}}
  if((Get-FileHash -LiteralPath $case.path -Algorithm SHA256).Hash -ne $beforeHash){throw 'Read-only file changed'}
 }
} finally {$app.Quit(0);[void][Runtime.InteropServices.Marshal]::ReleaseComObject($app)}
$results | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $taskPath 'qa/qcc_design/SAD-format-track-native-probe.json')
foreach($r in $results){Write-Output ($r.case+' revisions='+$r.revision_count+' nulls='+$r.null_items.Count)}
