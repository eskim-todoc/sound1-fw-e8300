$ErrorActionPreference = 'Stop'
$qa = Join-Path $PSScriptRoot 'qa/evidence-positive-wording'
$base = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qa 'baseline.json') | ConvertFrom-Json
$plan = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qa 'plan.json') | ConvertFrom-Json
$excel = $null; $before = $null; $after = $null; $results = @()
try {
    # 사용자 Excel 대신 별도 숨김 인스턴스에서 읽기 전용으로 저장값/폰트를 확인한다.
    $excel = New-Object -ComObject Excel.Application
    $excel.Visible = $false; $excel.DisplayAlerts = $false
    $excel.AskToUpdateLinks = $false; $excel.AutomationSecurity = 3
    foreach ($b in $base.books) {
        $before = $excel.Workbooks.Open($b.backup, 0, $true)
        $candidate = Join-Path $qa ($b.key + '-candidate.xlsx')
        $after = $excel.Workbooks.Open($candidate, 0, $true)
        $checked = 0
        $samples = @($plan.changes | Where-Object { $_.key -eq $b.key -and $_.value -ne '' -and $_.sheet -eq $b.sheet -and ($_.sourceCells -or $_.cell -in @('J54','J161','J901','J1666','D30','K191')) })
        $samples += @($plan.changes | Where-Object { $_.key -eq $b.key -and $_.value -ne '' } | Select-Object -First 10)
        foreach ($p in @($samples | Sort-Object sheet,cell -Unique)) {
            $a = $before.Worksheets.Item($p.sheet).Range($p.cell)
            $c = $after.Worksheets.Item($p.sheet).Range($p.cell)
            $value = ([string]$c.Value2).Replace("`r`n", "`n")
            if ($value -cne $p.value) { throw ('Saved value differs: ' + $p.cell) }
            $oldText = [string]$a.Value2
            $oldStart = [Math]::Max(1, $oldText.Length - $oldText.TrimStart().Length + 1)
            $newStart = [Math]::Max(1, $value.Length - $value.TrimStart().Length + 1)
            foreach ($field in @('Name', 'Size', 'Bold', 'Italic')) {
                $oldFont = $a.Characters($oldStart,1).Font.$field
                $newFont = $c.Characters($newStart,1).Font.$field
                if ($oldFont -ne $newFont) { throw ('Font differs: ' + $p.cell + '/' + $field) }
            }
            $checked++
        }
        $merged = @($plan.merges | Where-Object { $_.key -eq $b.key -and $_.sheet -eq $b.sheet })
        foreach ($m in $merged) {
            if (-not $after.Worksheets.Item($m.sheet).Range($m.range).MergeCells) { throw ('Merge missing: ' + $m.range) }
        }
        $measure=$after.Worksheets.Add()
        $measure.Range('A1').WrapText=$true
        $main=$after.Worksheets.Item($b.sheet)
        $heightChecks=@()
        foreach ($addr in @('K567','K569','K571')) {
            $c=$main.Range($addr)
            $probe=$measure.Range('A1')
            $probe.Font.Name=$c.Characters(2,1).Font.Name
            $probe.Font.Size=$c.Characters(2,1).Font.Size
            $probe.Font.Bold=$c.Characters(2,1).Font.Bold
            $measure.Columns.Item(1).ColumnWidth=$main.Columns.Item($c.Column).ColumnWidth
            $lines=([string]$c.Value2) -split "`n"
            $needed=0.0
            for($i=0;$i -lt $lines.Count;$i+=8) {
                $end=[Math]::Min($i+7,$lines.Count-1)
                $probe.Value2=($lines[$i..$end] -join "`n")
                [void]$probe.EntireRow.AutoFit()
                $needed += [double]$probe.RowHeight
            }
            $heightChecks += [pscustomobject]@{cell=$addr; actual=$c.RowHeight; needed=$needed}
            if($needed -gt [double]$c.RowHeight+1){throw ('Height insufficient: '+$addr+' / '+$needed)}
        }
        [void]$measure.Delete()
        $results += [pscustomobject]@{key=$b.key; readOnly=$after.ReadOnly; checkedCells=$checked; mergedCellsChecked=$merged.Count; heightChecks=$heightChecks; savedValuesAndFonts='PASS'}
        $after.Close($false); [void][Runtime.InteropServices.Marshal]::ReleaseComObject($after); $after=$null
        $before.Close($false); [void][Runtime.InteropServices.Marshal]::ReleaseComObject($before); $before=$null
    }
    $results | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $qa 'native-checks.json')
    $results | ConvertTo-Json -Depth 5
} finally {
    foreach ($book in @($after,$before)) {
        if ($null -ne $book) { $book.Close($false); [void][Runtime.InteropServices.Marshal]::ReleaseComObject($book) }
    }
    if ($null -ne $excel) { $excel.Quit(); [void][Runtime.InteropServices.Marshal]::ReleaseComObject($excel) }
}
