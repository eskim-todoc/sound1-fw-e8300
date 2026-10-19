$ErrorActionPreference = 'Stop'
$base = $PSScriptRoot
$source = Join-Path $base 'style-after.xlsx'
$expected = '8A6F683CC357F217900CC7975C6750F0D05394A46DDE242A4380C02FC9C801A4'
$recordPath = Join-Path $base 'style-native-export.json'
$sourceHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
if ($sourceHash -ne $expected) { throw 'Candidate SHA guard mismatch' }
$record = [ordered]@{ status='미실행'; source=$source; source_sha256=$sourceHash; started_at=(Get-Date).ToString('o'); readonly=$false; saved=$false; exports=@(); sheets=@(); photos=@(); options_scope='Owned hidden Excel instance only; workbook not saved' }
function Save-Progress { $record | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $recordPath -Encoding utf8 }
$excel=$null
$book=$null
try {
  $excel=New-Object -ComObject Excel.Application
  $excel.Visible=$false
  $excel.DisplayAlerts=$false
  $excel.AutomationSecurity=3
  $excel.ScreenUpdating=$false
  $book=$excel.Workbooks.Open($source,0,$true)
  $record.readonly=[bool]$book.ReadOnly
  Save-Progress
  foreach ($sheet in $book.Worksheets) {
    $used=$sheet.UsedRange
    $record.sheets += [ordered]@{ name=[string]$sheet.Name; used_range=[string]$used.Address(); stored_print_area=[string]$sheet.PageSetup.PrintArea; font_size=$used.Font.Size }
    [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($used)
    [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($sheet)
  }
  $main=$book.Worksheets.Item(2)
  $record.main_body_font_size=$main.Range('B34:L293').Font.Size
  foreach ($shape in $main.Shapes) {
    $topCell=$shape.TopLeftCell
    $bottomCell=$shape.BottomRightCell
    $record.photos += [ordered]@{ name=[string]$shape.Name; height_pt=[double]$shape.Height; width_pt=[double]$shape.Width; top_pt=[double]$shape.Top; left_pt=[double]$shape.Left; top_left=[string]$topCell.Address(); bottom_right=[string]$bottomCell.Address() }
    [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($topCell)
    [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($bottomCell)
    [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($shape)
  }
  [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($main)
  $specs=@(
    @{name='cover'; sheet=1; area='B1:AD34'; title=''; tall=1},
    @{name='main-intro'; sheet=2; area='B1:L31'; title=''; tall=0},
    @{name='main-results'; sheet=2; area='J34:L293'; title='$32:$33'; tall=0},
    @{name='main-first-full'; sheet=2; area='B32:L38'; title='$32:$33'; tall=0},
    @{name='main-ci'; sheet=2; area='C42:I56'; title='$32:$33'; tall=0},
    @{name='main-ci-crc'; sheet=2; area='C219:I220'; title='$32:$33'; tall=0},
    @{name='main-ci-errors'; sheet=2; area='C234:I235'; title='$32:$33'; tall=0},
    @{name='main-ci-last'; sheet=2; area='C291:I293'; title='$32:$33'; tall=0},
    @{name='support'; sheet=3; area='A2:D12'; title=''; tall=0},
    @{name='support-test75'; sheet=3; area='A210:D212'; title=''; tall=0},
    @{name='support-test59'; sheet=3; area='A233:D235'; title=''; tall=0},
    @{name='expected'; sheet=4; area='A2:C12'; title=''; tall=0},
    @{name='cyber'; sheet=5; area='A1:L8'; title=''; tall=0},
    @{name='comparison'; sheet=6; area='B1220:L1226'; title=''; tall=0},
    @{name='cyber-comparison'; sheet=7; area='A1:L8'; title=''; tall=0}
  )
  foreach ($spec in $specs) {
    $sheet=$book.Worksheets.Item($spec.sheet)
    $sheet.PageSetup.PrintArea=$spec.area
    $sheet.PageSetup.PrintTitleRows=$spec.title
    $sheet.PageSetup.Orientation=2
    $sheet.PageSetup.Zoom=$false
    $sheet.PageSetup.FitToPagesWide=1
    if ($spec.tall -eq 1) { $sheet.PageSetup.FitToPagesTall=1 } else { $sheet.PageSetup.FitToPagesTall=$false }
    $pdf=Join-Path $base ('style-native-'+$spec.name+'.pdf')
    $record.current_export=$spec.name
    Save-Progress
    $sheet.ExportAsFixedFormat(0,$pdf)
    $record.exports += [ordered]@{ name=$spec.name; sheet=[string]$sheet.Name; area=$spec.area; title_rows=$spec.title; pdf=$pdf; sha256=(Get-FileHash -LiteralPath $pdf -Algorithm SHA256).Hash; exported_at=(Get-Date).ToString('o'); note='Print area/fit changed only in read-only RAM copy; not saved to source' }
    Write-Output ('EXPORTED '+$spec.name)
    Save-Progress
    [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($sheet)
  }
  $record.status='통과'
} catch {
  $record.status='실패'
  $record.error=$_.Exception.Message
} finally {
  if ($null -ne $book) { $book.Close($false); [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($book) }
  if ($null -ne $excel) { $excel.Quit(); [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($excel) }
  [GC]::Collect()
  [GC]::WaitForPendingFinalizers()
  $record.source_end_sha256=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
  $record.source_unchanged=($record.source_sha256 -eq $record.source_end_sha256)
  $record.completed_at=(Get-Date).ToString('o')
  $record.closed_without_saving=$true
  Save-Progress
}
Write-Output ('STATUS '+$record.status)
