param([Parameter(Mandatory=$true)][string]$InputFolder,[Parameter(Mandatory=$true)][string]$OutputFolder)
$ErrorActionPreference = 'Stop'
$null = New-Item -ItemType Directory -Path $OutputFolder -Force
$app = New-Object -ComObject Word.Application
$app.Visible = $false
$app.DisplayAlerts = 0
$app.AutomationSecurity = 3
try {
    foreach ($file in Get-ChildItem -LiteralPath $InputFolder -Filter '*.docx' -File) {
        $doc = $null
        try {
            $doc = $app.Documents.Open($file.FullName, $false, $true, $false)
            $doc.Repaginate()
            # Update field results in memory only; source DOCX is never saved.
            $null = $doc.Fields.Update()
            foreach ($toc in $doc.TablesOfContents) { $toc.Update() }
            foreach ($section in $doc.Sections) {
                foreach ($header in $section.Headers) { if ($header.Exists) { $null=$header.Range.Fields.Update() } }
                foreach ($footer in $section.Footers) { if ($footer.Exists) { $null=$footer.Range.Fields.Update() } }
            }
            $doc.Repaginate()
            $pdf = Join-Path $OutputFolder ($file.BaseName + '.pdf')
            $doc.ExportAsFixedFormat($pdf,17)
            Write-Output ($file.Name + ': ' + $doc.ComputeStatistics(2) + ' pages -> ' + $pdf)
        } finally { if ($null -ne $doc) { $doc.Close(0); $null=[Runtime.InteropServices.Marshal]::ReleaseComObject($doc) } }
    }
} finally { $app.Quit(0); $null=[Runtime.InteropServices.Marshal]::ReleaseComObject($app) }
