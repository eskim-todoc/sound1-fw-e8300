$ErrorActionPreference='Stop'
$qaPath=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot 'qa/dhf-materials-system-20261012'))
$localRoot=[IO.Path]::GetFullPath('E:\workspace\projects\sound1-fw-e8300-backup\docs\참고\인허가\사운드1\DHF')
$newRoot=[IO.Path]::GetFullPath('G:\공유 드라이브\20_프로젝트별\01_project sound1\08_DHF')
$plan=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qaPath 'cleanup-plan.json')|ConvertFrom-Json
$published=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qaPath 'published.json')|ConvertFrom-Json
foreach($p in $published){if((Get-FileHash -Algorithm SHA256 -LiteralPath $p.path).Hash.ToLowerInvariant() -ne $p.hash){throw "Published file differs: $($p.path)"}}
foreach($item in $plan) {
 $source=[IO.Path]::GetFullPath($item.source);$dest=[IO.Path]::GetFullPath($item.destination)
 $expectedRoot=if($item.kind -eq 'local-document'){$localRoot}else{$newRoot}
 if(-not $source.StartsWith($expectedRoot+'\',[StringComparison]::OrdinalIgnoreCase)){throw "Source outside approved folder: $source"}
 if(-not $dest.StartsWith($qaPath+'\',[StringComparison]::OrdinalIgnoreCase)){throw "Recovery destination outside QA: $dest"}
 if(Test-Path -LiteralPath $dest){throw "Recovery file exists: $dest"}
 if(-not (Test-Path -LiteralPath $item.current)){throw "Current copy missing: $($item.current)"}
 if((Get-FileHash -Algorithm SHA256 -LiteralPath $source).Hash.ToLowerInvariant() -ne $item.source_hash){throw "Source changed: $source"}
 if($item.kind -eq 'new-supplement' -and (Get-FileHash -Algorithm SHA256 -LiteralPath $item.current).Hash.ToLowerInvariant() -ne $item.source_hash){throw 'Local supplement differs'}
}
$done=New-Object System.Collections.Generic.List[object]
foreach($item in $plan) {
 [void](New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($item.destination)) -Force)
 Move-Item -LiteralPath $item.source -Destination $item.destination
 if((Get-FileHash -Algorithm SHA256 -LiteralPath $item.destination).Hash.ToLowerInvariant() -ne $item.source_hash){throw 'Recovery hash differs'}
 $done.Add($item)
 $done|ConvertTo-Json -Depth 5|Set-Content -Encoding UTF8 -LiteralPath (Join-Path $qaPath 'cleanup-complete.json')
}
foreach($folder in @($plan|Where-Object {$_.kind -eq 'new-supplement'}|ForEach-Object {[IO.Path]::GetDirectoryName($_.source)}|Sort-Object -Unique)) {
 $resolved=[IO.Path]::GetFullPath($folder)
 if($resolved.StartsWith($newRoot+'\',[StringComparison]::OrdinalIgnoreCase) -and [IO.Path]::GetFileName($resolved) -eq '이미지 참조' -and @(Get-ChildItem -LiteralPath $resolved -Force).Count -eq 0){Remove-Item -LiteralPath $resolved}
}
Write-Output ('Recoverably organized: '+$done.Count)
