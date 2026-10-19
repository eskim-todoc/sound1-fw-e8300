param([switch]$Render,[switch]$Recheck,[switch]$RenderOnly,[string]$PlanFile='plan.json',[string]$ResultsFile='native-check.json')
$ErrorActionPreference='Stop'
$qaPath=Join-Path $PSScriptRoot 'qa/dhf-materials-system-20261012'
$plan=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qaPath $PlanFile)|ConvertFrom-Json
$checks=New-Object System.Collections.Generic.List[object]
$cache=@{}
$prior=@{}
if($Recheck) {foreach($x in (Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qaPath $ResultsFile)|ConvertFrom-Json)) {$prior[$x.code+'|'+$x.sheet+'|'+$x.cell]=$x.needed}}
$excelInstance=$null
$candidateBook=$null
try {
    $excelInstance=New-Object -ComObject Excel.Application
    $excelInstance.Visible=$false
    $excelInstance.DisplayAlerts=$false
    $excelInstance.AskToUpdateLinks=$false
    $excelInstance.AutomationSecurity=3
    foreach($item in $plan.books) {
        $candidateBook=$excelInstance.Workbooks.Open($item.candidate,0,$true)
        $scratch=$candidateBook.Worksheets.Add()
        $targets=@{}
        foreach($e in $item.edits) {$targets[$e.sheet+'|'+$e.cell]=$e}
        foreach($e in $item.styles) {if($e.notice -and -not $targets.ContainsKey($e.sheet+'|'+$e.cell)) {$targets[$e.sheet+'|'+$e.cell]=$e}}
        if($RenderOnly){$targets=@{}}
        $count=0
        foreach($edit in $targets.Values) {
            $sheet=$candidateBook.Worksheets.Item($edit.sheet)
            $cell=$sheet.Range($edit.cell)
            $value=[string]$cell.Value2
            if($null -ne $edit.after -and $value -cne [string]$edit.after) {throw "Cell value differs: $($item.code)/$($edit.sheet)/$($edit.cell)"}
            $area=$cell.MergeArea
            $key=$value+'|'+[string]$area.Width+'|'+[string]$cell.Font.Name+'|'+[string]$cell.Font.Size
            if($Recheck -and $prior.ContainsKey($item.code+'|'+$edit.sheet+'|'+$edit.cell)) {$needed=$prior[$item.code+'|'+$edit.sheet+'|'+$edit.cell]}
            elseif($cache.ContainsKey($key)) {$needed=$cache[$key]} else {
                [void]$scratch.Cells.Clear()
                [void]$cell.Copy($scratch.Range('A1'))
                [void]$scratch.UsedRange.UnMerge()
                $scratch.Columns.Item(1).ColumnWidth=[double]$sheet.Columns.Item($cell.Column).ColumnWidth
                for($n=0;$n -lt 3;$n++) {
                    $units=[double]$scratch.Columns.Item(1).ColumnWidth
                    $scratch.Columns.Item(1).ColumnWidth=[double][Math]::Min(255,($units*[double]$area.Width/[double]$scratch.Columns.Item(1).Width))
                }
                [void]$scratch.Rows.Item(1).AutoFit()
                $needed=[double]$scratch.Rows.Item(1).RowHeight
                $cache[$key]=$needed
            }
            $current=[double]$area.Height
            $rows=[int]$area.Rows.Count
            $checks.Add([pscustomobject]@{code=$item.code;sheet=$edit.sheet;cell=$edit.cell;start_row=[int]$area.Row;end_row=([int]$area.Row+$rows-1);needed=$needed;current=$current;row_needed=[Math]::Ceiling(($needed+3)/$rows*4)/4;fits=($needed -le $current+1);limit=($needed -ge 409.5)})
            foreach($prefix in @('[E8300 보완]','[QCC 자료 입력]','[QCC/E8300 자료 입력]')) {
                $at=$value.IndexOf($prefix)
                if($at -lt 0) {continue}
                $font=$cell.Characters($at+1,$prefix.Length).Font
                $expected=if($prefix -eq '[QCC 자료 입력]'){192}elseif($prefix -eq '[E8300 보완]'){12611584}else{10498160}
                if($font.Color -ne $expected -or $font.Underline -ne 2 -or $font.Size -ne 12) {throw "Notice style differs: $($item.code)/$($edit.cell) $prefix color=$($font.Color) size=$($font.Size) underline=$($font.Underline)"}
            }
            $count++
            if($count%300 -eq 0){Write-Output ($item.code+' measured '+$count)}
        }
        [void]$scratch.Delete()
        if($Render) {
            $regions=switch($item.code){
                'SRS' {@('F49:H50','F60:H62')}
                'SUVC' {@('C19:D19','D30:G30')}
                'SUVR' {@('C19:D19','K811:M811')}
                'SITC' {@('C19:D19','M25:M26')}
                'SITR' {@('C19:D19','M25:O26')}
                'SSTC' {@('I86:J87','I217:J218','B32:F35')}
                'SSTR' {@('J217:L218','J168:L169','J233:L233','B32:F35')}
            }
            $sheet=$candidateBook.Worksheets.Item($item.main)
            if($PlanFile -eq 'support-plan.json'){$sheet=$candidateBook.Worksheets.Item('시험 보조자료');$regions=@('B61:D63','B164:D166')}
            $sheet.PageSetup.Orientation=2;$sheet.PageSetup.PaperSize=8
            $sheet.PageSetup.Zoom=$false;$sheet.PageSetup.FitToPagesWide=1;$sheet.PageSetup.FitToPagesTall=$false
            $sheet.PageSetup.PrintTitleRows='';$sheet.PageSetup.PrintTitleColumns=''
            $sheet.PageSetup.LeftMargin=18;$sheet.PageSetup.RightMargin=18;$sheet.PageSetup.TopMargin=18;$sheet.PageSetup.BottomMargin=18
            $sheet.PageSetup.LeftHeader='';$sheet.PageSetup.CenterHeader='';$sheet.PageSetup.RightHeader=''
            $sheet.PageSetup.LeftFooter='';$sheet.PageSetup.CenterFooter='';$sheet.PageSetup.RightFooter=''
            foreach($region in $regions){
                $selection=$sheet.Range($region)
                $r1=$selection.Row;$r2=$r1+$selection.Rows.Count-1;$c1=$selection.Column;$c2=$c1+$selection.Columns.Count-1
                foreach($cell in $selection.Cells){
                    $merged=$cell.MergeArea
                    $r1=[Math]::Min($r1,$merged.Row);$r2=[Math]::Max($r2,$merged.Row+$merged.Rows.Count-1)
                    $c1=[Math]::Min($c1,$merged.Column);$c2=[Math]::Max($c2,$merged.Column+$merged.Columns.Count-1)
                }
                $sheet.PageSetup.PrintArea=$sheet.Range($sheet.Cells.Item($r1,$c1),$sheet.Cells.Item($r2,$c2)).Address()
                $sheet.ExportAsFixedFormat(0,(Join-Path $qaPath ($item.code+'-'+$region.Replace(':','_')+'.pdf')),0,$false,$false)
            }
        }
        $candidateBook.Close($false)
        [void][Runtime.InteropServices.Marshal]::ReleaseComObject($candidateBook);$candidateBook=$null
        Write-Output ($item.code+' native check complete')
        if(-not $RenderOnly){$checks|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 -LiteralPath (Join-Path $qaPath $ResultsFile)}
    }
    Write-Output ('Checked='+$checks.Count+'; undersized='+@($checks|Where-Object {-not $_.fits}).Count+'; limit='+@($checks|Where-Object {$_.limit}).Count)
} finally {
    if($null -ne $candidateBook){$candidateBook.Close($false);[void][Runtime.InteropServices.Marshal]::ReleaseComObject($candidateBook)}
    if($null -ne $excelInstance){$excelInstance.Quit();[void][Runtime.InteropServices.Marshal]::ReleaseComObject($excelInstance)}
}
