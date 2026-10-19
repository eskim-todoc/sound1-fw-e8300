import fs from 'node:fs/promises';
import path from 'node:path';
import crypto from 'node:crypto';
import {FileBlob, SpreadsheetFile} from '@oai/artifact-tool';

const root=import.meta.dirname;
const plan=JSON.parse(await fs.readFile(path.join(root,'plan.json'),'utf8'));
const records=[];
const importDiagnostic=await fs.readFile(path.join(root,'artifact-import-fixes.json'),'utf8').then(JSON.parse).catch(e=>{if(e.code==='ENOENT')return {fixes:[],mergeDifferences:[]};throw e;});
const importFixes=importDiagnostic.fixes;
await fs.mkdir(path.join(root,'authored'),{recursive:true});
for(const b of plan.books){
  const hash=crypto.createHash('sha256').update(await fs.readFile(b.path)).digest('hex').toUpperCase();
  if(hash!==b.sourceSHA256.toUpperCase())throw new Error(`${b.code}: source hash changed`);
  const wb=await SpreadsheetFile.importXlsx(await FileBlob.load(b.path));
  for(const fix of importFixes.filter(f=>f.code===b.code)){
    if(fix.expected!=='')throw new Error('Only verified empty shared-string import fixes are supported');
    wb.worksheets.getItem(fix.sheet).getRange(fix.cell).values=[['']];
  }
  const record={code:b.code,sourceSHA256:hash,sheets:[]};
  for(const s of b.sheets){
    const main=wb.worksheets.getItem(s.name);
    const compare=wb.worksheets.add(s.compareName);
    compare.getRange(s.range).copyFrom(main.getRange(s.range),'all');
    for(const m of importDiagnostic.mergeDifferences.filter(m=>m.code===b.code && m.sheet===s.name)){
      for(const address of m.missing)compare.getRange(address).merge();
    }
    for(const c of s.changes)main.getRange(c.cell).values=[[c.after]];
    record.sheets.push({name:s.name,compareName:s.compareName,range:s.range,changes:s.changes.length});
  }
  wb.recalculate();
  for(const s of b.sheets){
    const main=wb.worksheets.getItem(s.name);
    for(const c of s.changes){
      const got=main.getRange(c.cell).values[0][0];
      if((got??'')!==c.after)throw new Error(`${b.code}/${s.name}/${c.cell}: authored value mismatch`);
    }
  }
  record.inspect=await wb.inspect({kind:'sheet',include:'id,name',maxChars:3000});
  record.path=path.join(root,'authored',`${b.code}-authored.xlsx`);
  await(await SpreadsheetFile.exportXlsx(wb)).save(record.path);
  record.allValuesMatched=true;
  records.push(record);
  await fs.writeFile(path.join(root,'artifact-authoring.json'),JSON.stringify(records,null,2));
  console.log(JSON.stringify({code:b.code,sheets:record.sheets,allValuesMatched:true,exported:true}));
}
