import fs from "node:fs/promises";
import { Presentation, PresentationFile } from "file:///C:/Users/%E5%BE%90%E4%B8%80%E8%B1%AA/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/@oai/artifact-tool/dist/artifact_tool.mjs";

const OUT_DIR = "D:/qtcode/ChatDemo_1/outputs";
const QA_DIR = "D:/qtcode/ChatDemo_1/tmp/ppt_balanced/qa";
const FINAL = `${OUT_DIR}/VChat_PPT_答辩平衡版.pptx`;
const W = 960;
const H = 540;

const C = {
  bg1: "#0E2029",
  bg2: "#162C37",
  panel: "#142B36",
  panel2: "#071219",
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
    typeface: opt.face ?? "Microsoft YaHei",
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
  text(slide, head, x + 18, y + 16, w - 36, 26, { size: 21, bold: true, color });
  text(slide, body, x + 18, y + 52, w - 36, h - 60, { size: 16, color: C.muted });
}

function bullets(slide, items, x, y, w, gap = 44, size = 17) {
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

function codeBox(slide, value, x, y, w, h, head) {
  rect(slide, x, y, w, h, "rgba(7,18,25,0.96)", "#24566B");
  text(slide, head, x + 18, y + 12, w - 36, 22, { size: 15, bold: true, color: C.accent2 });
  text(slide, value, x + 18, y + 42, w - 36, h - 52, {
    size: 13,
    color: "#D9F5FF",
    face: "Consolas",
  });
}

function protocol(slide, label, x, y, w = 150, color = C.accent2) {
  rect(slide, x, y, w, 58, "rgba(20,43,54,0.94)", color);
  text(slide, label, x + 8, y + 17, w - 16, 24, { size: 17, bold: true, color, align: "center" });
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
    text(s, "贴合当前 Qt 客户端代码的课堂答辩版", 558, 318, 330, 26, { size: 17, color: C.muted });
    text(s, "徐一豪 233020798 / 伍锐 233020799", 558, 352, 330, 24, { size: 15, color: C.muted });
  });

  add(p, (s, no) => {
    title(s, "项目与程序完成度", "这版 PPT 重点说明当前程序真实完成了什么。", no);
    card(s, 82, 178, 240, 105, "当前仓库", "Qt 客户端：登录窗口、验证码、聊天主界面、socket 通信和在线列表显示。", C.green);
    card(s, 360, 178, 240, 105, "联调依赖", "ChatServer 与 MySQL 是协议对端，答辩时按 C/S 架构说明。", C.orange);
    card(s, 638, 178, 240, 105, "代码口径", "只展示关键代码片段，不把完整函数全部塞进 PPT。", C.accent2);
    bullets(s, [
      "客户端不直接连接数据库，数据库访问应由服务器端完成。",
      "8080 是默认测试端口，服务器改端口时客户端也要同步修改。",
      "PPT 中涉及代码的部分都对应当前项目里的真实函数。"
    ], 118, 340, 740, 40, 18);
  });

  add(p, (s, no) => {
    title(s, "答辩路线", "项目介绍为主，代码只作为关键实现支撑。", no);
    const items = [
      ["01", "需求背景", "为什么做 VChat"],
      ["02", "系统架构", "客户端/服务器/数据库"],
      ["03", "功能流程", "登录、连接、聊天"],
      ["04", "关键代码", "验证码、socket、消息分发"],
      ["05", "测试总结", "完成度、问题与改进"]
    ];
    items.forEach(([n, h, b], i) => {
      const x = 86 + (i % 3) * 270;
      const y = 185 + Math.floor(i / 3) * 118;
      card(s, x, y, 225, 84, h, `${n}  ${b}`, C.accent2);
    });
  });

  add(p, (s, no) => {
    title(s, "需求与应用场景", "适合课堂项目、小组协作和局域网即时沟通。", no);
    card(s, 82, 180, 380, 105, "功能需求", "登录验证、服务器地址输入、聊天消息发送/接收、在线用户列表、连接状态提示。", C.accent2);
    card(s, 502, 180, 380, 105, "非功能需求", "界面清晰、错误提示明确、TCP 数据处理稳定、用户输入显示前转义。", C.green);
    card(s, 82, 325, 380, 105, "演示需求", "可以在本机或局域网服务器上连接，默认地址为 127.0.0.1:8080。", C.orange);
    card(s, 502, 325, 380, 105, "扩展需求", "后续可加入聊天记录查询、离线消息、文件发送和 AI 问答入口。", C.accent2);
  });

  add(p, (s, no) => {
    title(s, "当前程序结构", "真实项目主要由登录窗口、聊天主窗口和 CMake 配置组成。", no);
    codeBox(s, `ChatDemo_1
├─ CMakeLists.txt
├─ main.cpp
├─ logindialog.h / .cpp / .ui
└─ mainwindow.h  / .cpp / .ui`, 96, 178, 360, 220, "项目文件");
    bullets(s, [
      "LoginDialog：服务器地址、账号密码、验证码、登录认证。",
      "MainWindow：聊天主界面、socket 连接、消息收发、在线列表。",
      "CMakeLists：链接 Qt::Core、Qt::Widgets、Qt::Network。"
    ], 526, 188, 340, 62, 18);
  });

  add(p, (s, no) => {
    title(s, "客户端、服务器、数据库连接关系", "客户端只连接 ChatServer，不直接暴露数据库连接。", no);
    arrow(s, 250, 250, 380, 250);
    arrow(s, 580, 250, 710, 250);
    protocol(s, "Qt 客户端", 90, 220, 160, C.accent2);
    protocol(s, "ChatServer", 390, 220, 180, C.green);
    protocol(s, "MySQL", 720, 220, 140, C.orange);
    text(s, "QTcpSocket\nIP:端口", 280, 205, 85, 48, { size: 16, color: C.muted, align: "center" });
    text(s, "SQL 查询\nusers/messages", 612, 205, 90, 56, { size: 16, color: C.muted, align: "center" });
    bullets(s, [
      "客户端负责界面输入、TCP 通信和消息显示。",
      "服务器负责登录判断、消息转发、在线用户维护和数据库访问。",
      "数据库保存用户信息和聊天记录，客户端不直接连接数据库。"
    ], 120, 340, 735, 38, 18);
  });

  add(p, (s, no) => {
    title(s, "端口号 8080 的说明", "8080 只是默认测试端口，真正要求是客户端和服务器一致。", no);
    codeBox(s, `serverEdit->setText(QStringLiteral("127.0.0.1:8080"));
serverEdit->setPlaceholderText(QStringLiteral("例如 127.0.0.1:8080"));
ui->formLayout->insertRow(0, QStringLiteral("服务器:"), serverEdit);`, 80, 170, 535, 150, "LoginDialog 默认地址");
    card(s, 650, 170, 210, 78, "不是固定", "服务器监听 9000，客户端也填 9000。", C.orange);
    card(s, 650, 270, 210, 78, "方便演示", "本机调试时 127.0.0.1:8080 最直观。", C.green);
    text(s, "答辩表达：8080 是项目默认测试端口，不是协议规定。", 95, 380, 720, 36, { size: 23, bold: true, color: C.accent2, align: "center" });
  });

  add(p, (s, no) => {
    title(s, "核心流程：从登录到聊天", "流程与 main.cpp、LoginDialog、MainWindow 的调用关系一致。", no);
    const steps = [
      ["1", "启动", "main.cpp 显示登录框"],
      ["2", "校验", "账号密码 + 验证码"],
      ["3", "认证", "LOGIN 请求给服务器"],
      ["4", "连接", "MainWindow 正式连接"],
      ["5", "聊天", "发送消息并接收转发"]
    ];
    steps.forEach(([n, h, b], i) => {
      const x = 72 + i * 170;
      rect(s, x, 215, 130, 110, "rgba(20,43,54,0.94)", "#275A6E");
      text(s, n, x + 42, 222, 45, 34, { size: 28, bold: true, color: C.accent2, align: "center" });
      text(s, h, x + 18, 263, 94, 26, { size: 20, bold: true, align: "center" });
      text(s, b, x + 14, 294, 102, 32, { size: 12, color: C.muted, align: "center" });
      if (i < steps.length - 1) arrow(s, x + 130, 270, x + 165, 270);
    });
  });

  add(p, (s, no) => {
    title(s, "关键代码 1：验证码与登录校验", "这页只保留能说明登录逻辑的核心片段。", no);
    codeBox(s, `captchaCode = createCaptcha();
ui->captchaCodeLabel->setText(captchaCode);

if (inputCaptcha.compare(captchaCode, Qt::CaseInsensitive) != 0) {
    QMessageBox::warning(this, "登录失败", "验证码错误，请重新输入。");
    refreshCaptcha();
    return;
}

if (!authenticateWithServer(name, password, &errorMessage)) {
    QMessageBox::warning(this, "登录失败", errorMessage);
    refreshCaptcha();
    return;
}
accept();`, 70, 150, 570, 325, "logindialog.cpp");
    bullets(s, [
      "验证码在客户端本地判断。",
      "账号密码交给 ChatServer 查询 users 表。",
      "成功后 accept() 进入主窗口。"
    ], 675, 190, 220, 64, 17);
  });

  add(p, (s, no) => {
    title(s, "关键代码 2：服务器认证协议", "登录阶段使用临时 socket，正式聊天阶段再建立主窗口 socket。", no);
    codeBox(s, `QTcpSocket socket;
socket.connectToHost(host, port);

const QString request =
    QStringLiteral("LOGIN|%1|%2\\n").arg(username, password);
socket.write(request.toUtf8());

if (response.startsWith(QStringLiteral("LOGIN_OK|"))) {
    loggedInNickname =
        response.mid(QStringLiteral("LOGIN_OK|").size()).trimmed();
    return true;
}`, 70, 150, 555, 310, "authenticateWithServer()");
    protocol(s, "LOGIN|账号|密码", 680, 185, 190, C.orange);
    arrow(s, 775, 250, 775, 295);
    protocol(s, "LOGIN_OK|昵称", 680, 310, 190, C.green);
    text(s, "失败则返回 LOGIN_FAIL|原因", 660, 395, 235, 28, { size: 16, color: C.muted, align: "center" });
  });

  add(p, (s, no) => {
    title(s, "关键代码 3：聊天收发", "发送前检查连接状态，接收时用 receiveBuffer 处理 TCP 流。", no);
    codeBox(s, `const QString networkMessage = nickname + ": " + message + "\\n";
socket->write(networkMessage.toUtf8());

receiveBuffer.append(socket->readAll());
while ((lineEnd = receiveBuffer.indexOf('\\n')) != -1) {
    const QByteArray lineBytes = receiveBuffer.left(lineEnd).trimmed();
    receiveBuffer.remove(0, lineEnd + 1);
    appendIncomingLine(QString::fromUtf8(lineBytes));
}`, 70, 155, 585, 295, "mainwindow.cpp");
    card(s, 690, 176, 180, 80, "发送", "昵称: 消息内容\\n", C.green);
    card(s, 690, 286, 180, 80, "接收", "缓存后按 \\n 拆行", C.accent2);
    text(s, "重点：TCP 不是一条消息一次 readyRead。", 650, 405, 260, 32, { size: 16, color: C.orange, bold: true, align: "center" });
  });

  add(p, (s, no) => {
    title(s, "关键代码 4：消息分类显示", "服务器返回不同前缀，客户端进入不同处理逻辑。", no);
    codeBox(s, `if (line.startsWith("USERLIST|")) {
    updateUserList(users);
    return;
}

if (line.startsWith("[System]")) {
    appendSystemMessage(line);
    return;
}

appendMessage("服务器转发", line);`, 80, 160, 520, 270, "appendIncomingLine()");
    card(s, 650, 170, 210, 72, "USERLIST", "刷新在线用户列表。", C.accent2);
    card(s, 650, 274, 210, 72, "[System]", "显示系统提示。", C.orange);
    card(s, 650, 378, 210, 72, "普通消息", "进入聊天气泡。", C.green);
  });

  add(p, (s, no) => {
    title(s, "界面与交互细节", "程序不只是能连上，还要让用户知道当前状态。", no);
    card(s, 90, 180, 350, 98, "登录界面", "动态添加服务器地址输入框，刷新验证码，错误时弹窗提示。", C.accent2);
    card(s, 520, 180, 350, 98, "聊天主界面", "显示连接状态、聊天记录、消息输入框和在线用户列表。", C.green);
    card(s, 90, 326, 350, 98, "消息气泡", "自己消息靠右绿色气泡，其他消息靠左蓝色气泡，系统消息居中。", C.orange);
    card(s, 520, 326, 350, 98, "安全细节", "appendMessage() 里使用 toHtmlEscaped() 转义用户内容。", C.accent2);
  });

  add(p, (s, no) => {
    title(s, "演示路径", "答辩时可以按这条路径现场讲，不需要逐行讲代码。", no);
    bullets(s, [
      "启动程序，先出现 LoginDialog。",
      "确认服务器地址，默认 127.0.0.1:8080，也可以换成局域网 IP。",
      "输入账号、密码、验证码，验证码错误会刷新。",
      "登录成功进入 MainWindow，自动连接服务器并发送 JOIN。",
      "发送聊天消息，观察聊天气泡、系统提示和在线列表变化。"
    ], 120, 175, 735, 52, 21);
  });

  add(p, (s, no) => {
    title(s, "完成度与项目边界", "这页回答“现在做到哪一步了”。", no);
    card(s, 82, 175, 245, 108, "已完成", "Qt 客户端登录界面、验证码、地址输入、主窗口、发送/接收消息处理。", C.green);
    card(s, 358, 175, 245, 108, "需要联调", "ChatServer 运行、MySQL users/messages 表、局域网多客户端测试。", C.orange);
    card(s, 634, 175, 245, 108, "可扩展", "密码加密、重连机制、历史消息、文件发送、AI 助手入口。", C.accent2);
    bullets(s, [
      "当前仓库能直接证明的是 Qt 客户端代码。",
      "服务器和数据库内容按协议对端与课程要求说明。",
      "答辩时应强调：客户端不直接连数据库。"
    ], 112, 338, 760, 40, 18);
  });

  add(p, (s, no) => {
    title(s, "项目总结", "VChat 把 Qt 界面、网络编程和项目流程串成了可演示系统。", no);
    card(s, 88, 178, 350, 100, "技术收获", "熟悉 Qt6 Widgets、信号槽、QTcpSocket、CMake 和 UI 文件协作。", C.accent2);
    card(s, 522, 178, 350, 100, "工程收获", "围绕协议拆分登录、连接、收发、列表刷新和异常提示。", C.green);
    card(s, 88, 324, 350, 100, "调试重点", "验证码刷新、服务器地址格式、TCP 粘包/半包、断线状态同步。", C.orange);
    card(s, 522, 324, 350, 100, "后续方向", "补齐服务端部署、安全传输、消息持久化查询和更完整的多端测试。", C.accent2);
  });

  add(p, (s) => {
    text(s, "THANK YOU", 82, 190, 430, 70, { size: 56, bold: true });
    text(s, "欢迎老师和同学提问", 86, 270, 360, 34, { size: 28, color: C.accent2, bold: true });
    rule(s, 86, 328, 170);
    text(s, "VChat 学生群体聊天系统 / Qt6 客户端实现", 86, 354, 470, 28, { size: 17, color: C.muted });
  });

  for (const [i, slide] of p.slides.items.entries()) {
    slide.speakerNotes.textFrame.setText(`第 ${i + 1} 页讲稿提示：项目介绍为主，代码页只讲关键逻辑和当前项目文件的对应关系。`);
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
    "- User discussion about client/server/database connection and default 8080 port.",
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
