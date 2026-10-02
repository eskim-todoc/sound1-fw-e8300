import fs from 'node:fs/promises';
import path from 'node:path';
import {fileURLToPath,pathToFileURL} from 'node:url';
const workspaceDir=path.dirname(fileURLToPath(import.meta.url));
const SKILL_DIR='C:/Users/EUNSU/.codex/plugins/cache/openai-primary-runtime/presentations/26.909.61513/skills/presentations';
process.env.RUNTIME_NODE_MODULES='C:/Users/EUNSU/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules';
const {finalizePresentation}=await import(pathToFileURL(path.join(SKILL_DIR,'container_tools/artifact_tool_utils.mjs')).href);
const finalPath=path.join(workspaceDir,'qa','인공달팽이관 시스템 구성도.pptx');
await fs.mkdir(path.dirname(finalPath),{recursive:true});
console.log(await finalizePresentation({workspaceDir,candidatePath:path.join(workspaceDir,'build','인공달팽이관 시스템 구성도.pptx'),finalPath,
 pythonExecutable:'C:/Users/EUNSU/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe',
 integrityValidatorPath:path.join(SKILL_DIR,'container_tools/inspect_presentation_package_integrity.py'),
 layoutValidatorPath:path.join(SKILL_DIR,'container_tools/inspect_presentation_layout_geometry.py'),
 explicitTotalSlideCount:7,verifyArtifactToolImport:true,
 receiptPath:path.join(workspaceDir,'ppt-validation.json')}));
