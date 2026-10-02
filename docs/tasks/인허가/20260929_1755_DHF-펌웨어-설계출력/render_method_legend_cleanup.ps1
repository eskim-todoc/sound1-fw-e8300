$ErrorActionPreference='Stop'
$qa=Join-Path $PSScriptRoot 'qa/method-legend-cleanup'
$plan=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qa 'plan.json') | ConvertFrom-Json
$excel=New-Object -ComObject Excel.Application
$excel.Visible=$false
$excel.DisplayAlerts=$false
try {
    foreach($b in $plan.books) {
        foreach($version in @('before','candidate')) {
            $file=Join-Path (Join-Path $qa $b.key) "$version.xlsx"
            if(-not (Test-Path -LiteralPath $file)){continue}
            $book=$excel.Workbooks.Open($file,0,$true)
            try {
                $sheet=$book.Worksheets.Item(2)
                $last=[string][char](64+$b.cols)
                $sheet.PageSetup.PrintArea=('B10:'+ $last +'16')
                $sheet.PageSetup.PrintTitleRows=''
                $sheet.PageSetup.PrintTitleColumns=''
                $sheet.PageSetup.Zoom=$false
                $sheet.PageSetup.FitToPagesWide=1
                $sheet.PageSetup.FitToPagesTall=1
                $sheet.ExportAsFixedFormat(0,(Join-Path (Join-Path $qa $b.key) "$version-legend.pdf"))
                if($b.key -eq 'unit') {
                    $sheet.PageSetup.PrintArea='B22:K24'
                    $sheet.ExportAsFixedFormat(0,(Join-Path (Join-Path $qa $b.key) "$version-note.pdf"))
                }
            } finally {$book.Close($false)}
        }
    }
} finally {$excel.Quit();[void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($excel)}
