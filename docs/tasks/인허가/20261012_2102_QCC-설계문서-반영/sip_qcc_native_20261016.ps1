param([switch]$Fast,[switch]$RejectAll)
$ErrorActionPreference='Stop'
$qaPath=Join-Path $PSScriptRoot 'qa/sip-qcc-cleanup-20261016'
$records=New-Object System.Collections.Generic.List[object]
$word=$null
try {
  $word=New-Object -ComObject Word.Application
  $word.Visible=$false
  $word.DisplayAlerts=0
  $word.AutomationSecurity=3
  $word.ScreenUpdating=$false
  $phases=if($RejectAll){@('before-reject-all','candidate-reject-all')}elseif($Fast){@('before','candidate','reject-structured')}else{@('before','candidate','reject-native')}
  foreach($phase in $phases) {
    $doc=$null
    try {
      $inputName=if($phase.StartsWith('before')){'before.docx'}elseif($phase -eq 'reject-structured'){'reject-new.docx'}else{'candidate.docx'}
      $inputFile=Join-Path $qaPath $inputName
      $hashBefore=(Get-FileHash -Algorithm SHA256 -LiteralPath $inputFile).Hash
      Write-Output "$phase opening"
      $doc=$word.Documents.Open($inputFile,$false,$true,$false)
      Write-Output "$phase opened"
      $doc.TrackRevisions=$false
      $revisionCount=if($Fast){-1}else{$doc.Revisions.Count}
      $newRevisions=New-Object System.Collections.Generic.List[object]
      $revisionStart=if($Fast){0}else{$doc.Revisions.Count}
      for($i=$revisionStart;$i -ge 1;$i--) {
        $rev=$doc.Revisions.Item($i)
        $date=[datetime]$rev.Date
        if($date.Year -eq 2026 -and $date.Month -eq 10 -and $date.Day -eq 16) {
          $newRevisions.Add([pscustomobject]@{index=$i;type=[int]$rev.Type;author=[string]$rev.Author;date=$date.ToString('s');text=[string]$rev.Range.Text})
          if([string]$rev.Author -ne '김은수'){throw 'Unexpected new revision author'}
          if($phase -eq 'reject-native'){$rev.Reject()}
        }
        [void][Runtime.InteropServices.Marshal]::ReleaseComObject($rev)
      }
      $afterReject=if($Fast){-1}else{$doc.Revisions.Count}
      Write-Output "$phase accepting"
      if($RejectAll){$doc.Revisions.RejectAll()}else{$doc.Revisions.AcceptAll()}
      Write-Output "$phase accepted"
      $doc.Repaginate()
      Write-Output "$phase paginated"
      $pdf=Join-Path $qaPath ($phase+'.pdf')
      $doc.ExportAsFixedFormat($pdf,17,$false,0,0,1,1,0)
      Write-Output "$phase exported"
      $bodyText=[string]$doc.Content.Text
      $bodyText | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $qaPath ($phase+'-word-text.txt'))
      $paragraphs=New-Object System.Collections.Generic.List[object]
      $index=0
      $paras=if($Fast){@()}else{$doc.Paragraphs}
      foreach($paragraph in $paras) {
        $range=$paragraph.Range
        $paragraphs.Add([pscustomobject]@{index=$index;text=[string]$range.Text;page=[int]$range.Information(3);font=[string]$range.Font.Name;size=[single]$range.Font.Size})
        $index++
      }
      $record=[pscustomobject]@{phase=$phase;input=$inputFile;sha256_before=$hashBefore;revisions_at_open=$revisionCount;new_revisions=$newRevisions.ToArray();revisions_after_reject=$afterReject;revisions_after_accept=$doc.Revisions.Count;pages=$doc.ComputeStatistics(2);tables=$doc.Tables.Count;inline_shapes=$doc.InlineShapes.Count;shapes=$doc.Shapes.Count;pdf=$pdf;paragraphs=$paragraphs.ToArray()}
      $records.Add($record)
      Write-Output ($phase+' pages='+$record.pages+' new_revisions='+$newRevisions.Count+' tables='+$record.tables)
    } finally {
      if($null -ne $doc){$doc.Close(0);[void][Runtime.InteropServices.Marshal]::ReleaseComObject($doc)}
      if((Get-FileHash -Algorithm SHA256 -LiteralPath $inputFile).Hash -ne $hashBefore){throw 'Read-only DOCX changed during preview'}
    }
  }
  $jsonName=if($RejectAll){'native-reject-all-validation.json'}elseif($Fast){'native-fast-validation.json'}else{'native-validation.json'}
  $records.ToArray() | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $qaPath $jsonName)
} finally {
  if($null -ne $word){$word.Quit(0);[void][Runtime.InteropServices.Marshal]::ReleaseComObject($word)}
}
