$ErrorActionPreference='Stop'
$qa=Join-Path $PSScriptRoot 'qa\method-definitions\performance'
$word=New-Object -ComObject Word.Application
$word.Visible=$false;$word.DisplayAlerts=0;$word.AutomationSecurity=3
try{
 foreach($stem in @('before','candidate')){
  $src=Join-Path $qa ($stem+'.docx');$qaDoc=Join-Path $qa ($stem+'-accepted.docx');Copy-Item -LiteralPath $src -Destination $qaDoc
  $doc=$null
  try{
   $doc=$word.Documents.Open($qaDoc,$false,$false)
   $doc.Revisions.AcceptAll();$doc.TrackRevisions=$false;$doc.Repaginate();$doc.Save()
   $doc.ExportAsFixedFormat((Join-Path $qa ($stem+'-accepted.pdf')),17)
   Write-Output ($stem+' '+$doc.ComputeStatistics(2)+' pages')
  }finally{if($doc){$doc.Close(0)}}
 }
}finally{$word.Quit();$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($word)}
