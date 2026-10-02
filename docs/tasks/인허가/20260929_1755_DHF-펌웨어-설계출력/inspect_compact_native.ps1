$ErrorActionPreference='Stop'
$qa=Join-Path $PSScriptRoot 'qa\design-readability\purpose-and-compact-style'
$app=New-Object -ComObject Word.Application
$app.Visible=$false;$app.DisplayAlerts=0;$app.AutomationSecurity=3;$doc=$null
try {
 $doc=$app.Documents.Open((Join-Path $qa 'candidate-accepted.docx'),$false,$true,$false)
 $rows=New-Object System.Collections.Generic.List[object]
 foreach($s in $doc.Styles){
  if($s.NameLocal.StartsWith('TDC ')){$rows.Add(@{name=$s.NameLocal;size=$s.Font.Size;left_pt=$s.ParagraphFormat.LeftIndent})}
 }
 $rows|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $qa 'native-style-sizes.json') -Encoding utf8
 $rows|Where-Object {$_.name -match '^TDC (제목|본문) [1-6]$'}|Sort-Object name|Format-Table
 $headings=New-Object System.Collections.Generic.List[object]
 foreach($p in $doc.Paragraphs){
  if($p.Style.NameLocal -match '^TDC 제목 [1-6]$'){
   $level=$p.Range.ListFormat.ListLevelNumber
   $headings.Add(@{text=$p.Range.Text.Trim([char]13,[char]7);style=$p.Style.NameLocal;left_pt=$p.Format.LeftIndent;first_pt=$p.Format.FirstLineIndent;number_pt=$p.Range.ListFormat.ListTemplate.ListLevels.Item($level).NumberPosition;number=$p.Range.ListFormat.ListString})
  }
 }
 $headings|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $qa 'native-heading-positions.json') -Encoding utf8
 Write-Output ('Effective heading positions checked='+$headings.Count)
 $doc.Close(0);$doc=$null
}finally{if($null -ne $doc){$doc.Close(0)};$app.Quit(0);$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($app)}
