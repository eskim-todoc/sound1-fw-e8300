$ErrorActionPreference='Stop'
$h=Split-Path -Parent $PSCommandPath
$source=Join-Path $h 'base.xlsx'
$record=[ordered]@{source=$source;source_sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $source).Hash;status='미실행';readonly=$true;saved=$false;exports=@();columns=@();row_heights=@();cells=@()}
$excel=$null;$book=$null;$sheet=$null
function Save-Record { $record | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $h 'native-base.json') -Encoding utf8 }
try {
 $excel=New-Object -ComObject Excel.Application
 $excel.Visible=$false;$excel.DisplayAlerts=$false;$excel.AskToUpdateLinks=$false;$excel.AutomationSecurity=3
 $book=$excel.Workbooks.Open($source,0,$true)
 $sheet=$book.Worksheets.Item('보고서_유닛 시험')
 $record.native_readonly=[bool]$book.ReadOnly
 $record.sheets=@(foreach($ws in $book.Worksheets){[ordered]@{name=$ws.Name;used=$ws.UsedRange.Address();standard_height=$ws.StandardHeight;standard_width=$ws.StandardWidth}})
 $setup=$sheet.PageSetup
 $record.page_setup=[ordered]@{zoom=$setup.Zoom;fit_wide=$setup.FitToPagesWide;fit_tall=$setup.FitToPagesTall;orientation=$setup.Orientation;paper_size=$setup.PaperSize;print_area=$setup.PrintArea;print_title_rows=$setup.PrintTitleRows;left_margin=$setup.LeftMargin;right_margin=$setup.RightMargin;top_margin=$setup.TopMargin;bottom_margin=$setup.BottomMargin}
 for($c=1;$c -le 13;$c++){ $col=$sheet.Columns.Item($c);$record.columns+= [ordered]@{column=$c;address=$col.Address();column_width=$col.ColumnWidth;width_pt=$col.Width;hidden=$col.Hidden} }
 for($r=1686;$r -le 1726;$r++){ $row=$sheet.Rows.Item($r);$record.row_heights+=[ordered]@{row=$r;height_pt=$row.RowHeight;hidden=$row.Hidden} }
 foreach($addr in @('B32','C32','D32','E32','F32','G32','H32','I32','J32','K32','L32','M32','B1686','C1686','D1686','E1686','F1686','G1686','H1686','I1686','J1686','K1686','L1686','M1686','K1694','L1694','K1726','L1726')){
  $cell=$sheet.Range($addr);$font=$cell.Font
  $record.cells+=[ordered]@{address=$addr;value=$cell.Value2;merge=[bool]$cell.MergeCells;merge_area=$cell.MergeArea.Address();style=$cell.Style;font=$font.Name;size=$font.Size;bold=$font.Bold;color=$font.Color;underline=$font.Underline;horizontal=$cell.HorizontalAlignment;vertical=$cell.VerticalAlignment;wrap=$cell.WrapText;indent=$cell.IndentLevel;top=$cell.Top;left=$cell.Left;height=$cell.Height;width=$cell.Width}
 }
 $record.phase='inspected';Save-Record
 foreach($item in @(@('base-left','B1686:J1726'),@('base-results','K1686:M1726'))){
  $sheet.PageSetup.PrintArea=$item[1]
  $pdf=Join-Path $h ('native-'+$item[0]+'.pdf')
  $sheet.ExportAsFixedFormat(0,$pdf,0,$true,$false)
  $record.exports+=[ordered]@{name=$item[0];area=$item[1];pdf=$pdf;sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $pdf).Hash;scale='Original Zoom/Fit/Orientation preserved; print area only in read-only RAM'}
  Save-Record
 }
 $record.status='통과'
} catch {$record.status='실패';$record.error=$_.Exception.ToString();throw}
finally {
 if($null -ne $book){$book.Close($false)}
 if($null -ne $excel){$excel.Quit()}
 foreach($obj in @($sheet,$book,$excel)){if($null -ne $obj){[void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($obj)}}
 [GC]::Collect();[GC]::WaitForPendingFinalizers()
 $record.closed_without_saving=$true
 $record.source_end_sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $source).Hash
 $record.source_unchanged=$record.source_sha256 -eq $record.source_end_sha256
 Save-Record
}
