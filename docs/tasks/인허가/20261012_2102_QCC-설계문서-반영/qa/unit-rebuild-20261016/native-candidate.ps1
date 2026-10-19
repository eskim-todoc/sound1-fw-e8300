$ErrorActionPreference='Stop';$h=Split-Path -Parent $PSCommandPath;$source=Join-Path $h 'candidate.xlsx';$expected='037CF72A05B276DE25FD21B7EE457A655736BE8FB6D4AE14B67C881F0E3EBEF4'
$record=[ordered]@{source=$source;source_sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $source).Hash;status='미실행';readonly=$true;saved=$false;exports=@();columns=@();row_heights=@();cells=@();new_condition_fonts=@()}
if($record.source_sha256 -ne $expected){throw 'Candidate source guard mismatch'}
$x=$null;$b=$null;$w=$null;$c=$null
function Save-Record{$record | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $h 'native-candidate.json') -Encoding utf8}
function Export-Area($ws,$name,$area){$ws.PageSetup.PrintArea=$area;$pdf=Join-Path $h ('native-'+$name+'.pdf');$ws.ExportAsFixedFormat(0,$pdf,0,$true,$false);$record.exports+=[ordered]@{name=$name;sheet=$ws.Name;area=$area;pdf=$pdf;sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $pdf).Hash};Save-Record}
try{
 $x=New-Object -ComObject Excel.Application;$x.Visible=$false;$x.DisplayAlerts=$false;$x.AskToUpdateLinks=$false;$x.AutomationSecurity=3
 $b=$x.Workbooks.Open($source,0,$true);$w=$b.Worksheets.Item(2);$c=$b.Worksheets.Item(3)
 $record.native_readonly=[bool]$b.ReadOnly
 $record.sheets=@(foreach($ws in $b.Worksheets){[ordered]@{name=$ws.Name;used=$ws.UsedRange.Address();standard_height=$ws.StandardHeight;standard_width=$ws.StandardWidth}})
 $s=$w.PageSetup;$record.page_setup=[ordered]@{zoom=$s.Zoom;fit_wide=$s.FitToPagesWide;fit_tall=$s.FitToPagesTall;orientation=$s.Orientation;paper_size=$s.PaperSize;print_area=$s.PrintArea;print_title_rows=$s.PrintTitleRows;left_margin=$s.LeftMargin;right_margin=$s.RightMargin;top_margin=$s.TopMargin;bottom_margin=$s.BottomMargin}
 for($i=1;$i -le 13;$i++){$col=$w.Columns.Item($i);$record.columns+=[ordered]@{column=$i;column_width=$col.ColumnWidth;width_pt=$col.Width;hidden=$col.Hidden}}
 foreach($r in @(19,24)+@(1686..1775)){$row=$w.Rows.Item($r);$record.row_heights+=[ordered]@{row=$r;height_pt=$row.RowHeight;hidden=$row.Hidden}}
 foreach($addr in @('D19','D24','B1726','K1726','L1726','B1727','C1727','D1727','E1727','F1727','G1727','H1727','I1727','J1727','K1727','L1727','M1727','B1764','G1764','K1764','L1764','M1764','B1765','B1772')){$cell=$w.Range($addr);$font=$cell.Font;$record.cells+=[ordered]@{address=$addr;value=$cell.Value2;merge=[bool]$cell.MergeCells;merge_area=$cell.MergeArea.Address();font=$font.Name;size=$font.Size;bold=$font.Bold;color=$font.Color;underline=$font.Underline;horizontal=$cell.HorizontalAlignment;vertical=$cell.VerticalAlignment;wrap=$cell.WrapText;indent=$cell.IndentLevel}}
 for($r=1727;$r -le 1764;$r++){foreach($col in @('K','L','M')){$cell=$w.Range($col+$r);$f=$cell.Font;$record.new_condition_fonts+=[ordered]@{address=$col+$r;font=$f.Name;size=$f.Size;color=$f.Color;underline=$f.Underline;horizontal=$cell.HorizontalAlignment;vertical=$cell.VerticalAlignment;merge_area=$cell.MergeArea.Address()}}}
 $record.phase='inspected';Save-Record
 Export-Area $w 'candidate-qcc-full' 'B1727:M1775'
 Export-Area $w 'candidate-boundary' 'B1719:M1730'
 Export-Area $w 'candidate-common' 'B15:M27'
 $record.comparison_original_setup=[ordered]@{zoom=$c.PageSetup.Zoom;fit_wide=$c.PageSetup.FitToPagesWide;fit_tall=$c.PageSetup.FitToPagesTall;orientation=$c.PageSetup.Orientation;print_area=$c.PageSetup.PrintArea}
 $c.PageSetup.Zoom=$false;$c.PageSetup.FitToPagesWide=1;$c.PageSetup.FitToPagesTall=$false;$c.PageSetup.Orientation=2;$c.PageSetup.PrintTitleRows='$5:$5'
 Export-Area $c 'candidate-comparison' 'B5:G347'
 Export-Area $c 'candidate-comparison-first' 'B5:G12'
 Export-Area $c 'candidate-comparison-last' 'B341:G347'
 $record.status='통과'
}catch{$record.status='실패';$record.error=$_.Exception.ToString();throw}
finally{if($b){$b.Close($false)};if($x){$x.Quit()};foreach($o in @($c,$w,$b,$x)){if($o){[void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($o)}};[GC]::Collect();[GC]::WaitForPendingFinalizers();$record.closed_without_saving=$true;$record.source_end_sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $source).Hash;$record.source_unchanged=$record.source_sha256 -eq $record.source_end_sha256;Save-Record}
