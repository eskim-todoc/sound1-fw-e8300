param([ValidateSet('measure','verify')][string]$Mode='measure')
$ErrorActionPreference='Stop'
$qa=Join-Path $PSScriptRoot 'qa/unit-height-limit-split'
$base=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qa 'baseline.json') | ConvertFrom-Json
$plan=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qa 'plan.json') | ConvertFrom-Json
$excel=$null;$book=$null;$results=[Collections.Generic.List[object]]::new();$oppositeChecks=0
try {
    $excel=New-Object -ComObject Excel.Application
    $excel.Visible=$false;$excel.DisplayAlerts=$false;$excel.AskToUpdateLinks=$false;$excel.AutomationSecurity=3
    foreach($b in $base.books) {
        $path=if($Mode -eq 'measure'){$b.authorInput}else{Join-Path $qa ($b.key+'-candidate.xlsx')}
        $book=$excel.Workbooks.Open($path,0,$true)
        $scratch=$book.Worksheets.Add();$probe=$scratch.Range('A1')
        foreach($p in @($plan.changes | Where-Object key -eq $b.key)) {
            $s=$book.Worksheets.Item($p.sheet)
            $source=$s.Range($(if($Mode -eq 'measure'){$p.sourceCell}else{$p.cell}))
            [void]$probe.Clear()
            if($Mode -eq 'verify') {
                if($source.MergeCells){throw ('Split cell still merged: '+$p.cell)}
                # 글자별 굵기/색을 포함해 실제 셀을 복사하고 같은 글꼴로 측정한다.
                [void]$source.Copy($probe)
            } else {
                # 문자열 잘라내기 대신 원본 XML의 글자별 글꼴을 조각에 적용해 측정한다.
                $probe.Value2=[string]$p.value
                foreach($run in $p.fontRuns) {
                    $font=$probe.Characters([int]$run.start,[int]$run.length).Font
                    $font.Name=[string]$run.font.name;$font.Size=[double]$run.font.size
                    $font.Bold=[bool]$run.font.bold;$font.Italic=[bool]$run.font.italic
                }
            }
            $col=$source.Column;$scratch.Columns.Item(1).ColumnWidth=$s.Columns.Item($col).ColumnWidth
            $probe.WrapText=$true;$probe.ShrinkToFit=$false
            [void]$probe.EntireRow.AutoFit()
            $height=[double]$probe.RowHeight
            if($height -ge 409.5){throw ('Fragment reaches clipping limit: '+$p.key+'/'+$p.cell)}
            if($Mode -eq 'verify') {
                if(([string]$source.Value2).Replace("`r`n","`n") -cne $p.value){throw ('Saved text differs: '+$p.cell)}
                if($height -gt [double]$source.RowHeight+0.1){throw ('Saved height too small: '+$p.cell+'/'+$height+'/'+$source.RowHeight)}
                # 사용자처럼 실제 시험 시트를 AutoFit 해도 상한에 걸리지 않는지 확인한다.
                [void]$source.EntireRow.AutoFit()
                if([double]$source.RowHeight -ge 409.5){throw ('Native AutoFit clips: '+$p.cell)}
            }
            $results.Add([pscustomobject]@{key=$p.key;sheet=$p.sheet;cell=$p.cell;row=[int]($p.cell -replace '\D','');height=$height})
        }
        if($Mode -eq 'verify' -and $b.key -eq 'unit') {
            $groups=@($plan.groups | Where-Object {$_.key -eq $b.key -and $_.sheet -eq $b.sheet})
            foreach($g in $groups) {
                $otherCol=if($g.col -eq 11){12}else{11}
                if(@($groups | Where-Object {$_.start -eq $g.start -and $_.col -eq $otherCol}).Count -gt 0){continue}
                $s=$book.Worksheets.Item($b.sheet);$source=$s.Cells.Item([int]$g.start,$otherCol)
                if(-not $source.MergeCells){throw ('Short opposite side not merged: '+$source.Address())}
                [void]$probe.Clear();$v=[string]$source.Value2;$ix=[Math]::Max(1,$v.Length-$v.TrimStart().Length+1)
                $probe.Font.Name=$source.Characters($ix,1).Font.Name
                $probe.Font.Size=$source.Characters($ix,1).Font.Size
                $probe.Font.Bold=$source.Characters($ix,1).Font.Bold
                $scratch.Columns.Item(1).ColumnWidth=$s.Columns.Item($otherCol).ColumnWidth
                $probe.Value2=$v;$probe.WrapText=$true;[void]$probe.EntireRow.AutoFit()
                if([double]$probe.RowHeight -gt [double]$source.MergeArea.Height+0.1){throw ('Short opposite side clips: '+$source.Address())}
                $oppositeChecks++
            }
        }
        $book.Close($false);[void][Runtime.InteropServices.Marshal]::ReleaseComObject($book);$book=$null
        Write-Output ($b.key+' '+$Mode+' done')
    }
    if($Mode -eq 'measure') {
        $results.ToArray() | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $qa 'fragment-heights.json')
    } else {
        [pscustomobject]@{status='PASS';checkedFragments=$results.Count;shortOppositeChecks=$oppositeChecks;nativeAutoFit='PASS';maximumHeight=($results | Measure-Object height -Maximum).Maximum;cells=$results.ToArray()} | ConvertTo-Json -Depth 7 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $qa 'native-checks.json')
    }
    Write-Output ('Checked '+$results.Count)
} finally {
    if($null -ne $book){$book.Close($false);[void][Runtime.InteropServices.Marshal]::ReleaseComObject($book)}
    if($null -ne $excel){$excel.Quit();[void][Runtime.InteropServices.Marshal]::ReleaseComObject($excel)}
}
