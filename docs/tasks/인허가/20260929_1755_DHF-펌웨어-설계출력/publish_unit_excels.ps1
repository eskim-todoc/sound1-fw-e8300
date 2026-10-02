$ErrorActionPreference='Stop'
$qa=Join-Path $PSScriptRoot 'qa\unit-reconciliation'
$out=Join-Path (Get-Location) 'docs\참고\인허가\사운드1\DHF\03_설계 출력\SW\펌웨어'
$previous=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qa 'final-checks.json') | ConvertFrom-Json
$checked=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qa 'candidate-checks.json') | ConvertFrom-Json
$files=@(
 @{key='SUVC';source='SUVC-candidate.xlsx';name='SUVC-FW-TD24A 소프트웨어 유닛 검증 기준서.xlsx'},
 @{key='SITC';source='SITC-candidate.xlsx';name='SITC-FW-TD24A 소프트웨어 통합시험 기준서.xlsx'}
)
if((Get-FileHash -LiteralPath (Join-Path $out 'SDDD-FW-TD24A 소프트웨어 상세 설계서.docx')).Hash -ne $checked.sha256.SDDD){throw 'SDDD differs from the verified candidate.'}
foreach($f in $files){
 if((Get-FileHash -LiteralPath (Join-Path $out $f.name)).Hash -ne $previous.sha256.($f.key)){throw ('External change: '+$f.name)}
 if((Get-FileHash -LiteralPath (Join-Path $qa $f.source)).Hash -ne $checked.sha256.($f.key)){throw ('Unverified candidate: '+$f.source)}
}
foreach($f in $files){
 Copy-Item -LiteralPath (Join-Path $qa $f.source) -Destination (Join-Path $out $f.name) -Force
 if((Get-FileHash -LiteralPath (Join-Path $out $f.name)).Hash -ne $checked.sha256.($f.key)){throw ('Copy verification failed: '+$f.name)}
 Write-Output ('Published verified file: '+$f.name)
}
