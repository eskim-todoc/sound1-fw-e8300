param([switch]$Probe)
$ErrorActionPreference='Stop'
$qa=Join-Path $PSScriptRoot 'qa\design-readability\purpose-and-compact-style'
$rows=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qa 'fragments.json')|ConvertFrom-Json
if($Probe){$rows=@($rows[0])}
$app=New-Object -ComObject Word.Application
$app.Visible=$false;$app.DisplayAlerts=0;$app.AutomationSecurity=3;$oldName=$app.UserName;$oldInitials=$app.UserInitials
$app.UserName='김은수';$app.UserInitials='은수';$app.ScreenUpdating=$false;$doc=$null
try {
 $doc=$app.Documents.Open((Join-Path $qa 'styled.docx'),$false,$false,$false)
 $doc.TrackRevisions=$true;$doc.TrackFormatting=$true
 $records=New-Object System.Collections.Generic.List[object]
 foreach($row in @($rows|Sort-Object { $doc.Bookmarks.Item($_.bookmark).Range.Start } -Descending)){
  $range=$doc.Bookmarks.Item($row.bookmark).Range
  $start=$range.Start;$old=$range.Text
  $range.Delete()|Out-Null
  $range=$doc.Range($start,$start)
  $range.InsertXML((Get-Content -Raw -Encoding UTF8 -LiteralPath $row.fragment))
  $records.Add(@{unit=$row.unit;oldChars=$old.Length;tables=$row.tables})
  if($records.Count % 10 -eq 0){Write-Output ('units updated='+$records.Count)}
 }
 $out=if($Probe){'probe'}else{'interim'}
 $doc.SaveAs2((Join-Path $qa ($out+'.docx')),16)
 $records|ConvertTo-Json -Depth 6|Set-Content -LiteralPath (Join-Path $qa ($out+'-redline.json')) -Encoding utf8
 Write-Output ($out+' revisions='+$doc.Revisions.Count+' tables='+$doc.Tables.Count)
 $doc.Close(0);$doc=$null
}finally{
 if($null -ne $doc){$doc.Close(0)};$app.UserName=$oldName;$app.UserInitials=$oldInitials;$app.Quit(0);$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($app)
}
