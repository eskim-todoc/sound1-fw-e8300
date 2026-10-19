param([string]$QaRoot,[switch]$SkipReject)
$ErrorActionPreference='Stop'
$r83Items=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $QaRoot 'word\authored.json') | ConvertFrom-Json
$r83Word=New-Object -ComObject Word.Application
$r83Word.Visible=$false
$r83Word.DisplayAlerts=0
$r83Word.AutomationSecurity=3
$r83Settings=@{userName=$r83Word.UserName;initials=$r83Word.UserInitials}
$r83Records=@()
function Normalize-R83([string]$Value){return [regex]::Replace($Value,'\s|\x07','')}
try{
 foreach($r83Item in $r83Items){
  if((Get-FileHash -LiteralPath $r83Item.candidate).Hash -ne $r83Item.candidate_sha256){throw 'Word candidate hash mismatch'}
  $r83Doc=$r83Word.Documents.Open([string]$r83Item.candidate,$false,$true)
  try{
   Write-Output ($r83Item.code+' accept and render')
   [IO.File]::WriteAllText((Join-Path $QaRoot ('word\'+$r83Item.code+'-native-open.xml')),[string]$r83Doc.Content.WordOpenXML,[Text.UTF8Encoding]::new($false))
   $r83Doc.AcceptAllRevisions()
   $r83Doc.TrackRevisions=$false
   $r83Doc.Repaginate()
   $r83Text=[string]$r83Doc.Content.Text
   if($r83Text.Contains('SYS-RID-ACDOC')){throw 'Active ACDOC remained'}
   if(-not (Normalize-R83 $r83Text).Contains((Normalize-R83 $r83Item.after_current_paragraph))){throw 'Expected accepted paragraph missing'}
   $r83CellCount=0
   if($r83Item.code -eq 'SAD'){
    $r83Expected=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $QaRoot 'word\SAD-expected-native-cells.json') | ConvertFrom-Json
    foreach($r83Table in $r83Doc.Tables){
     if($r83Table.Range.Text -match 'SW-FW-RID-83' -and $r83Table.Range.Text -match 'SW-FW-RID-1'){
      if($r83Table.Rows.Count -ne $r83Expected.row_count){throw 'SAD row count mismatch'}
      for($r83Row=1;$r83Row -le $r83Expected.row_count;$r83Row++){
       for($r83Column=1;$r83Column -le 3;$r83Column++){
        $r83Actual=[string]$r83Table.Cell($r83Row,$r83Column).Range.Text
        if((Normalize-R83 $r83Actual) -ne (Normalize-R83 $r83Expected.rows[$r83Row-1].cells[$r83Column-1])){throw "SAD cell mismatch $r83Row/$r83Column"}
        $r83CellCount++
       }
      }
     }
    }
    if($r83CellCount -ne 249){throw 'SAD 249 native cells not checked'}
   }
   $r83Pdf=Join-Path $QaRoot ('word\'+$r83Item.code+'-native.pdf')
   $r83Doc.ExportAsFixedFormat($r83Pdf,17)
   $r83Pages=$r83Doc.ComputeStatistics(2)
   [IO.File]::WriteAllText((Join-Path $QaRoot ('word\'+$r83Item.code+'-native.txt')),$r83Text,[Text.UTF8Encoding]::new($false))
  }finally{$r83Doc.Close(0);[void][Runtime.InteropServices.Marshal]::ReleaseComObject($r83Doc)}
  $r83Rejected=0
  if(-not $SkipReject){
  $r83Doc=$r83Word.Documents.Open([string]$r83Item.candidate,$false,$true)
  try{
   Write-Output ($r83Item.code+' reject changed paragraph only')
   $r83Date=[datetimeoffset]::Parse($r83Item.new_revision_date)
   $r83Rejected=0
   $r83Find=$r83Doc.Content
   $r83Needle=if($r83Item.code -eq 'SR'){'자체 소프트웨어 요구사항이다.'}else{'제품 문서 항목 / 실행 유닛 배정 대상 아님'}
   if(-not $r83Find.Find.Execute($r83Needle,$false,$false,$false,$false,$false,$true,0,$false)){throw 'Target paragraph not found'}
   Write-Output ('Found '+$r83Find.Start+'/'+$r83Find.End+' '+$r83Find.Text)
   $r83ParagraphRange=$r83Find.Duplicate
   [void]$r83ParagraphRange.Expand(4)
   Write-Output ('Paragraph '+$r83ParagraphRange.Start+'/'+$r83ParagraphRange.End+' '+$r83ParagraphRange.Text)
   Write-Output ('Target revisions '+$r83ParagraphRange.Revisions.Count)
   $r83ScopedRevisions=@($r83ParagraphRange.Revisions)
   for($r83Index=$r83ScopedRevisions.Count-1;$r83Index -ge 0;$r83Index--){
    $r83Revision=$r83ScopedRevisions[$r83Index]
    try{
     Write-Output ([string]$r83Revision.Author+' '+([datetime]$r83Revision.Date).ToString('o')+' range '+$r83Revision.Range.Start+'/'+$r83Revision.Range.End)
     if($r83Revision.Author -eq $r83Item.new_revision_author -and ([datetime]$r83Revision.Date).ToString('yyyy-MM-ddTHH:mm') -eq $r83Date.UtcDateTime.ToString('yyyy-MM-ddTHH:mm')){$r83Revision.Reject();$r83Rejected++}
    }finally{[void][Runtime.InteropServices.Marshal]::ReleaseComObject($r83Revision)}
   }
   if($r83Rejected -ne $r83Item.new_revision_ids.Count){throw "New-only rejection count mismatch $r83Rejected"}
   $r83Doc.AcceptAllRevisions()
   if(-not (Normalize-R83 ([string]$r83Doc.Content.Text)).Contains((Normalize-R83 $r83Item.before_current_paragraph))){throw 'Native rejection did not restore old paragraph'}
  }finally{$r83Doc.Close(0);[void][Runtime.InteropServices.Marshal]::ReleaseComObject($r83Doc)}
  }
  if((Get-FileHash -LiteralPath $r83Item.candidate).Hash -ne $r83Item.candidate_sha256){throw 'Word source modified by read-only QA'}
  $r83Records+=@{code=$r83Item.code;path=$r83Item.candidate;sha256=$r83Item.candidate_sha256;pdf=$r83Pdf;pages=$r83Pages;nativeAccepted=$true;nativeRejectedNewCount=$r83Rejected;SADNativeCellCount=$r83CellCount;sourceUnchanged=$true}
  $r83Records | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $QaRoot 'word\native-checks.json')
  Write-Output ($r83Item.code+' native accepted/exported; rejected-new='+$r83Rejected)
 }
 if($r83Word.UserName -ne $r83Settings.userName -or $r83Word.UserInitials -ne $r83Settings.initials){throw 'Word user settings changed'}
}finally{$r83Word.Quit();[void][Runtime.InteropServices.Marshal]::ReleaseComObject($r83Word)}
