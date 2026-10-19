param([string]$Sdk = 'C:/Program Files (x86)/ON Semiconductor/Ezairo 8300 SDK')
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$out = Join-Path $root 'tmp/unit-tests/link-pmic-cfx/asip'
$null = New-Item -ItemType Directory -Path $out -Force
$compiler = Join-Path $Sdk 'ASIP Programmer/U-2023.06-SP1/win64/bin/WINbin/chesscc.exe'
$includes = @()
foreach ($dir in @('include/shared', 'include/cfx', 'cf624c-a20_vU-2023.06/lib/runtime/include')) {
    $includes += '-I' + (Join-Path $Sdk $dir)
}
foreach ($dir in @('systemControl', 'environment', 'hear', 'signalProcessing', 'internalDevice', 'lib_cfx', 'OTE_1_5_gen', 'test')) {
    $includes += '-I' + (Join-Path $root "src/1__cfx/$dir")
}
$source = Join-Path $root 'src/1__cfx/internalDevice/driver_PCM_liveStimulation.c'
$results = @()
foreach ($mode in @('normal', 'test')) {
    $arguments = @('-p', 'cf624c', '-P', (Join-Path $Sdk 'cf624c-a20_vU-2023.06/lib'), '-c')
    if ($mode -eq 'test') { $arguments += '-DTDC_UNIT_TEST' }
    $arguments += @('-DSK5_CID=101', '-DHARDWARE=0', '-DISS=1', '-DTARGET=HARDWARE', '-DCFX_CORE=1')
    $arguments += $includes
    $arguments += @('-o', (Join-Path $out "$mode.o"), $source)
    & $compiler @arguments
    if ($LASTEXITCODE -ne 0) { throw "CFX PCM live $mode compilation failed: $LASTEXITCODE" }
    $results += [PSCustomObject]@{
        mode = $mode; command = @($compiler) + $arguments
        source_sha256 = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
        exit_code = $LASTEXITCODE
    }
    Write-Output "CFX PCM live $mode compile PASS"
}
$results | ConvertTo-Json -Depth 4 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $out 'checks.json')
