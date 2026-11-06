import fs from "node:fs/promises";
import { FileBlob, PresentationFile } from "file:///C:/Users/%E5%BE%90%E4%B8%80%E8%B1%AA/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/@oai/artifact-tool/dist/artifact_tool.mjs";

const source = "D:/qtcode/ChatDemo_1/tmp/ppt_vchat/source_business_plan.pptx";
const outDir = "D:/qtcode/ChatDemo_1/tmp/ppt_vchat/manual_inspect";

async function writeBlob(path, blob) {
  await fs.writeFile(path, new Uint8Array(await blob.arrayBuffer()));
}

await fs.mkdir(outDir, { recursive: true });
const presentation = await PresentationFile.importPptx(await FileBlob.load(source));
const snapshot = await presentation.inspect({
  kind: "slide,textbox,shape,image,table,chart,layout",
  maxChars: 50000,
});
await fs.writeFile(`${outDir}/inspect.ndjson`, snapshot.ndjson, "utf8");
await writeBlob(`${outDir}/montage.webp`, await presentation.export({ format: "webp", montage: true, scale: 1 }));

for (const [index, slide] of presentation.slides.items.entries()) {
  const stem = `slide-${String(index + 1).padStart(2, "0")}`;
  await writeBlob(`${outDir}/${stem}.png`, await presentation.export({ slide, format: "png", scale: 1 }));
  const layout = await slide.export({ format: "layout" });
  await fs.writeFile(`${outDir}/${stem}.layout.json`, await layout.text(), "utf8");
}

console.log(`slides=${presentation.slides.items.length}`);
console.log(`${outDir}/inspect.ndjson`);
