param([string]$QaRoot)
$ErrorActionPreference='Stop'
$projectionPlan=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $QaRoot 'projection-plan.json') | ConvertFrom-Json
$projectionWord=New-Object -ComObject Word.Application
$projectionWord.Visible=$false;$projectionWord.DisplayAlerts=0;$projectionWord.AutomationSecurity=3
$projectionUser=@{name=$projectionWord.UserName;initials=$projectionWord.UserInitials}
$records=@()
function Normal([string]$s){return ($s -replace "`r`n","`n" -replace "`r","`n").Trim()}
try{
 foreach($item in $projectionPlan){
  foreach($mode in @('accept','reject')){
   $d=$projectionWord.Documents.Open([string]$item.path,$false,$true)
   try{
    if($d.Content.Revisions.Count -ne $item.revision_count){throw 'New text revision count mismatch'}
    if($mode -eq 'accept'){$d.AcceptAllRevisions();$expected=$item.expected_accept}else{$d.RejectAllRevisions();$expected=$item.expected_reject}
    if((Normal $d.Content.Text) -cne (Normal $expected)){throw ($item.code+' '+$mode+' text mismatch')}
   }finally{$d.Close(0);[void][Runtime.InteropServices.Marshal]::ReleaseComObject($d)}
  }
  if((Get-FileHash -LiteralPath $item.path).Hash -ne $item.sha256){throw 'QA input modified'}
  $records+=@{code=$item.code;newTextAcceptMatches=$true;newTextRejectRestoresBefore=$true;nativeRevisionCount=$item.revision_count;scope=$item.scope}
  Write-Output ($item.code+' text accept/reject PASS')
 }
 if($projectionWord.UserName -ne $projectionUser.name -or $projectionWord.UserInitials -ne $projectionUser.initials){throw 'Global settings changed'}
 ConvertTo-Json -InputObject @($records) -Depth 5 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $QaRoot 'native-projection.json')
}finally{$projectionWord.Quit();[void][Runtime.InteropServices.Marshal]::ReleaseComObject($projectionWord)}
