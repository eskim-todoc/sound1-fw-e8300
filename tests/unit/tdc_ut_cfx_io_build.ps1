param(
    [string]$Sdk = 'C:/Program Files (x86)/ON Semiconductor/Ezairo 8300 SDK'
)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$out = Join-Path $root 'tmp/unit-tests/cfx-io/asip'
$null = New-Item -ItemType Directory -Path $out -Force
$compiler = Join-Path $Sdk 'ASIP Programmer/U-2023.06-SP1/win64/bin/WINbin/chesscc.exe'
$includes = @()
foreach ($dir in @('include/shared', 'include/cfx', 'cf624c-a20_vU-2023.06/lib/runtime/include')) {
    $includes += '-I' + (Join-Path $Sdk $dir)
}
foreach ($dir in @('systemControl', 'environment', 'hear', 'signalProcessing', 'internalDevice', 'lib_cfx', 'OTE_1_5_gen', 'test')) {
    $includes += '-I' + (Join-Path $root "src/1__cfx/$dir")
}
$sources = @('test/tdc_ut_io_cfx.c', 'systemControl/main.c', 'systemControl/system_control.c',
    'signalProcessing/FrequencyAnalysis.c', 'signalProcessing/stimulationStrategy.c',
    'internalDevice/driver_PCM.c', 'lib_cfx/lib_i2s.c')
$results = @()
foreach ($mode in @('test', 'normal')) {
    foreach ($source in $sources) {
        $path = Join-Path $root "src/1__cfx/$source"
        $object = Join-Path $out ($mode + '_' + [IO.Path]::GetFileNameWithoutExtension($source) + '.o')
        $arguments = @('-p', 'cf624c', '-P', (Join-Path $Sdk 'cf624c-a20_vU-2023.06/lib'), '-c')
        if ($mode -eq 'test') { $arguments += '-DTDC_UNIT_TEST' }
        $arguments += @('-DSK5_CID=101', '-DHARDWARE=0', '-DISS=1', '-DTARGET=HARDWARE', '-DCFX_CORE=1')
        $arguments += $includes
        $arguments += @('-o', $object, $path)
        & $compiler @arguments
        $code = $LASTEXITCODE
        $results += [PSCustomObject]@{
            mode = $mode; source = $source; command = @($compiler) + $arguments
            exit_code = $code; source_sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
        }
        Write-Output "$mode $source exit=$code"
        if ($code -ne 0) { throw "ASIP compilation failed: $mode $source ($code)" }
    }
}
# 빌드 산출물의 자동 기록. 중앙 작업/기준서 파일은 수정하지 않는다.
$results | ConvertTo-Json -Depth 4 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $out 'checks.json')
