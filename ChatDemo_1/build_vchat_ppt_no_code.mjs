import fs from "node:fs/promises";
import { Presentation, PresentationFile } from "file:///C:/Users/%E5%BE%90%E4%B8%80%E8%B1%AA/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/@oai/artifact-tool/dist/artifact_tool.mjs";

const OUT_DIR = "D:/qtcode/ChatDemo_1/outputs";
const QA_DIR = "D:/qtcode/ChatDemo_1/tmp/ppt_no_code/qa";
const FINAL = `${OUT_DIR}/VChat_PPT_课堂分享优化版.pptx`;
const W = 960;
const H = 540;

const C = {
  bg1: "#0E2029",
  bg2: "#162C37",
  panel: "#142B36",
  line: "#276175",
  accent: "#00AEEF",
  accent2: "#59D8FF",
  green: "#46D07E",
  orange: "#FFB84D",
  red: "#FF6B6B",
  text: "#F4FBFF",
  muted: "#A7C1CC",
};

async function writeBlob(path, blob) {
  await fs.writeFile(path, new Uint8Array(await blob.arrayBuffer()));
}

function bg(slide) {
  slide.background.fill = `linear(180deg, ${C.bg1} 0%, ${C.bg2} 100%)`;
  slide.shapes.add({
    geometry: "ellipse",
    position: { left: 590, top: -120, width: 430, height: 430 },
    fill: "rgba(255,255,255,0.028)",
    line: { style: "solid", fill: "rgba(255,255,255,0.02)", width: 1 },
  });
  slide.shapes.add({
    geometry: "ellipse",
    position: { left: -170, top: 330, width: 360, height: 360 },
    fill: "rgba(0,174,239,0.04)",
    line: { style: "solid", fill: "rgba(0,174,239,0.03)", width: 1 },
  });
}

function text(slide, value, x, y, w, h, opt = {}) {
  const shape = slide.shapes.add({
    geometry: "textbox",
    position: { left: x, top: y, width: w, height: h },
    fill: "none",
    line: { style: "solid", fill: "none", width: 0 },
  });
  shape.text = value;
  shape.text.style = {
    typeface: "Microsoft YaHei",
    fontSize: opt.size ?? 18,
    color: opt.color ?? C.text,
    bold: opt.bold ?? false,
    alignment: opt.align ?? "left",
    verticalAlignment: opt.valign ?? "top",
  };
  return shape;
}

function rect(slide, x, y, w, h, fill = C.panel, border = C.line) {
  return slide.shapes.add({
    geometry: "roundRect",
    position: { left: x, top: y, width: w, height: h },
    fill,
    line: { style: "solid", fill: border, width: 1 },
    borderRadius: "rounded-lg",
  });
}

function rule(slide, x, y, w, color = C.accent) {
  slide.shapes.add({
    geometry: "rect",
    position: { left: x, top: y, width: w, height: 3 },
    fill: color,
    line: { style: "solid", fill: color, width: 0 },
  });
}

function arrow(slide, x1, y1, x2, y2, color = C.accent2) {
  slide.shapes.add({
    geometry: "line",
    position: { left: x1, top: y1, width: x2 - x1, height: y2 - y1 },
    line: { style: "solid", fill: color, width: 2, endArrowType: "triangle" },
  });
}

function brand(slide, no) {
  text(slide, "VChat", 34, 20, 110, 24, { size: 16, bold: true, color: C.accent2 });
  text(slide, String(no).padStart(2, "0"), 875, 22, 42, 20, { size: 12, color: C.muted, align: "right" });
}

function title(slide, value, subtitle, no) {
  brand(slide, no);
  text(slide, value, 70, 62, 790, 48, { size: 36, bold: true });
  rule(slide, 70, 118, 100);
  if (subtitle) text(slide, subtitle, 70, 132, 790, 30, { size: 16, color: C.muted });
}

function card(slide, x, y, w, h, head, body, color = C.accent2) {
  rect(slide, x, y, w, h, "rgba(20,43,54,0.92)", "#275A6E");
  text(slide, head, x + 18, y + 16, w - 36, 28, { size: 23, bold: true, color });
  text(slide, body, x + 18, y + 56, w - 36, h - 64, { size: 16, color: C.muted });
}

function bullets(slide, items, x, y, w, gap = 44, size = 18) {
  items.forEach((item, i) => {
    const yy = y + i * gap;
    slide.shapes.add({
      geometry: "ellipse",
      position: { left: x, top: yy + 7, width: 11, height: 11 },
      fill: C.accent,
      line: { style: "solid", fill: C.accent, width: 0 },
    });
    text(slide, item, x + 24, yy, w - 24, gap - 4, { size });
  });
}

function node(slide, label, x, y, w, color = C.accent2) {
  rect(slide, x, y, w, 62, "rgba(20,43,54,0.94)", color);
  text(slide, label, x + 10, y + 18, w - 20, 28, { size: 19, bold: true, color, align: "center" });
}

function stat(slide, x, y, label, value, color) {
  rect(slide, x, y, 210, 92, "rgba(20,43,54,0.92)", "#275A6E");
  text(slide, label, x + 18, y + 16, 160, 22, { size: 15, color: C.muted });
  text(slide, value, x + 18, y + 44, 170, 34, { size: 27, bold: true, color });
}

function add(p, build) {
  const slide = p.slides.add();
  bg(slide);
  build(slide, p.slides.items.length);
  return slide;
}

async function main() {
  await fs.mkdir(OUT_DIR, { recursive: true });
  await fs.mkdir(QA_DIR, { recursive: true });
  const p = Presentation.create({ slideSize: { width: W, height: H } });

  add(p, (s) => {
    text(s, "VChat", 555, 172, 240, 66, { size: 58, bold: true });
    text(s, "学生群体聊天系统", 558, 242, 300, 34, { size: 26, bold: true, color: C.accent2 });
    rule(s, 558, 296, 180);
    text(s, "面向课堂项目答辩的分享优化版", 558, 318, 330, 26, { size: 17, color: C.muted });
    text(s, "徐一豪 233020798 / 伍锐 233020799", 558, 352, 330, 24, { size: 15, color: C.muted });
  });

  add(p, (s, no) => {
    title(s, "分享重点", "把听众带进项目场景，而不是陷入实现细节。", no);
    card(s, 90, 178, 235, 110, "为什么做", "面向小组协作和课堂演示的即时沟通工具。", C.accent2);
    card(s, 362, 178, 235, 110, "怎么使用", "登录、连接、聊天、查看在线成员，流程清晰。", C.green);
    card(s, 634, 178, 235, 110, "做到哪里", "当前完成 Qt 客户端主体，服务端和数据库作为联调依赖。", C.orange);
    bullets(s, [
      "弱化实现细节，保留必要的架构和流程说明。",
      "删除端口号专题，不再把答辩焦点放在低价值细节上。",
      "内容更贴近实际程序：LoginDialog、MainWindow、QTcpSocket、在线列表。"
    ], 120, 345, 730, 40, 18);
  });

  add(p, (s, no) => {
    title(s, "答辩路线", "用 5 个问题串起整个项目。", no);
    const items = [
      ["01", "项目背景", "为什么需要 VChat"],
      ["02", "用户流程", "从登录到聊天怎么走"],
      ["03", "系统关系", "客户端、服务器、数据库怎么配合"],
      ["04", "功能亮点", "界面、验证码、在线列表"],
      ["05", "总结展望", "完成度和后续优化"]
    ];
    items.forEach(([n, h, b], i) => {
      const x = 86 + (i % 3) * 270;
      const y = 185 + Math.floor(i / 3) * 118;
      card(s, x, y, 225, 84, h, `${n}  ${b}`, C.accent2);
    });
  });

  add(p, (s, no) => {
    title(s, "项目背景", "课堂项目协作需要一个轻量、直观、可演示的沟通工具。", no);
    stat(s, 115, 190, "面向场景", "课程小组", C.accent2);
    stat(s, 375, 190, "核心目标", "即时沟通", C.green);
    stat(s, 635, 190, "技术路线", "Qt 客户端", C.orange);
    bullets(s, [
      "小组项目推进时，需要快速同步任务、讨论问题和确认进度。",
      "VChat 以桌面端为入口，适合课堂演示和局域网调试环境。",
      "项目既能体现 Qt 界面开发，也能体现 C/S 通信流程。"
    ], 120, 340, 735, 40, 18);
  });

  add(p, (s, no) => {
    title(s, "需求与功能定位", "功能不用追求大而全，重点是把核心聊天链路打通。", no);
    card(s, 82, 180, 380, 105, "登录认证", "输入账号、密码和验证码，认证成功后进入聊天主界面。", C.accent2);
    card(s, 502, 180, 380, 105, "服务器连接", "客户端填写服务器地址，通过 TCP 与聊天服务器通信。", C.green);
    card(s, 82, 325, 380, 105, "聊天收发", "支持发送消息、接收服务器转发消息和显示系统提示。", C.orange);
    card(s, 502, 325, 380, 105, "在线状态", "显示在线用户列表和在线人数，帮助用户确认当前会话状态。", C.accent2);
  });

  add(p, (s, no) => {
    title(s, "当前程序结构", "程序可以按两个核心界面来理解。", no);
    card(s, 105, 188, 310, 150, "LoginDialog", "登录窗口：服务器地址、账号密码、验证码刷新、登录结果提示。", C.accent2);
    card(s, 545, 188, 310, 150, "MainWindow", "聊天主界面：连接状态、消息输入、聊天记录、在线用户列表。", C.green);
    arrow(s, 420, 263, 540, 263);
    text(s, "登录成功后传递昵称和服务器地址", 335, 312, 300, 28, { size: 16, color: C.muted, align: "center" });
  });

  add(p, (s, no) => {
    title(s, "系统关系", "客户端负责交互，服务器负责业务和数据库访问。", no);
    arrow(s, 250, 250, 380, 250);
    arrow(s, 580, 250, 710, 250);
    node(s, "Qt 客户端", 90, 220, 160, C.accent2);
    node(s, "ChatServer", 390, 220, 180, C.green);
    node(s, "MySQL", 720, 220, 140, C.orange);
    text(s, "TCP 通信", 285, 218, 80, 24, { size: 16, color: C.muted, align: "center" });
    text(s, "数据查询/保存", 606, 218, 105, 24, { size: 16, color: C.muted, align: "center" });
    bullets(s, [
      "客户端不直接连接数据库，避免暴露数据库信息。",
      "服务器维护在线用户、转发消息，并负责访问用户表和消息表。",
      "这种分层更符合 C/S 架构，也便于后续扩展。"
    ], 120, 340, 735, 38, 18);
  });

  add(p, (s, no) => {
    title(s, "用户流程", "从用户视角看，整个项目只有几步。", no);
    const steps = [
      ["1", "启动程序", "进入登录窗口"],
      ["2", "填写信息", "服务器、账号、密码、验证码"],
      ["3", "完成认证", "通过服务器校验账号"],
      ["4", "进入聊天", "自动连接聊天服务"],
      ["5", "开始沟通", "收发消息、查看在线成员"]
    ];
    steps.forEach(([n, h, b], i) => {
      const x = 72 + i * 170;
      rect(s, x, 215, 130, 112, "rgba(20,43,54,0.94)", "#275A6E");
      text(s, n, x + 42, 222, 45, 34, { size: 28, bold: true, color: C.accent2, align: "center" });
      text(s, h, x + 14, 263, 102, 26, { size: 20, bold: true, align: "center" });
      text(s, b, x + 13, 294, 104, 34, { size: 12, color: C.muted, align: "center" });
      if (i < steps.length - 1) arrow(s, x + 130, 270, x + 165, 270);
    });
  });

  add(p, (s, no) => {
    title(s, "登录设计", "登录窗口既做用户输入，也承担进入系统前的基础校验。", no);
    card(s, 82, 178, 240, 108, "验证码", "本地生成并校验，输入错误会刷新，减少误操作。", C.green);
    card(s, 360, 178, 240, 108, "账号认证", "验证码通过后，再交给服务器验证账号密码。", C.accent2);
    card(s, 638, 178, 240, 108, "结果反馈", "失败时弹窗说明原因，成功后进入聊天主界面。", C.orange);
    bullets(s, [
      "登录流程清楚地区分了“本地校验”和“服务器校验”。",
      "登录成功后使用数据库昵称或账号作为聊天身份。",
      "服务器地址输入放在登录窗口，方便本机和局域网环境切换。"
    ], 120, 340, 735, 38, 18);
  });

  add(p, (s, no) => {
    title(s, "聊天主界面设计", "MainWindow 负责聊天体验和网络状态展示。", no);
    card(s, 88, 178, 350, 100, "聊天记录区", "用不同颜色和位置区分自己、他人和系统消息。", C.accent2);
    card(s, 522, 178, 350, 100, "消息输入区", "按钮发送和回车发送都支持，发送后清空输入框。", C.green);
    card(s, 88, 324, 350, 100, "在线用户列表", "服务器返回在线列表后刷新右侧列表和在线人数。", C.orange);
    card(s, 522, 324, 350, 100, "连接状态", "连接、断开、错误都会给出明确提示，便于演示。", C.accent2);
  });

  add(p, (s, no) => {
    title(s, "通信过程", "用流程讲清楚消息如何流动。", no);
    node(s, "输入消息", 95, 215, 130, C.accent2);
    arrow(s, 225, 246, 300, 246);
    node(s, "客户端发送", 310, 215, 150, C.green);
    arrow(s, 460, 246, 535, 246);
    node(s, "服务器转发", 545, 215, 150, C.orange);
    arrow(s, 695, 246, 770, 246);
    node(s, "客户端显示", 780, 215, 130, C.accent2);
    bullets(s, [
      "客户端发送消息前先检查连接状态。",
      "服务器负责把消息转发给其他在线客户端。",
      "客户端收到内容后分类显示：在线列表、系统提示、普通消息。"
    ], 118, 342, 735, 38, 18);
  });

  add(p, (s, no) => {
    title(s, "演示路径", "课堂上按用户操作讲，节奏更自然。", no);
    bullets(s, [
      "启动程序，先展示登录窗口和验证码刷新。",
      "输入服务器地址、账号和密码，演示错误提示或成功进入。",
      "进入聊天主界面，说明连接状态和在线用户列表。",
      "发送一条消息，展示本地消息气泡和服务器转发效果。",
      "断开或连接失败时，说明系统提示和状态变化。"
    ], 120, 175, 735, 52, 21);
  });

  add(p, (s, no) => {
    title(s, "当前完成度", "明确边界，答辩时更稳。", no);
    card(s, 82, 175, 245, 108, "已完成", "Qt 客户端界面、登录流程、验证码、连接状态、聊天收发处理。", C.green);
    card(s, 358, 175, 245, 108, "需要联调", "ChatServer 运行、数据库表、局域网多客户端测试。", C.orange);
    card(s, 634, 175, 245, 108, "可扩展", "历史消息、文件发送、重连机制、安全传输、AI 助手入口。", C.accent2);
    bullets(s, [
      "当前项目最完整的是客户端功能与通信流程。",
      "服务器和数据库作为课程要求中的对端，需要配合运行环境完成联调。",
      "答辩中重点展示客户端体验和 C/S 架构理解。"
    ], 112, 338, 760, 40, 18);
  });

  add(p, (s, no) => {
    title(s, "项目亮点", "把功能做完整，比堆复杂概念更重要。", no);
    card(s, 90, 180, 350, 98, "流程完整", "从登录、认证、连接到聊天展示，形成闭环。", C.green);
    card(s, 520, 180, 350, 98, "界面清晰", "聊天区、输入区、在线列表和状态提示分区明确。", C.accent2);
    card(s, 90, 326, 350, 98, "异常提示", "验证码错误、连接失败、未连接发送都会反馈。", C.orange);
    card(s, 520, 326, 350, 98, "扩展空间", "服务端、数据库和 AI 功能都可以继续叠加。", C.accent2);
  });

  add(p, (s, no) => {
    title(s, "后续优化方向", "从演示系统走向更完整的聊天应用。", no);
    bullets(s, [
      "安全性：增加密码加密或更安全的传输方式。",
      "稳定性：加入断线重连、心跳检测和更完善的异常处理。",
      "数据能力：支持历史消息查询、离线消息和用户资料维护。",
      "体验升级：增加头像、群聊管理、文件发送和 AI 问答入口。"
    ], 130, 176, 720, 66, 22);
  });

  add(p, (s, no) => {
    title(s, "项目总结", "VChat 的重点是把 Qt 界面、网络通信和项目流程串起来。", no);
    card(s, 88, 178, 350, 100, "技术收获", "熟悉 Qt6 Widgets、信号槽、QTcpSocket 和 UI 文件协作。", C.accent2);
    card(s, 522, 178, 350, 100, "工程收获", "围绕登录、连接、收发、列表刷新和异常提示拆分模块。", C.green);
    card(s, 88, 324, 350, 100, "协作收获", "需要统一协议、界面流程和演示路径，减少集成风险。", C.orange);
    card(s, 522, 324, 350, 100, "答辩结论", "当前客户端主体已成型，后续重点是服务端联调和功能完善。", C.accent2);
  });

  add(p, (s) => {
    text(s, "THANK YOU", 82, 190, 430, 70, { size: 56, bold: true });
    text(s, "欢迎老师和同学提问", 86, 270, 360, 34, { size: 28, color: C.accent2, bold: true });
    rule(s, 86, 328, 170);
    text(s, "VChat 学生群体聊天系统 / 课堂项目分享", 86, 354, 470, 28, { size: 17, color: C.muted });
  });

  for (const [i, slide] of p.slides.items.entries()) {
    slide.speakerNotes.textFrame.setText(`第 ${i + 1} 页讲稿提示：围绕项目背景、用户流程、系统关系、功能完成度和后续优化来讲。`);
  }

  for (const [i, slide] of p.slides.items.entries()) {
    const stem = `slide-${String(i + 1).padStart(2, "0")}`;
    await writeBlob(`${QA_DIR}/${stem}.png`, await p.export({ slide, format: "png", scale: 1 }));
    await fs.writeFile(`${QA_DIR}/${stem}.layout.json`, await (await slide.export({ format: "layout" })).text(), "utf8");
  }
  await writeBlob(`${QA_DIR}/montage.webp`, await p.export({ format: "webp", montage: true, scale: 1 }));
  await fs.writeFile(`${QA_DIR}/source-notes.txt`, [
    "Sources used:",
    "- D:/qtcode/ChatDemo_1/CMakeLists.txt",
    "- D:/qtcode/ChatDemo_1/logindialog.h and logindialog.cpp",
    "- D:/qtcode/ChatDemo_1/mainwindow.h and mainwindow.cpp",
    "- User discussion: keep classroom sharing focused and remove low-value implementation detail topics.",
    "Boundary: current workspace contains Qt client code; server/MySQL are described as integration counterparts."
  ].join("\n"), "utf8");

  const pptx = await PresentationFile.exportPptx(p);
  await pptx.save(FINAL);
  console.log(FINAL);
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
