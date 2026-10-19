param([string]$QaRoot)
$ErrorActionPreference='Stop'
$sadPlan=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $QaRoot 'authored.json') | ConvertFrom-Json
$sadWord=New-Object -ComObject Word.Application
$sadWord.Visible=$false
$sadWord.DisplayAlerts=0
$sadWord.AutomationSecurity=3
$sadUser=@{name=$sadWord.UserName;initials=$sadWord.UserInitials}
$sadRecords=@()
$sadTablesBefore=@()
try{
 foreach($sadMode in @('before','after')){
  $sadPath=if($sadMode -eq 'before'){$sadPlan.source}else{$sadPlan.candidate}
  $sadExpectedHash=if($sadMode -eq 'before'){$sadPlan.source_sha256}else{$sadPlan.candidate_sha256}
  if((Get-FileHash -LiteralPath $sadPath).Hash -ne $sadExpectedHash){throw 'SAD input hash changed'}
  $sadDoc=$sadWord.Documents.Open([string]$sadPath,$false,$true)
  try{
   $sadDoc.AcceptAllRevisions()
   $sadDoc.TrackRevisions=$false
   $sadDoc.Repaginate()
   $sadWholeText=[string]$sadDoc.Content.Text
   $sadOutside=''
   $sadTables=@()
   foreach($sadTable in $sadDoc.Tables){$sadTables+=,[string]$sadTable.Range.Text}
   if($sadMode -eq 'before'){$sadTablesBefore=$sadTables}else{
    if($sadTables.Count -ne $sadTablesBefore.Count){throw 'Native table count changed'}
    for($sadT=0;$sadT -lt $sadTables.Count;$sadT++){if($sadTables[$sadT] -cne $sadTablesBefore[$sadT]){throw "Native table $sadT changed"}}
   }
   $sadPreviousEnd=$sadDoc.Content.Start
   for($sadT=1;$sadT -le $sadDoc.Tables.Count;$sadT++){
    $sadRange=$sadDoc.Tables.Item($sadT).Range
    if($sadRange.Start -gt $sadPreviousEnd){$sadOutside+=[string]$sadDoc.Range($sadPreviousEnd,$sadRange.Start).Text}
    $sadPreviousEnd=[Math]::Max($sadPreviousEnd,$sadRange.End)
   }
   if($sadPreviousEnd -lt $sadDoc.Content.End){$sadOutside+=[string]$sadDoc.Range($sadPreviousEnd,$sadDoc.Content.End).Text}
   $sadOutsideCount=([regex]::Matches($sadOutside,'SI-ID-QCC-00[1-4]')).Count
   if($sadMode -eq 'before' -and $sadOutsideCount -ne 8){throw "Unexpected original outside-table ID count $sadOutsideCount"}
   if($sadMode -eq 'after' -and $sadOutsideCount -ne 0){throw "Module IDs remained outside tables $sadOutsideCount"}
   $sadPdf=Join-Path $QaRoot ('SAD-'+$sadMode+'-native.pdf')
   $sadDoc.ExportAsFixedFormat($sadPdf,17)
   [IO.File]::WriteAllText((Join-Path $QaRoot ('SAD-'+$sadMode+'-native.txt')),$sadWholeText,[Text.UTF8Encoding]::new($false))
   $sadRecords+=@{mode=$sadMode;path=$sadPath;sha256=$sadExpectedHash;pdf=$sadPdf;
    pages=$sadDoc.ComputeStatistics(2);nativeAccepted=$true;nativeTableCount=$sadTables.Count;
    allNativeTableTextPreserved=($sadMode -eq 'after');outsideTableModuleIdCount=$sadOutsideCount;
    checkedAt=(Get-Date).ToString('o')}
  }finally{$sadDoc.Close(0);[void][Runtime.InteropServices.Marshal]::ReleaseComObject($sadDoc)}
  if((Get-FileHash -LiteralPath $sadPath).Hash -ne $sadExpectedHash){throw 'Read-only native test modified input'}
  Write-Output ('SAD '+$sadMode+' native accepted and rendered')
 }
 if($sadWord.UserName -ne $sadUser.name -or $sadWord.UserInitials -ne $sadUser.initials){throw 'Word user settings changed'}
 ConvertTo-Json -InputObject @($sadRecords) -Depth 4 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $QaRoot 'native-checks.json')
}finally{$sadWord.Quit();[void][Runtime.InteropServices.Marshal]::ReleaseComObject($sadWord)}
