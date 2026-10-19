import fs from 'node:fs/promises';
import path from 'node:path';
import {FileBlob,SpreadsheetFile} from '@oai/artifact-tool';
const base='E:/workspace/projects/sound1-fw-e8300-backup/docs/tasks/인허가/20261012_2102_QCC-설계문서-반영';
const source=path.join(base,'qa/qcc_requirements/srs-prepared.xlsx');
const wb=await SpreadsheetFile.importXlsx(await FileBlob.load(source));
const sheet=wb.worksheets.getItem('소프트웨어 입력 요구사항 사양서');
const plan=JSON.parse(await fs.readFile(path.join(base,'evidence/qcc_requirements_change_plan.json'),'utf8'));
for(const edit of plan.cell_edits){
 const range=sheet.getRange(edit.cell);
 if(edit.formula)range.formulas=[[edit.formula]];
 else range.values=[[edit.value]];
 if(edit.cell.startsWith('I'))range.format.wrapText=true;
}
wb.recalculate();
const check=plan.cell_edits.map(edit=>({cell:edit.cell,value:sheet.getRange(edit.cell).values[0][0],formula:sheet.getRange(edit.cell).formulas[0][0]}));
for(const edit of plan.cell_edits){
 const item=check.find(c=>c.cell===edit.cell);
 if(edit.formula ? item.formula!==edit.formula : item.value!==edit.value)throw new Error(`Authoring mismatch ${edit.cell}`);
}
await fs.writeFile(path.join(base,'qa/qcc_requirements/srs-artifact-authoring-check.json'),JSON.stringify({sheet:sheet.name,edits:check},null,2));
await (await SpreadsheetFile.exportXlsx(wb)).save(path.join(base,'qa/qcc_requirements/srs-artifact.xlsx'));
console.log(JSON.stringify({authored_cells:check.length,formula82:sheet.getRange('G49').values[0][0],formula83:sheet.getRange('G80').values[0][0]}));
