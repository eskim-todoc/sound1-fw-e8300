$ErrorActionPreference='Stop'
$qa=Join-Path $PSScriptRoot 'qa/unit-height-limit-split'
$base=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qa 'baseline.json') | ConvertFrom-Json
$jobs=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qa 'height-jobs.json') | ConvertFrom-Json
$excel=$null;$book=$null;$results=[Collections.Generic.List[object]]::new()
try {
    $excel=New-Object -ComObject Excel.Application
    $excel.Visible=$false;$excel.DisplayAlerts=$false;$excel.AskToUpdateLinks=$false;$excel.AutomationSecurity=3
    foreach($b in $base.books) {
        $book=$excel.Workbooks.Open($b.backup,0,$true)
        $scratch=$book.Worksheets.Add();$probe=$scratch.Range('A1')
        $probe.WrapText=$true;$probe.ShrinkToFit=$false
        $n=0
        foreach($p in @($jobs | Where-Object key -eq $b.key)) {
            $s=$book.Worksheets.Item($p.sheet);$source=$s.Range($p.cell)
            $text=[string]$p.text
            $ix=[Math]::Max(1,$text.Length-$text.TrimStart().Length+1)
            $probe.Font.Name=$source.Characters($ix,1).Font.Name
            $probe.Font.Size=$source.Characters($ix,1).Font.Size
            $probe.Font.Bold=$source.Characters($ix,1).Font.Bold
            $scratch.Columns.Item(1).ColumnWidth=$s.Columns.Item([int]$p.col).ColumnWidth
            $probe.Value2=$text;[void]$probe.EntireRow.AutoFit()
            $whole=[double]$probe.RowHeight
            $fragments=[Collections.Generic.List[object]]::new()
            if($whole -ge 409.5) {
                # Excel AutoFit은 상한에서 멈춘다. 실제 초과 여부와 분할 지점을
                # 원래 줄 단위로 확인한다. 여유 9pt를 두어 마지막 줄 잘림을 막는다.
                $lines=$text -split "`n";$first=0;$pos=0
                while($first -lt $lines.Count) {
                    $lo=$first;$hi=$lines.Count-1;$last=$first-1;$height=0.0
                    while($lo -le $hi) {
                        $mid=[int][Math]::Floor(($lo+$hi)/2)
                        $part=$lines[$first..$mid] -join "`n"
                        $probe.Value2=$part;[void]$probe.EntireRow.AutoFit()
                        $h=[double]$probe.RowHeight
                        if($h -le 400.5) {$last=$mid;$height=$h;$lo=$mid+1} else {$hi=$mid-1}
                    }
                    if($last -lt $first) {throw ('Single text line exceeds limit: '+$p.key+'/'+$p.cell)}
                    # 분할 경계의 빈 줄은 윗칸에 남기지 않는다.
                    while($last -gt $first -and $last -lt $lines.Count-1 -and $lines[$last] -eq '') {$last--}
                    $part=$lines[$first..$last] -join "`n"
                    $probe.Value2=$part;[void]$probe.EntireRow.AutoFit();$height=[double]$probe.RowHeight
                    $fragments.Add([pscustomobject]@{text=$part;offset=$pos;height=$height})
                    $pos+=$part.Length+1;$first=$last+1
                }
            } else {$fragments.Add([pscustomobject]@{text=$text;offset=0;height=$whole})}
            $results.Add([pscustomobject]@{key=$p.key;sheet=$p.sheet;cell=$p.cell;start=$p.start;end=$p.end;col=$p.col;wholeHeight=$whole;parts=@($fragments.ToArray())})
            $n++;if($n%100 -eq 0){Write-Output ($b.key+' measured '+$n)}
        }
        $book.Close($false);[void][Runtime.InteropServices.Marshal]::ReleaseComObject($book);$book=$null
    }
    $results.ToArray() | ConvertTo-Json -Depth 7 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $qa 'measurements.json')
    Write-Output ('Measurements '+$results.Count+'; split '+@($results | Where-Object {$_.parts.Count -gt 1}).Count)
} finally {
    if($null -ne $book){$book.Close($false);[void][Runtime.InteropServices.Marshal]::ReleaseComObject($book)}
    if($null -ne $excel){$excel.Quit();[void][Runtime.InteropServices.Marshal]::ReleaseComObject($excel)}
}
