// Author complete inline acceptance/result cells with Artifact Tool. Native
// historical sheets, styling and images are preserved in the package step.
import fs from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
import {FileBlob, SpreadsheetFile} from '@oai/artifact-tool';
const task=fileURLToPath(new URL('./',import.meta.url));
const qa=`${task}/qa/unit-inline-acceptance`;
const plan=JSON.parse(await fs.readFile(`${qa}/plan.json`,'utf8'));
const mode=process.argv[2]??'build';
for(const book of plan.baseline.books){
  const input=mode==='build'?book.backup:`${qa}/${book.key}-candidate.xlsx`;
  const wb=await SpreadsheetFile.importXlsx(await FileBlob.load(input));
  const sheet=wb.worksheets.getItem(book.sheet);
  if(mode==='build'){
    // Only K/L payloads are imported into native packages; original common
    // metadata and headers are not reserialized by this authoring step.
    const end=plan.endNew;
    const rows=plan.body.map(r=>book.key==='criterion'?[r[10]]:[r[10],r[11]]);
    sheet.getRange(`K30:${book.key==='criterion'?'K':'L'}${end}`).values=rows;
    for(const r of plan.records)for(const row of r.rows){
      sheet.getRange(`K${row.row}:${book.key==='criterion'?'K':'L'}${row.row}`).format.rowHeight=row.height;
    }
    wb.recalculate();
    const errors=await wb.inspect({kind:'match',searchTerm:'#REF!|#DIV/0!|#VALUE!|#NAME\\?',options:{useRegex:true,maxResults:20},maxChars:1200});
    await fs.writeFile(`${qa}/${book.key}-artifact-check.ndjson`,errors.ndjson??'');
    await(await SpreadsheetFile.exportXlsx(wb)).save(`${qa}/${book.key}-authored.xlsx`);
    console.log(book.key,'authored',rows.length,'rows');
  }else{
    for(const id of ['CFX-001','CFX-020','HEAR-002','CM3-029','CM3-051','BOOT-018']){
      const r=plan.records.find(r=>r.id===id);
      const start=id==='CFX-020'?r.rows.find(r=>r.kind==='rtt').row:r.start;
      const last=Math.min(r.end,start+2);
      const png=await wb.render({sheetName:book.sheet,range:`K${start}:${book.key==='criterion'?'K':'M'}${last}`,scale:1,format:'png'});
      await fs.writeFile(`${qa}/${book.key}-${id}.png`,new Uint8Array(await png.arrayBuffer()));
    }
    console.log(book.key,'rendered samples');
  }
}
