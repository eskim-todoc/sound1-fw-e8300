param([Parameter(Mandatory=$true)][string]$PlanPath)
$ErrorActionPreference='Stop'
$plan=Get-Content -Raw -Encoding UTF8 -LiteralPath $PlanPath | ConvertFrom-Json
$records=New-Object System.Collections.Generic.List[object]
$app=New-Object -ComObject Word.Application
$app.Visible=$false
$app.DisplayAlerts=0
$app.AutomationSecurity=3
try {
 foreach($item in $plan.docs) {
  $doc=$null
  try {
   Write-Output ($item.code+' opening')
   $sourceHash=(Get-FileHash -Algorithm SHA256 -LiteralPath ([string]$item.path)).Hash
   $doc=$app.Documents.Open([string]$item.path,$false,$true,$false)
   # Accept only in this read-only in-memory preview. Deliverable revisions remain.
   $revisionCount=$doc.Revisions.Count
   $doc.Revisions.AcceptAll()
   if($item.nativeAcceptedXML){
    [IO.File]::WriteAllText([string]$item.nativeAcceptedXML,[string]$doc.Content.WordOpenXML,[Text.UTF8Encoding]::new($false))
   }
   $doc.ShowRevisions=$false
   $doc.Repaginate()
   $pageCount=$doc.ComputeStatistics(2)
   $doc.ExportAsFixedFormat([string]$item.pdf,17,$false,0,0,1,$pageCount,0)
   $afterHash=(Get-FileHash -Algorithm SHA256 -LiteralPath ([string]$item.path)).Hash
   if($sourceHash -ne $afterHash){throw ($item.code+' source changed during read-only render')}
   $records.Add([pscustomobject]@{code=$item.code;path=$item.path;pdf=$item.pdf;pages=$pageCount;revisions=$revisionCount;sourceSHA256=$sourceHash;sourceUnchanged=$true;nativeAcceptedXML=$item.nativeAcceptedXML;scope='Read-only in-memory accepted preview; deliverable revisions preserved';checkedAt=(Get-Date).ToString('o')})
   Write-Output ($item.code+' exported '+$pageCount+' pages')
  } finally {
   if($null -ne $doc){$doc.Close(0);[void][Runtime.InteropServices.Marshal]::ReleaseComObject($doc)}
  }
 }
 $records | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 -LiteralPath $plan.recordPath
} finally {$app.Quit(0);[void][Runtime.InteropServices.Marshal]::ReleaseComObject($app)}
