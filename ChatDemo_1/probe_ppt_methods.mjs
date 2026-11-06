import { FileBlob, PresentationFile } from "file:///C:/Users/%E5%BE%90%E4%B8%80%E8%B1%AA/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/@oai/artifact-tool/dist/artifact_tool.mjs";
const p = await PresentationFile.importPptx(await FileBlob.load("D:/qtcode/ChatDemo_1/tmp/ppt_vchat/source_business_plan.pptx"));
console.log("slides collection", Object.getOwnPropertyNames(Object.getPrototypeOf(p.slides)));
const s = p.slides.items[0];
console.log("slide proto", Object.getOwnPropertyNames(Object.getPrototypeOf(s)));
console.log("shape proto", Object.getOwnPropertyNames(Object.getPrototypeOf(s.shapes)));
const sh = s.shapes.items?.[0];
if (sh) console.log("one shape proto", Object.getOwnPropertyNames(Object.getPrototypeOf(sh)));
const textShape = s.shapes.items?.find(x => x.text);
if (textShape) {
  console.log("text shape keys", Object.keys(textShape));
  console.log("text proto", Object.getOwnPropertyNames(Object.getPrototypeOf(textShape.text)));
  console.log("text style", textShape.text.style);
}
