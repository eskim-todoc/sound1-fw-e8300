param([string]$QaRoot,[string]$Mode='before')
$ErrorActionPreference='Stop'
$runPlan=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $QaRoot $(if($Mode -eq 'before'){'baseline.json'}else{'authored.json'})) | ConvertFrom-Json
$renderWord=New-Object -ComObject Word.Application
$renderWord.Visible=$false
$renderWord.DisplayAlerts=0
$renderWord.AutomationSecurity=3
$renderUser=@{name=$renderWord.UserName;initials=$renderWord.UserInitials}
$renderRecords=@()
try {
 foreach($item in $runPlan.docs){
  $renderPath=if($Mode -eq 'before'){$item.backup}else{$item.candidate}
  $renderHash=(Get-FileHash -LiteralPath $renderPath).Hash
  $renderDoc=$renderWord.Documents.Open([string]$renderPath,$false,$true)
  try {
   $renderDoc.AcceptAllRevisions();$renderDoc.TrackRevisions=$false
   $renderDoc.Repaginate()
   if($Mode -eq 'update'){
    [void]$renderDoc.Fields.Update()
    foreach($renderTOC in $renderDoc.TablesOfContents){$renderTOC.Update()}
    $renderDoc.Repaginate()
   }
   $renderPdf=Join-Path $QaRoot ($item.code+'-'+$Mode+'.pdf')
   $renderDoc.ExportAsFixedFormat($renderPdf,17)
   $renderText=[string]$renderDoc.Content.Text
   [IO.File]::WriteAllText((Join-Path $QaRoot ($item.code+'-'+$Mode+'.txt')),$renderText,[Text.UTF8Encoding]::new($false))
   if($Mode -eq 'update'){
    $renderDoc.SaveAs2((Join-Path $QaRoot ($item.code+'-accepted-fields.docx')),16)
   }
   $renderRecords+=@{code=$item.code;path=$renderPath;sha256=$renderHash;pdf=$renderPdf;pages=$renderDoc.ComputeStatistics(2);mode=$Mode;nativeAccepted=$true;globalUserUnchanged=$true;checkedAt=(Get-Date).ToString('o')}
   Write-Output ($item.code+' '+$Mode+' native render '+$renderDoc.ComputeStatistics(2)+' pages')
  }finally{$renderDoc.Close(0);[void][Runtime.InteropServices.Marshal]::ReleaseComObject($renderDoc)}
  if((Get-FileHash -LiteralPath $renderPath).Hash -ne $renderHash){throw 'Native read-only render modified source'}
 }
 if($renderWord.UserName -ne $renderUser.name -or $renderWord.UserInitials -ne $renderUser.initials){throw 'Word user settings changed'}
 ConvertTo-Json -InputObject @($renderRecords) -Depth 5 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $QaRoot ('native-'+$Mode+'.json'))
}finally{$renderWord.Quit();[void][Runtime.InteropServices.Marshal]::ReleaseComObject($renderWord)}
