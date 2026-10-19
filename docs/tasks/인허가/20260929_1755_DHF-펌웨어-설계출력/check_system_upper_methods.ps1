param([switch]$Render)
$ErrorActionPreference='Stop'
$qaPath=Join-Path $PSScriptRoot 'qa/system-upper-method-alignment-20261012'
$plan=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qaPath 'plan.json')|ConvertFrom-Json
$checks=New-Object System.Collections.Generic.List[object]
$excelInstance=$null
$candidateBook=$null
try {
    # 사용자 Excel에 연결하지 않고 후보 사본만 별도 숨김 인스턴스로 읽는다.
    $excelInstance=New-Object -ComObject Excel.Application
    $excelInstance.Visible=$false
    $excelInstance.DisplayAlerts=$false
    $excelInstance.AskToUpdateLinks=$false
    $excelInstance.AutomationSecurity=3
    foreach($item in $plan.books) {
        $candidateBook=$excelInstance.Workbooks.Open($item.candidate,0,$true)
        if(-not $candidateBook.ReadOnly) {throw 'Candidate must be read-only'}
        $scratch=$candidateBook.Worksheets.Add()
        foreach($edit in $item.edits) {
            $sheet=$candidateBook.Worksheets.Item($edit.sheet)
            $cell=$sheet.Range($edit.cell)
            if([string]$cell.Value2 -cne [string]$edit.after) {throw "Cell value differs: $($item.code)/$($edit.sheet)/$($edit.cell)"}
            $area=$cell.MergeArea
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
            if($needed -ge 409.5) {throw "AutoFit reached Excel height limit: $($item.code)/$($edit.cell)"}
            $current=[double]$area.Height
            $rows=[int]$area.Rows.Count
            $checks.Add([pscustomobject]@{code=$item.code;sheet=$edit.sheet;cell=$edit.cell;is_main=($edit.sheet -eq $item.main);start_row=[int]$area.Row;end_row=([int]$area.Row+$rows-1);needed=$needed;current=$current;row_needed=[Math]::Ceiling(($needed+4)/$rows*4)/4;fits=($needed -le $current+1)})
            foreach($prefix in @('[E8300 보완]','[QCC 자료 입력]')) {
                $at=([string]$cell.Value2).IndexOf($prefix)
                if($at -lt 0) {continue}
                $font=$cell.Characters($at+1,$prefix.Length).Font
                if($prefix -eq '[E8300 보완]') {
                    if($font.Color -ne 12611584 -or -not $font.Bold) {throw "E8300 notice style differs: $($edit.cell)"}
                } else {
                    if($font.Color -ne 192 -or $font.Underline -ne 2) {throw "QCC notice style differs: $($edit.cell)"}
                }
            }
        }
        [void]$scratch.Delete()
        if($Render) {
            $sheet=$candidateBook.Worksheets.Item($item.main)
            $sheet.PageSetup.Orientation=2
            $sheet.PageSetup.PaperSize=8
            $sheet.PageSetup.Zoom=$false
            $sheet.PageSetup.FitToPagesWide=1
            $sheet.PageSetup.FitToPagesTall=$false
            $sheet.PageSetup.PrintTitleRows=''
            $sheet.PageSetup.PrintTitleColumns=''
            $sheet.PageSetup.LeftMargin=18
            $sheet.PageSetup.RightMargin=18
            $sheet.PageSetup.TopMargin=18
            $sheet.PageSetup.BottomMargin=18
            $sheet.PageSetup.LeftHeader=''
            $sheet.PageSetup.CenterHeader=''
            $sheet.PageSetup.RightHeader=''
            $sheet.PageSetup.LeftFooter=''
            $sheet.PageSetup.CenterFooter=''
            $sheet.PageSetup.RightFooter=''
            $regions=@('I34:K34','H35:K37','H41:K43','I44:K47','G50:K52','I53:K53','H64:K67','I68:K69','I203:K203','H239:K242','I243:K243','I244:K246','I247:K248','H260:K260','H261:K268','I274:K275','I219:K221','J230:K233')
            if($item.code -eq 'SSTC') {$regions=@('H35:J37','I53:J53','H239:J242','H261:J268')}
            foreach($region in $regions) {
                $sheet.PageSetup.PrintArea=$region
                $target=Join-Path $qaPath ($item.code.ToLowerInvariant()+'-'+$region.Replace(':','_')+'.pdf')
                $sheet.ExportAsFixedFormat(0,$target,0,$false,$false)
            }
        }
        $candidateBook.Close($false)
        [void][Runtime.InteropServices.Marshal]::ReleaseComObject($candidateBook)
        $candidateBook=$null
        Write-Output ($item.code+' native read-only check complete')
    }
    $checks|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 -LiteralPath (Join-Path $qaPath 'native-check.json')
    Write-Output ('Checked cells='+$checks.Count+'; undersized='+@($checks|Where-Object {-not $_.fits}).Count+'; max needed='+($checks|Measure-Object -Property needed -Maximum).Maximum)
} finally {
    if($null -ne $candidateBook) {$candidateBook.Close($false);[void][Runtime.InteropServices.Marshal]::ReleaseComObject($candidateBook)}
    if($null -ne $excelInstance) {$excelInstance.Quit();[void][Runtime.InteropServices.Marshal]::ReleaseComObject($excelInstance)}
}
