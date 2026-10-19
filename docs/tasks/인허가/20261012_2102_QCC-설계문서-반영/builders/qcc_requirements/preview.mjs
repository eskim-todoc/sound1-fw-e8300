import fs from 'node:fs/promises';
import path from 'node:path';
import {FileBlob,SpreadsheetFile} from '@oai/artifact-tool';
const base='E:/workspace/projects/sound1-fw-e8300-backup/docs/tasks/인허가/20261012_2102_QCC-설계문서-반영';
const source='G:/공유 드라이브/20_프로젝트별/01_project sound1/08_DHF/02_설계 입력/SW/외부기/[E8300-QCC] SRS-FW-TD24A 소프트웨어 요구사항 사양서.xlsx';
const wb=await SpreadsheetFile.importXlsx(await FileBlob.load(source));
for(const [id,range] of [['rid82','C49:I49'],['rid52','C61:I61'],['rid83','C80:I80']]){
 const png=await wb.render({sheetName:'소프트웨어 입력 요구사항 사양서',range,scale:1,format:'png'});
 await fs.writeFile(path.join(base,`qa/qcc_requirements/srs-${id}-saved.png`),new Uint8Array(await png.arrayBuffer()));
 console.log(JSON.stringify({id,range,rendered:true}));
}
