param([string]$TaskRoot='E:\workspace\projects\sound1-fw-e8300-backup\docs\tasks\인허가\20261012_2102_QCC-설계문서-반영',[string]$ProofRelative='qcc_design\final-layout\proof.json',[string]$ReadyName='final-design-field-ready.json',[string[]]$Codes=@('SAD','SDDD','SIP'))
$ErrorActionPreference='Stop'
$qa=Join-Path $TaskRoot 'qa\review-20261013'
$python='C:\Users\EUNSU\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
$proof=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qa $ProofRelative) | ConvertFrom-Json
$proofV2=$null
if($ProofRelative -eq 'qcc_design\final-layout\proof.json'){$proofV2=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qa 'qcc_design\final-layout-v2\proof.json') | ConvertFrom-Json}
$records=@()
foreach($code in $Codes){
 $item=$proof.$code
 if($code -eq 'SDDD' -and $proofV2){$item=$proofV2.SDDD}
 if(-not $item.source){throw "Missing source for $code"}
 if((Get-FileHash -Algorithm SHA256 -LiteralPath $item.source).Hash -ne $item.before_sha256){throw "G source changed: $code"}
 if((Get-FileHash -Algorithm SHA256 -LiteralPath $item.candidate).Hash -ne $item.candidate_sha256){throw "Candidate changed: $code"}
 $dest=Join-Path (Join-Path $qa 'fields-candidate') ([IO.Path]::GetFileName($item.candidate))
 Write-Output "$code native field refresh"
 if($code -eq 'SAD'){
  & (Join-Path $TaskRoot 'refresh_candidate.ps1') -Source $item.candidate -Candidate $dest -KeepTocEntries
 }else{
  & (Join-Path $TaskRoot 'refresh_candidate.ps1') -Source $item.candidate -Candidate $dest
 }
 $comparison=Join-Path $qa "$code-final-layout-fields-comparison.json"
 if($ProofRelative -ne 'qcc_design\final-layout\proof.json'){$comparison=Join-Path $qa "$code-final-mic-fields-comparison.json"}
 & $python (Join-Path $TaskRoot 'compare_word_candidate.py') $item.candidate $dest $comparison
 if($LASTEXITCODE -ne 0){
  & $python (Join-Path $qa 'restore_native_downsampling.py') $comparison
  if($LASTEXITCODE -ne 0){throw "Undiagnosed native change: $code"}
  & $python (Join-Path $TaskRoot 'compare_word_candidate.py') $item.candidate $dest $comparison
  if($LASTEXITCODE -ne 0){throw "Native comparison failed: $code"}
 }
 $records+= [pscustomobject]@{code=$code;source=$item.source;beforeSha256=$item.before_sha256;candidate=$dest;candidateSha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $dest).Hash;comparison=$comparison}
}
$records | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $qa $ReadyName)
