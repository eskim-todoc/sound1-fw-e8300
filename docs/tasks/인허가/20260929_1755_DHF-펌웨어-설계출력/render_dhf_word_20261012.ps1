$ErrorActionPreference='Stop'
$qaPath=Join-Path $PSScriptRoot 'qa/dhf-materials-system-20261012'
$plan=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qaPath 'plan.json')|ConvertFrom-Json
$app=New-Object -ComObject Word.Application
$app.Visible=$false;$app.DisplayAlerts=0;$app.AutomationSecurity=3
$records=New-Object System.Collections.Generic.List[object]
try {
 foreach($item in $plan.docs) {
  foreach($phase in @('before','candidate')) {
   $doc=$null
   try {
    Write-Output ($item.code+' '+$phase+' opening')
    $doc=$app.Documents.Open($item.$phase,$false,$true,$false)
    # Preview the final text in memory; never accept revisions in the deliverable.
    $doc.Revisions.AcceptAll()
    $doc.Repaginate()
    Write-Output ($item.code+' '+$phase+' paginated')
    $pages=New-Object 'System.Collections.Generic.HashSet[int]'
    foreach($paragraph in $doc.Paragraphs) {
     $range=$paragraph.Range
     if([string]$range.Text -match '\[(QCC 자료 입력|자료 추가 위치|E8300 보완|QCC/E8300 자료 입력)\]') {
      [void]$pages.Add([int]$range.Information(3))
     }
    }
    $selected=@($pages|Sort-Object)
    if($phase -eq 'before') {$selected=@($selected|Select-Object -First 1)}
    foreach($page in $selected) {
     $pdf=Join-Path $qaPath ($item.code+'-'+$phase+'-page'+$page+'.pdf')
     $doc.ExportAsFixedFormat($pdf,17,$false,0,3,$page,$page,0)
     $records.Add([pscustomobject]@{code=$item.code;phase=$phase;page=$page;pdf=$pdf})
    }
    Write-Output ($item.code+' '+$phase+' preview pages='+$selected.Count)
   } finally {if($null -ne $doc){$doc.Close(0);[void][Runtime.InteropServices.Marshal]::ReleaseComObject($doc)}}
  }
 }
 $records|ConvertTo-Json -Depth 5|Set-Content -Encoding UTF8 -LiteralPath (Join-Path $qaPath 'word-render.json')
} finally {$app.Quit(0);[void][Runtime.InteropServices.Marshal]::ReleaseComObject($app)}
