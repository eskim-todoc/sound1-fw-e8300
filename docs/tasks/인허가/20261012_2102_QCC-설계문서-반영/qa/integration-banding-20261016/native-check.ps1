param([Parameter(Mandatory=$true)][string]$ExpectedSHA256)
$ErrorActionPreference='Stop';$h=Split-Path -Parent $PSCommandPath;$source=Join-Path $h 'candidate.xlsx'
$r=[ordered]@{source=$source;source_sha256=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash;status='미실행';readonly=$true;saved=$false;rows=@();groups=@();exports=@()}
if($r.source_sha256 -ne $ExpectedSHA256){throw 'Source hash guard mismatch'}
$x=$null;$b=$null;$w=$null
try{
 $x=New-Object -ComObject Excel.Application;$x.Visible=$false;$x.DisplayAlerts=$false;$x.AskToUpdateLinks=$false;$x.AutomationSecurity=3
 $b=$x.Workbooks.Open($source,0,$true);$w=$b.Worksheets.Item('보고서_통합시험');$r.native_readonly=[bool]$b.ReadOnly;$r.repair_requested=$false
 $r.page_setup=[ordered]@{zoom=$w.PageSetup.Zoom;fit_wide=$w.PageSetup.FitToPagesWide;fit_tall=$w.PageSetup.FitToPagesTall;orientation=$w.PageSetup.Orientation;print_title_rows=$w.PageSetup.PrintTitleRows}
 $starts=@();for($row=25;$row -le 144;$row++){$value=[string]$w.Range('C'+$row).Value2;if($value -match '^\s*SW-FW-IntegrationTest-ID-(\d{3})\s*$'){$starts+=@{row=$row;id=[int]$Matches[1]}}}
 for($i=0;$i -lt $starts.Count;$i++){$g=$starts[$i];$end=if($i+1 -lt $starts.Count){$starts[$i+1].row-1}else{144};$expected=if($g.id%2 -eq 1){16777215}else{16119285};$r.groups+=[ordered]@{id=$g.id;first_row=$g.row;last_row=$end;expected_rgb=if($g.id%2 -eq 1){'FFFFFF'}else{'F5F5F5'}};for($row=$g.row;$row -le $end;$row++){$actual=$w.Range('B'+$row+':O'+$row).Interior.Color;$ok=$null -ne $actual -and [int]$actual -eq $expected;$r.rows+=[ordered]@{row=$row;id=$g.id;range='B'+$row+':O'+$row;actual_color=$actual;expected_color=$expected;pass=$ok};if(-not $ok){$r.rows[-1].cells=@(foreach($col in @('B','C','D','E','F','G','H','I','J','K','L','M','N','O')){@{address=$col+$row;color=$w.Range($col+$row).Interior.Color}})}}}
 $r.ids_complete=($starts.Count -eq 24 -and ($starts.id -join ',') -eq (1..24 -join ','));$r.band_rows_pass=@($r.rows|Where-Object{-not $_.pass}).Count -eq 0
 foreach($item in @(@('front','B25:O28'),@('end','B142:O144'))){$w.PageSetup.PrintArea=$item[1];$pdf=Join-Path $h ('native-'+$item[0]+'.pdf');$w.ExportAsFixedFormat(0,$pdf,0,$true,$false);$r.exports+=[ordered]@{name=$item[0];area=$item[1];pdf=$pdf;sha256=(Get-FileHash -LiteralPath $pdf -Algorithm SHA256).Hash}}
 $r.status=if($r.ids_complete -and $r.band_rows_pass){'통과'}else{'실패'}
}catch{$r.status='실패';$r.error=$_.Exception.ToString();throw}
finally{if($b){$b.Close($false)};if($x){$x.Quit()};foreach($o in @($w,$b,$x)){if($o){[void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($o)}};[GC]::Collect();[GC]::WaitForPendingFinalizers();$r.closed_without_saving=$true;$r.source_end_sha256=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash;$r.source_unchanged=$r.source_sha256 -eq $r.source_end_sha256;$r|ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $h 'native-check.json') -Encoding utf8}
