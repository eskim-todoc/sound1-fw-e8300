$ErrorActionPreference='Stop'
$taskPath=$PSScriptRoot
$dhfPath='G:\공유 드라이브\20_프로젝트별\01_project sound1\08_DHF\03_설계 출력\SW\외부기'
$current=(Get-ChildItem -LiteralPath $dhfPath -Filter '*SAD*.docx' | Where-Object Name -Like '[[]E8300-QCC]*').FullName
$cases=@(@{name='current';path=$current},@{name='before_targeted';path=(Join-Path $taskPath 'qa/qcc_design/SAD-before-final-targeted.docx')},@{name='diagnostic_without_tail_format_revision';path=(Join-Path $taskPath 'qa/qcc_design/SAD-diagnostic-no-tail-format-track.docx')})
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
   $items=New-Object System.Collections.Generic.List[object]
   foreach($i in @(37,38,39)){
    $rev=$doc.Revisions.Item($i)
    $items.Add([pscustomobject]@{index=$i;type=$rev.Type;author=$rev.Author;date=[string]$rev.Date;start=$rev.Range.Start;end=$rev.Range.End;text=$rev.Range.Text;range_is_null=($null -eq $rev.Range)})
   }
   $r=[pscustomobject]@{case=$case.name;path=$case.path;sha256=$beforeHash;revision_count=$doc.Revisions.Count;items=$items;read_only=$doc.ReadOnly;saved=$false}
   $results.Add($r)
   Write-Output ($case.name+': '+($items|ConvertTo-Json -Compress -Depth 3))
  } finally {if($null -ne $doc){$doc.Close(0);[void][Runtime.InteropServices.Marshal]::ReleaseComObject($doc)}}
  if((Get-FileHash -LiteralPath $case.path -Algorithm SHA256).Hash -ne $beforeHash){throw 'Read-only file changed'}
 }
} finally {$app.Quit(0);[void][Runtime.InteropServices.Marshal]::ReleaseComObject($app)}
$results | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $taskPath 'qa/qcc_design/SAD-format-track-native-quick.json')
