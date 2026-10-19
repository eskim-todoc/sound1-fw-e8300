param([string]$QaRoot)
$ErrorActionPreference='Stop'
$r83Cases=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $QaRoot 'narrow-proof.json') | ConvertFrom-Json
$r83Word=New-Object -ComObject Word.Application
$r83Word.Visible=$false
$r83Word.DisplayAlerts=0
$r83Word.AutomationSecurity=3
$r83Settings=@{name=$r83Word.UserName;initials=$r83Word.UserInitials}
$r83Records=@()
function Normalize-R83Projection([string]$Text){return [regex]::Replace($Text,'\s|\x07','')}
try{
 foreach($r83Case in $r83Cases){
  if((Get-FileHash -LiteralPath $r83Case.original_candidate).Hash -ne $r83Case.original_candidate_sha256){throw 'Full candidate changed'}
  if((Get-FileHash -LiteralPath $r83Case.qa_new_only).Hash -ne $r83Case.qa_new_only_sha256){throw 'Projection changed'}
  $r83Count=0
  $r83TextResults=@{}
  foreach($r83Mode in @('accept','reject')){
   $r83Doc=$r83Word.Documents.Open([string]$r83Case.qa_new_only,$false,$true)
   try{
    $r83Count=$r83Doc.Content.Revisions.Count
    if($r83Count -ne $r83Case.new_revision_count){throw "Unexpected projection revision count $r83Count"}
    if($r83Mode -eq 'accept'){$r83Doc.AcceptAllRevisions()}else{$r83Doc.RejectAllRevisions()}
    $r83Text=[string]$r83Doc.Content.Text
    $r83Expected=if($r83Mode -eq 'accept'){$r83Case.expected_accept}else{$r83Case.expected_reject}
    if((Normalize-R83Projection $r83Text) -ne (Normalize-R83Projection $r83Expected)){throw "Projection $r83Mode text mismatch: $r83Text"}
    $r83TextResults[$r83Mode]=$r83Text
   }finally{$r83Doc.Close(0);[void][Runtime.InteropServices.Marshal]::ReleaseComObject($r83Doc)}
  }
  if((Get-FileHash -LiteralPath $r83Case.qa_new_only).Hash -ne $r83Case.qa_new_only_sha256){throw 'Read-only projection was modified'}
  if((Get-FileHash -LiteralPath $r83Case.original_candidate).Hash -ne $r83Case.original_candidate_sha256){throw 'Full candidate modified'}
  $r83Records+=@{code=$r83Case.code;candidate=$r83Case.original_candidate;candidateSHA256=$r83Case.original_candidate_sha256;
   projection=$r83Case.qa_new_only;projectionSHA256=$r83Case.qa_new_only_sha256;nativeNewRevisionCount=$r83Count;
   nativeAcceptMatchesCurrent=$true;nativeRejectMatchesBefore=$true;observed=$r83TextResults;
   candidateAndProjectionUnchanged=$true;directSelectiveRejectOnFullCandidate=$false;
   scope='Actual Word acceptance/rejection of new revisions in minimal original-paragraph QA projection after accepting prior paragraph history. Full candidate separately tested for native acceptance, retained revision XML, and XML new-only rejection restoring prior paragraph.';
   checkedAt=(Get-Date).ToString('o')}
  ConvertTo-Json -InputObject @($r83Records) -Depth 5 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $QaRoot 'native-projection-checks.json')
  Write-Output ($r83Case.code+' projection native accept/reject PASS; revisions='+$r83Count)
 }
 if($r83Word.UserName -ne $r83Settings.name -or $r83Word.UserInitials -ne $r83Settings.initials){throw 'Global Word settings changed'}
}finally{$r83Word.Quit();[void][Runtime.InteropServices.Marshal]::ReleaseComObject($r83Word)}

