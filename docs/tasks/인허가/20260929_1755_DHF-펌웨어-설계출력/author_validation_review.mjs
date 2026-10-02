import fs from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
import {FileBlob, SpreadsheetFile, Workbook} from '@oai/artifact-tool';
const q = new URL('./qa/validation-review/', import.meta.url);
const mode = process.argv[2];
const plan = JSON.parse(await fs.readFile(new URL('plan.json',q),'utf8'));
if (mode === 'author') {
  const w = Workbook.create();
  const s = w.worksheets.add('수정 내용');
  const rows = plan.books.flatMap(b => Object.entries(b.targets).flatMap(([id,t]) =>
    Object.entries(t.changes).map(([col,text]) => [b.key+'|'+id+'|'+col,text])));
  s.getRange(`A1:B${rows.length}`).values=rows;
  const reference=JSON.parse(await fs.readFile(new URL('../../../../../../tests/unit/tdc_ut_cfx_io_criteria.json',q),'utf8'))['HEAR-002'];
  const h=w.worksheets.add('HEAR 기대값');
  h.getRange('A1:H1').merge();h.getRange('A1').values=[['HEAR-002 사전 기대값 / 실기 측정 결과 아님']];
  h.getRange('A2:H2').merge();h.getRange('A2').values=[[reference.dft_formula+' / '+reference.window_formula]];
  h.getRange('A3:H3').merge();h.getRange('A3').values=[['FFT raw*2^E, vMag raw*2^(E+1). FFT 허용=64+ceil(abs(기준)/1000), vMag 허용=64+ceil(기준/100). 사전 검토 오차이며 제조사 보장값 아님.']];
  h.getRange('A5:C5').values=[['표본 n','창 계수 w[n]','Q1.23']];
  h.getRange('E5:J5').values=[['vMag bin','입력1:0','입력2:bin32','입력3:bin64','입력4:DC','입력5:Nyquist']];
  h.getRange('A6:B517').values=reference.exact_window_512.map((v,i)=>[i,v]);
  h.getRange('E6:J261').values=Array.from({length:256},(_,i)=>[i,...reference.expected_vectors.map(v=>v.vmag_unscaled_256[i])]);
  h.getRange('E264:K264').merge();h.getRange('E264').values=[['FFT 기준: k=0의 두 성분은 DC/Nyquist. 다른 k는 실수/허수. R은 해당 성분의 기준값. 입력1은 정확히0.']];
  h.getRange('E266:I266').values=[['입력 번호','bin k','실수/DC','허수/Nyquist','허용 오차']];
  const fft=reference.expected_vectors.flatMap(v=>reference.fft_bins_compared.map((k,i)=>[v.vector,k,v.fft_unscaled_packed_re_im[2*i],v.fft_unscaled_packed_re_im[2*i+1],v.vector===1?'0 (정확히)':'64 + ceil\n(|R| / 1000)']));
  h.getRange(`E267:I${266+fft.length}`).values=fft;
  h.getRange('A1:K517').format.font.name='맑은 고딕';h.getRange('A1:K517').format.font.size=11;
  h.getRange('A1:K517').format.rowHeight=24;h.getRange('A1:K517').format.columnWidth=16;
  h.getRange('I267:I311').format.wrapText=true;h.getRange('E267:I311').format.rowHeight=36;
  h.getRange('A1:K3').format.rowHeight=48;h.getRange('A1:K3').format.wrapText=true;
  for(const r of ['A1:H1','A5:C5','E5:J5','E266:I266']){h.getRange(r).format.font.bold=true;h.getRange(r).format.fill='#F2F2F2';}
  w.recalculate();
  await (await SpreadsheetFile.exportXlsx(w)).save(fileURLToPath(new URL('authored.xlsx',q)));
  console.log('Authored '+rows.length+' targeted values');
} else {
  const selection = process.argv[3];
  for (const b of plan.books.filter(b=>!selection||b.key===selection)) {
    const folder = new URL(b.key+'/',q);
    const w = await SpreadsheetFile.importXlsx(await FileBlob.load(fileURLToPath(new URL(mode==='before'?'before.xlsx':'candidate.xlsx',folder))));
    w.recalculate();
    const out = await w.inspect({kind:'match',searchTerm:'#REF!|#DIV/0!|#VALUE!|#NAME\\?',options:{useRegex:true,maxResults:15},maxChars:1800});
    console.log(b.key,out.ndjson);
    const ids = Object.keys(b.targets);
    for(const id of mode==='comparison'?[]:ids) {
      const row=b.targets[id].row;
      const range=b.key==='srs'?`D${row}:H${row}`:b.key==='unit'?`G${row}:K${row}`:b.key==='integration'?`I${row}:M${row}`:`G${row}:J${row}`;
      const image=await w.render({sheetName:b.sheet,range,scale:1.2,format:'png'});
      await fs.writeFile(new URL(mode+'-'+id+'.png',folder),new Uint8Array(await image.arrayBuffer()));
    }
    const sheet=w.worksheets.getItem(b.comparison);
    // Anchor at A1: a used range may start below row 1. Its array index is
    // otherwise not the worksheet row used by the image renderer.
    const values=sheet.getRange('A1:M1000').values;
    const id=Object.keys(b.targets)[0];
    const row=values.findIndex(r=>String(r[b.idcol-1])===id && String(r[0])==='[+]')+1;
    if(row) {
      const range=b.key==='srs'?`A${Math.max(1,row-1)}:H${row}`:b.key==='unit'?`F${Math.max(1,row-1)}:K${row}`:b.key==='integration'?`I${Math.max(1,row-1)}:M${row}`:`G${Math.max(1,row-1)}:J${row}`;
      const image=await w.render({sheetName:b.comparison,range,scale:1.1,format:'png'});
      await fs.writeFile(new URL(mode+'-comparison.png',folder),new Uint8Array(await image.arrayBuffer()));
    }
    if(mode!=='before' && mode!=='comparison' && b.key==='unit'){
      for(const range of ['A1:K15','E260:K278']){
        const image=await w.render({sheetName:'HEAR 기대값',range,scale:1.2,format:'png'});
        await fs.writeFile(new URL('review-hear-'+range.replace(':','-')+'.png',folder),new Uint8Array(await image.arrayBuffer()));
      }
    }
  }
}
