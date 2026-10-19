param([string]$QaRoot)
$ErrorActionPreference='Stop'
$fontPlan=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $QaRoot 'authored.json') | ConvertFrom-Json
$fontWord=New-Object -ComObject Word.Application
$fontWord.Visible=$false;$fontWord.DisplayAlerts=0;$fontWord.AutomationSecurity=3
$fontUser=@{name=$fontWord.UserName;initials=$fontWord.UserInitials};$records=@()
try{
 foreach($item in $fontPlan.docs){
  $before=(Get-FileHash -LiteralPath $item.candidate -Algorithm SHA256).Hash
  $d=$fontWord.Documents.Open([string]$item.candidate,$false,$true)
  try{
   $d.TrackRevisions=$false;$d.AcceptAllRevisions();$d.ShowRevisions=$false
   $matches=@()
   $checks=@($item.highlighted_paragraphs | ForEach-Object { @{kind='QCC';text=$_.text} })+@($item.note_changes | ForEach-Object { @{kind='unconfirmed';text=$_.after} })
   foreach($check in $checks){
    $range=$d.Content.Duplicate;$find=$range.Find;$find.ClearFormatting();$find.Text=[string]$check.text;$find.Forward=$true;$find.Wrap=0;$find.MatchWildcards=$false
    $found=$find.Execute()
    if(-not $found){throw ('Not found: '+$check.text)}
    $font=$range.Font
    $size=[double]$font.Size;$bold=[int]$font.Bold;$color=[int]$font.Color;$under=[int]$font.Underline
    $pass=($size -eq 10 -and $under -eq 0)
    if($check.kind -eq 'QCC'){$pass=$pass -and $bold -eq -1 -and $color -eq 192}
    else{$pass=$pass -and $bold -eq 0 -and $color -in @(0,-16777216)}
    $matches+=@{kind=$check.kind;text=$check.text;sizePt=$size;bold=$bold;colorWordBgr=$color;underline=$under;fontName=[string]$font.Name;start=$range.Start;end=$range.End;pass=$pass}
    if(-not $pass){throw ('Font failed: '+($matches[-1] | ConvertTo-Json -Compress))}
   }
   $records+=@{code=$item.code;path=$item.candidate;sha256=$before;matches=$matches;allPass=$true;scope='Read-only Word open; accept revisions in memory only to inspect current visible text. No document save or global setting change.'}
  }finally{$d.Close(0);[void][Runtime.InteropServices.Marshal]::ReleaseComObject($d)}
  if((Get-FileHash -LiteralPath $item.candidate -Algorithm SHA256).Hash -ne $before){throw 'Source changed'}
 }
 if($fontWord.UserName -ne $fontUser.name -or $fontWord.UserInitials -ne $fontUser.initials){throw 'Global settings changed'}
 @{status='PASS_NATIVE_EFFECTIVE_FONT';globalUserUnchanged=$true;documents=$records} | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $QaRoot 'native-font-check.json') -Encoding UTF8
 Write-Output 'Native Word effective font check passed: 12 QCC spans and 6 unconfirmed notices, all 10pt'
}finally{$fontWord.Quit();[void][Runtime.InteropServices.Marshal]::ReleaseComObject($fontWord)}
