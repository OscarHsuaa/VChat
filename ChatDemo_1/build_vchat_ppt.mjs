import fs from "node:fs/promises";
import { Presentation, PresentationFile } from "file:///C:/Users/%E5%BE%90%E4%B8%80%E8%B1%AA/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/@oai/artifact-tool/dist/artifact_tool.mjs";

const OUT_DIR = "D:/qtcode/ChatDemo_1/outputs";
const QA_DIR = "D:/qtcode/ChatDemo_1/tmp/ppt_vchat/final_qa";
const FINAL = `${OUT_DIR}/VChat项目课堂答辩PPT.pptx`;

const W = 960;
const H = 540;
const C = {
  bg: "#11212B",
  panel: "#152A36",
  panel2: "#1D3544",
  accent: "#00AEEF",
  accent2: "#2DD4FF",
  text: "#F4FBFF",
  muted: "#A9C0CA",
  line: "#2B5D73",
  green: "#46D07E",
  orange: "#FFB84D",
  red: "#FF6B6B",
};

async function writeBlob(path, blob) {
  await fs.writeFile(path, new Uint8Array(await blob.arrayBuffer()));
}

function textBox(slide, text, x, y, w, h, style = {}) {
  const shape = slide.shapes.add({
    geometry: "textbox",
    position: { left: x, top: y, width: w, height: h },
    fill: "none",
    line: { style: "solid", fill: "none", width: 0 },
  });
  shape.text = text;
  shape.text.style = {
    typeface: "Microsoft YaHei",
    fontSize: style.size ?? 22,
    color: style.color ?? C.text,
    bold: style.bold ?? false,
    alignment: style.align ?? "left",
    verticalAlignment: style.valign ?? "top",
    ...style.extra,
  };
  return shape;
}

function codeBox(slide, code, x, y, w, h, titleText = "") {
  rect(slide, x, y, w, h, "rgba(8,21,29,0.96)", "#24566B", "rounded-lg");
  if (titleText) {
    textBox(slide, titleText, x + 18, y + 12, w - 36, 22, {
      size: 14,
      bold: true,
      color: C.accent2,
    });
  }
  const shape = slide.shapes.add({
    geometry: "textbox",
    position: { left: x + 18, top: y + (titleText ? 42 : 18), width: w - 36, height: h - (titleText ? 54 : 30) },
    fill: "none",
    line: { style: "solid", fill: "none", width: 0 },
  });
  shape.text = code;
  shape.text.style = {
    typeface: "Consolas",
    fontSize: 12,
    color: "#D8F3FF",
    verticalAlignment: "top",
  };
  return shape;
}

function rect(slide, x, y, w, h, fill = C.panel, line = C.line, radius = "rounded-lg") {
  return slide.shapes.add({
    geometry: "roundRect",
    position: { left: x, top: y, width: w, height: h },
    fill,
    line: { style: "solid", fill: line, width: 1 },
    borderRadius: radius,
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

function brand(slide, no) {
  rect(slide, 26, 14, 142, 30, "#0F202A", "#0F202A", "rounded-sm");
  textBox(slide, "VChat", 35, 20, 120, 28, { size: 18, bold: true, color: C.accent2 });
  textBox(slide, `0${no}`.slice(-2), 875, 22, 40, 22, { size: 12, color: C.muted, align: "right" });
}

function title(slide, t, sub = "", no = 1) {
  brand(slide, no);
  textBox(slide, t, 66, 62, 720, 48, { size: 30, bold: true });
  rule(slide, 66, 116, 94);
  if (sub) textBox(slide, sub, 66, 126, 780, 36, { size: 14, color: C.muted });
}

function clearTemplateContent(slide) {
  slide.background.fill = "linear(180deg, #0F202A 0%, #172A35 100%)";
  for (const shape of [...slide.shapes.items]) {
    shape.delete();
  }
  for (const image of [...slide.images.items]) {
    image.delete();
  }
  for (const table of [...slide.tables.items]) {
    table.delete?.();
  }
  for (const chart of [...slide.charts.items]) {
    chart.delete?.();
  }
  slide.shapes.add({
    geometry: "ellipse",
    position: { left: 560, top: -90, width: 430, height: 430 },
    fill: "rgba(255,255,255,0.025)",
    line: { style: "solid", fill: "rgba(255,255,255,0.03)", width: 1 },
  });
  slide.shapes.add({
    geometry: "ellipse",
    position: { left: -150, top: 330, width: 360, height: 360 },
    fill: "rgba(0,174,239,0.035)",
    line: { style: "solid", fill: "rgba(0,174,239,0.04)", width: 1 },
  });
}

function bullets(slide, items, x, y, w, gap = 46, size = 18) {
  items.forEach((item, i) => {
    const yy = y + i * gap;
    slide.shapes.add({
      geometry: "ellipse",
      position: { left: x, top: yy + 5, width: 12, height: 12 },
      fill: C.accent,
      line: { style: "solid", fill: C.accent, width: 0 },
    });
    textBox(slide, item, x + 24, yy, w - 24, 38, { size, color: C.text });
  });
}

function statCard(slide, x, y, w, h, k, v, color = C.accent) {
  rect(slide, x, y, w, h, "rgba(20,45,58,0.88)", "#23546A");
  textBox(slide, k, x + 18, y + 16, w - 36, 28, { size: 15, color: C.muted });
  textBox(slide, v, x + 18, y + 48, w - 36, 42, { size: 28, bold: true, color });
}

function diagramNode(slide, label, x, y, w = 128, h = 52, fill = C.panel2) {
  rect(slide, x, y, w, h, fill, C.accent);
  textBox(slide, label, x + 8, y + 14, w - 16, h - 10, { size: 16, bold: true, align: "center" });
}

function arrow(slide, x1, y1, x2, y2) {
  slide.shapes.add({
    geometry: "line",
    position: { left: x1, top: y1, width: x2 - x1, height: y2 - y1 },
    line: { style: "solid", fill: C.accent2, width: 2, endArrowType: "triangle" },
  });
}

async function main() {
  await fs.mkdir(OUT_DIR, { recursive: true });
  await fs.mkdir(QA_DIR, { recursive: true });

  const presentation = Presentation.create({ slideSize: { width: W, height: H } });
  for (let i = 0; i < 17; i++) {
    const slide = presentation.slides.add();
    clearTemplateContent(slide);
  }
  const slides = presentation.slides.items;

  // 1 Cover
  {
    const s = slides[0];
    textBox(s, "VChat", 542, 192, 230, 62, { size: 48, bold: true, color: C.text });
    textBox(s, "学生群体聊天系统", 542, 252, 300, 38, { size: 25, bold: true, color: C.accent2 });
    rule(s, 542, 305, 190);
    textBox(s, "专业综合实战-1 课堂答辩", 542, 324, 300, 26, { size: 15, color: C.muted });
    textBox(s, "徐一豪 233020798 / 伍锐 233020799", 542, 356, 330, 22, { size: 13, color: C.muted });
    textBox(s, "Qt6 Widgets + TCP Socket + ChatServer/MySQL", 542, 390, 360, 24, { size: 13, color: C.muted });
  }

  // 2 Agenda
  {
    const s = slides[1];
    title(s, "答辩目录", "从需求、设计、实现、测试到总结，快速说明项目完成度。", 2);
    const items = [
      ["01", "项目背景", "为什么做校园即时沟通工具"],
      ["02", "需求分析", "用户、功能与非功能需求"],
      ["03", "系统设计", "客户端结构与通信协议"],
      ["04", "核心实现", "登录认证、验证码、消息收发"],
      ["05", "测试总结", "演示路径、问题与展望"],
    ];
    items.forEach(([n, h, b], i) => {
      const x = 80 + (i % 3) * 270;
      const y = 185 + Math.floor(i / 3) * 118;
      rect(s, x, y, 225, 84, "rgba(18,43,56,0.9)", "#236278");
      textBox(s, n, x + 16, y + 14, 45, 28, { size: 22, bold: true, color: C.accent2 });
      textBox(s, h, x + 66, y + 12, 120, 24, { size: 19, bold: true });
      textBox(s, b, x + 66, y + 42, 140, 22, { size: 12, color: C.muted });
    });
  }

  // 3 Background
  {
    const s = slides[2];
    title(s, "项目背景", "面向课程小组、实验室和课堂演示场景的轻量级即时通信工具。", 3);
    statCard(s, 86, 186, 190, 112, "使用场景", "小组协作", C.accent2);
    statCard(s, 306, 186, 190, 112, "核心目标", "实时沟通", C.green);
    statCard(s, 526, 186, 190, 112, "技术路线", "Qt + TCP", C.orange);
    bullets(s, [
      "课程项目协作需要稳定、清晰、可演示的即时消息工具。",
      "本项目以桌面客户端为主体，聚焦登录认证、聊天收发和在线状态。",
      "后端通过 ChatServer 与 MySQL 完成账号验证和消息持久化扩展。"
    ], 112, 338, 760, 42, 17);
  }

  // 4 Requirements
  {
    const s = slides[3];
    title(s, "需求分析", "围绕“能登录、能连接、能聊天、能展示在线状态”组织功能。", 4);
    const cards = [
      ["登录认证", "服务器地址、账号密码、随机验证码，验证通过后进入主界面。"],
      ["聊天通信", "通过 QTcpSocket 与服务器建立连接，支持发送和接收消息。"],
      ["在线状态", "解析 USERLIST 协议，刷新在线用户列表和在线人数。"],
      ["异常处理", "地址格式错误、连接失败、断开连接时给出明确提示。"],
    ];
    cards.forEach(([h, b], i) => {
      const x = 86 + (i % 2) * 390;
      const y = 176 + Math.floor(i / 2) * 126;
      rect(s, x, y, 338, 92, "rgba(18,43,56,0.92)", "#236278");
      textBox(s, h, x + 22, y + 16, 120, 24, { size: 19, bold: true, color: C.accent2 });
      textBox(s, b, x + 22, y + 46, 288, 36, { size: 13, color: C.muted });
    });
  }

  // 5 Architecture
  {
    const s = slides[4];
    title(s, "系统整体设计", "当前仓库完成 Qt 客户端主体，服务端与数据库作为联调依赖。", 5);
    diagramNode(s, "LoginDialog\n登录窗口", 80, 220, 150, 62);
    diagramNode(s, "MainWindow\n聊天主界面", 292, 220, 160, 62);
    diagramNode(s, "ChatServer\n消息转发/登录验证", 520, 220, 180, 62);
    diagramNode(s, "MySQL\nusers/messages", 760, 220, 130, 62);
    arrow(s, 230, 251, 292, 251);
    arrow(s, 452, 251, 520, 251);
    arrow(s, 700, 251, 760, 251);
    bullets(s, [
      "客户端：Qt6 Widgets + Qt Network，界面与网络事件通过信号槽连接。",
      "协议：LOGIN、JOIN、USERLIST 和普通消息行，均以换行符作为消息边界。",
      "数据：账号验证与聊天记录持久化预留在服务器和 MySQL 层完成。"
    ], 98, 348, 780, 40, 16);
  }

  // 6 Flow
  {
    const s = slides[5];
    title(s, "核心流程设计", "登录阶段使用临时 socket；聊天阶段再建立正式连接。", 6);
    const steps = [
      ["1", "输入服务器/账号/密码/验证码"],
      ["2", "本地校验验证码"],
      ["3", "发送 LOGIN 请求"],
      ["4", "成功后进入主窗口"],
      ["5", "发送 JOIN 并开始聊天"],
    ];
    steps.forEach(([n, t], i) => {
      const x = 82 + i * 165;
      rect(s, x, 205, 128, 96, "rgba(18,43,56,0.95)", "#236278");
      textBox(s, n, x + 42, 214, 44, 38, { size: 28, bold: true, color: C.accent2, align: "center" });
      textBox(s, t, x + 14, 258, 100, 35, { size: 13, color: C.text, align: "center" });
      if (i < steps.length - 1) arrow(s, x + 130, 252, x + 160, 252);
    });
    textBox(s, "关键点：TCP 是流式传输，readyRead 中先进入 receiveBuffer，再按换行拆成完整消息。", 126, 360, 720, 36, { size: 17, color: C.muted, align: "center" });
  }

  // 7 Implementation
  {
    const s = slides[6];
    title(s, "核心实现", "围绕登录、网络、消息展示三个模块拆分代码。", 7);
    const cols = [
      ["验证码", "QRandomGenerator 生成 5 位验证码，去除 I/O/0/1 等易混字符，输入错误后自动刷新。"],
      ["登录认证", "连接服务器并发送 LOGIN|username|password，等待 LOGIN_OK 或 LOGIN_FAIL。"],
      ["消息展示", "自己消息右侧绿色气泡，其他消息左侧蓝色气泡，系统提示居中显示。"],
    ];
    cols.forEach(([h, b], i) => {
      const x = 76 + i * 285;
      rect(s, x, 180, 250, 210, "rgba(18,43,56,0.92)", "#236278");
      textBox(s, h, x + 22, 204, 160, 28, { size: 22, bold: true, color: C.accent2 });
      textBox(s, b, x + 22, 248, 205, 96, { size: 15, color: C.text });
    });
    textBox(s, "涉及文件：logindialog.cpp / mainwindow.cpp / CMakeLists.txt", 96, 430, 760, 24, { size: 13, color: C.muted, align: "center" });
  }

  // 8 Code: project structure
  {
    const s = slides[7];
    title(s, "代码实现 1：项目结构", "CMake 将界面、登录窗口和聊天主窗口组织成一个 Qt6 客户端。", 8);
    codeBox(s, `find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network)

qt_add_executable(ChatDemo_1
    main.cpp
    logindialog.cpp  logindialog.h  logindialog.ui
    mainwindow.cpp   mainwindow.h   mainwindow.ui
)

target_link_libraries(ChatDemo_1
    PRIVATE Qt::Core Qt::Widgets Qt::Network
)`, 86, 166, 470, 250, "CMakeLists.txt");
    bullets(s, [
      "Widgets 负责登录窗口与聊天主界面。",
      "Network 提供 QTcpSocket，用于登录认证和聊天通信。",
      "UI 文件与 C++ 槽函数配合，降低界面逻辑耦合。"
    ], 592, 186, 285, 58, 15);
  }

  // 9 Code: captcha
  {
    const s = slides[8];
    title(s, "代码实现 2：随机验证码", "验证码在客户端本地校验，先挡住明显错误输入，再进入服务器认证。", 9);
    codeBox(s, `QString LoginDialog::createCaptcha() const
{
    static const QString chars =
        QStringLiteral("ABCDEFGHJKLMNPQRSTUVWXYZ23456789");
    QString code;
    code.reserve(5);

    for (int i = 0; i < 5; ++i) {
        const int index =
            QRandomGenerator::global()->bounded(chars.size());
        code.append(chars.at(index));
    }
    return code;
}`, 76, 158, 520, 300, "logindialog.cpp");
    statCard(s, 642, 178, 190, 88, "长度", "5 位", C.accent2);
    statCard(s, 642, 292, 190, 88, "体验", "去混淆字符", C.green);
    textBox(s, "输入错误后 refreshCaptcha() 会重新生成验证码，并清空输入框。", 632, 406, 230, 42, { size: 14, color: C.muted });
  }

  // 10 Code: login validation
  {
    const s = slides[9];
    title(s, "代码实现 3：登录认证流程", "验证码通过后，再连接 ChatServer 查询 users 表。", 10);
    codeBox(s, `if (inputCaptcha.compare(captchaCode, Qt::CaseInsensitive) != 0) {
    QMessageBox::warning(this, "登录失败", "验证码错误，请重新输入。");
    refreshCaptcha();
    ui->captchaEdit->setFocus();
    return;
}

QString errorMessage;
if (!authenticateWithServer(name, password, &errorMessage)) {
    QMessageBox::warning(this, "登录失败", errorMessage);
    refreshCaptcha();
    ui->passwordEdit->clear();
    return;
}

accept();`, 76, 152, 520, 330, "tryLogin()");
    bullets(s, [
      "本地校验：账号密码不能为空，验证码不区分大小写。",
      "服务器校验：账号密码交给 ChatServer，不在客户端直接访问数据库。",
      "成功路径：accept() 关闭登录窗口，进入 MainWindow。"
    ], 630, 174, 260, 64, 15);
  }

  // 11 Code: protocol
  {
    const s = slides[10];
    title(s, "代码实现 4：登录协议与服务器响应", "使用轻量文本协议，便于调试和课堂演示。", 11);
    codeBox(s, `socket.connectToHost(host, port);
if (!socket.waitForConnected(3000)) {
    *errorMessage = "连接登录服务器失败：" + socket.errorString();
    return false;
}

const QString request =
    QStringLiteral("LOGIN|%1|%2\\n").arg(username, password);
socket.write(request.toUtf8());

if (response.startsWith(QStringLiteral("LOGIN_OK|"))) {
    loggedInNickname = response.mid(QStringLiteral("LOGIN_OK|").size()).trimmed();
    return true;
}`, 68, 150, 565, 330, "authenticateWithServer()");
    rect(s, 676, 176, 190, 58, "rgba(18,43,56,0.92)", "#236278");
    textBox(s, "LOGIN|账号|密码", 694, 196, 154, 24, { size: 16, bold: true, color: C.accent2, align: "center" });
    arrow(s, 770, 246, 770, 286);
    rect(s, 676, 296, 190, 58, "rgba(18,43,56,0.92)", "#236278");
    textBox(s, "LOGIN_OK|昵称", 694, 316, 154, 24, { size: 16, bold: true, color: C.green, align: "center" });
    textBox(s, "失败时返回 LOGIN_FAIL|原因，用于弹窗提示。", 660, 384, 220, 40, { size: 13, color: C.muted, align: "center" });
  }

  // 12 Code: send and connect
  {
    const s = slides[11];
    title(s, "代码实现 5：连接服务器与发送消息", "主窗口连接成功后发送 JOIN，聊天消息以换行结束。", 12);
    codeBox(s, `connect(socket, &QTcpSocket::connected, this, [this]() {
    isConnected = true;
    ui->serverStatusLabel->setText("服务器状态: 已连接");
    sendJoinMessage();
});

const QString networkMessage = nickname + ": " + message + "\\n";
if (socket->write(networkMessage.toUtf8()) == -1) {
    appendSystemMessage("消息发送失败: " + socket->errorString());
    return;
}

appendMessage(nickname, message);
ui->messageInput->clear();`, 76, 152, 548, 328, "MainWindow");
    bullets(s, [
      "发送前检查 socket 是否处于 ConnectedState。",
      "JOIN|昵称 用来通知服务器刷新在线列表。",
      "本地立即显示自己发送的消息，提升交互反馈。"
    ], 660, 178, 235, 66, 15);
  }

  // 13 Code: receive buffer
  {
    const s = slides[12];
    title(s, "代码实现 6：TCP 拆包与消息分发", "readyRead 不等于一条完整消息，因此需要缓存再按行解析。", 13);
    codeBox(s, `receiveBuffer.append(socket->readAll());

int lineEnd = -1;
while ((lineEnd = receiveBuffer.indexOf('\\n')) != -1) {
    const QByteArray lineBytes =
        receiveBuffer.left(lineEnd).trimmed();
    receiveBuffer.remove(0, lineEnd + 1);

    if (!lineBytes.isEmpty()) {
        appendIncomingLine(QString::fromUtf8(lineBytes));
    }
}`, 74, 154, 530, 300, "on_socketReadyRead()");
    diagramNode(s, "readAll()", 650, 176, 150, 48);
    diagramNode(s, "receiveBuffer", 650, 256, 150, 48);
    diagramNode(s, "按 \\n 拆行", 650, 336, 150, 48);
    arrow(s, 725, 224, 725, 256);
    arrow(s, 725, 304, 725, 336);
  }

  // 14 Code: message render
  {
    const s = slides[13];
    title(s, "代码实现 7：消息展示与在线列表", "对服务器消息分类处理，同时避免 HTML 特殊字符破坏界面。", 14);
    codeBox(s, `if (line.startsWith("USERLIST|")) {
    QStringList users = line.mid(QStringLiteral("USERLIST|").size())
        .split(',', Qt::SkipEmptyParts);
    updateUserList(users);
    return;
}

if (line.startsWith("[System]")) {
    appendSystemMessage(line);
    return;
}

appendMessage("服务器转发", line);`, 78, 154, 520, 302, "appendIncomingLine()");
    rect(s, 642, 168, 210, 72, "rgba(18,43,56,0.92)", "#236278");
    textBox(s, "USERLIST", 666, 184, 160, 24, { size: 18, bold: true, color: C.accent2, align: "center" });
    textBox(s, "刷新右侧在线用户", 666, 212, 160, 18, { size: 12, color: C.muted, align: "center" });
    rect(s, 642, 264, 210, 72, "rgba(18,43,56,0.92)", "#236278");
    textBox(s, "[System]", 666, 280, 160, 24, { size: 18, bold: true, color: C.orange, align: "center" });
    textBox(s, "居中显示系统提示", 666, 308, 160, 18, { size: 12, color: C.muted, align: "center" });
    rect(s, 642, 360, 210, 72, "rgba(18,43,56,0.92)", "#236278");
    textBox(s, "普通消息", 666, 376, 160, 24, { size: 18, bold: true, color: C.green, align: "center" });
    textBox(s, "进入聊天气泡", 666, 404, 160, 18, { size: 12, color: C.muted, align: "center" });
  }

  // 15 Test / completion
  {
    const s = slides[14];
    title(s, "测试与当前完成度", "客户端主体已经成型，完整演示需要配合服务器运行。", 15);
    rect(s, 100, 150, 720, 310, "rgba(15,32,42,0.96)", "#203E4C");
    const bars = [
      ["登录界面", 92, C.green],
      ["验证码", 90, C.green],
      ["服务器连接", 82, C.accent2],
      ["聊天收发", 85, C.accent2],
      ["在线列表", 78, C.orange],
    ];
    bars.forEach(([name, value, color], i) => {
      const y = 178 + i * 50;
      textBox(s, name, 130, y, 100, 22, { size: 15, color: C.text });
      rect(s, 245, y + 4, 460, 13, "rgba(255,255,255,0.08)", "#2A5062", "rounded-sm");
      rect(s, 245, y + 4, 460 * value / 100, 13, color, color, "rounded-sm");
      textBox(s, `${value}%`, 722, y - 2, 60, 22, { size: 14, color: C.muted });
    });
    textBox(s, "说明：百分比表示课堂演示准备度，不代表商业化完成度。", 130, 444, 660, 22, { size: 12, color: C.muted });
  }

  // 16 SWOT
  {
    const s = slides[15];
    title(s, "项目亮点与不足", "用 SWOT 方式总结项目状态，答辩时可以直接展开讲。", 16);
    const items = [
      ["S", "优势", "模块清晰；Qt 信号槽与网络通信结合完整；界面反馈明确。", C.green],
      ["W", "不足", "密码传输仍是明文协议；服务端代码不在当前客户端仓库中。", C.red],
      ["O", "机会", "可扩展历史记录、文件发送、AI 问答、班级通知等功能。", C.accent2],
      ["T", "挑战", "多人并发、异常重连、数据安全和跨网络部署需要继续完善。", C.orange],
    ];
    items.forEach(([letter, h, b, color], i) => {
      const x = 92 + (i % 2) * 392;
      const y = 174 + Math.floor(i / 2) * 126;
      rect(s, x, y, 330, 94, "rgba(18,43,56,0.92)", "#236278");
      textBox(s, letter, x + 18, y + 18, 45, 44, { size: 32, bold: true, color, align: "center" });
      textBox(s, h, x + 78, y + 16, 110, 24, { size: 18, bold: true });
      textBox(s, b, x + 78, y + 44, 220, 34, { size: 13, color: C.muted });
    });
  }

  // 17 Future
  {
    const s = slides[16];
    title(s, "后续优化方向", "从安全性、稳定性和体验三个方向继续迭代。", 17);
    const list = [
      ["安全", "加入密码哈希或 TLS，避免明文口令在网络中传输。"],
      ["稳定", "完善自动重连、心跳检测、服务器断开后的状态恢复。"],
      ["数据", "补充聊天记录查询、离线消息和用户资料管理。"],
      ["体验", "加入头像、群聊管理、文件发送和 AI 助手入口。"],
    ];
    list.forEach(([h, b], i) => {
      const y = 172 + i * 72;
      textBox(s, `0${i + 1}`, 122, y, 42, 30, { size: 22, bold: true, color: C.accent2 });
      textBox(s, h, 178, y, 92, 28, { size: 20, bold: true });
      textBox(s, b, 282, y + 2, 560, 28, { size: 16, color: C.muted });
      rule(s, 178, y + 42, 560, "#244A5C");
    });
  }

  const summary = presentation.slides.add();
  clearTemplateContent(summary);
  title(summary, "项目总结与心得", "本项目把界面、网络、协议和异常处理串成了一个可演示系统。", 18);
  const blocks = [
    ["技术收获", "熟悉 Qt6 Widgets、信号槽、QTcpSocket 与 CMake 项目组织。"],
    ["工程收获", "先约定协议，再拆分登录、聊天、在线列表等模块。"],
    ["调试经验", "重点处理验证码状态、地址校验、TCP 粘包/半包和断线状态。"],
    ["协作体会", "小组需要统一接口、演示路径和答辩分工，减少最后集成风险。"],
  ];
  blocks.forEach(([h, b], i) => {
    const x = 90 + (i % 2) * 390;
    const y = 174 + Math.floor(i / 2) * 122;
    rect(summary, x, y, 335, 90, "rgba(18,43,56,0.92)", "#236278");
    textBox(summary, h, x + 22, y + 18, 120, 26, { size: 19, bold: true, color: C.accent2 });
    textBox(summary, b, x + 22, y + 48, 286, 28, { size: 13, color: C.muted });
  });

  const thanks = presentation.slides.add();
  clearTemplateContent(thanks);
  textBox(thanks, "THANK YOU", 82, 198, 390, 64, { size: 46, bold: true, color: C.text });
  textBox(thanks, "欢迎老师和同学提问", 86, 276, 360, 34, { size: 24, color: C.accent2 });
  rule(thanks, 86, 328, 160);
  textBox(thanks, "VChat 学生群体聊天系统 / 专业综合实战-1", 86, 352, 480, 24, { size: 14, color: C.muted });

  const allSlides = presentation.slides.items;
  allSlides.forEach((slide, i) => {
    slide.speakerNotes.textFrame.setText(`第 ${i + 1} 页讲稿提示：围绕页面标题展开，结合实际演示说明 VChat 当前完成度和后续改进。`);
  });

  await writeBlob(`${QA_DIR}/montage.webp`, await presentation.export({ format: "webp", montage: true, scale: 1 }));
  for (const [i, slide] of allSlides.entries()) {
    const stem = `slide-${String(i + 1).padStart(2, "0")}`;
    await writeBlob(`${QA_DIR}/${stem}.png`, await presentation.export({ slide, format: "png", scale: 1 }));
    const layout = await slide.export({ format: "layout" });
    await fs.writeFile(`${QA_DIR}/${stem}.layout.json`, await layout.text(), "utf8");
  }

  const pptx = await PresentationFile.exportPptx(presentation);
  await pptx.save(FINAL);

  await fs.writeFile(`${QA_DIR}/source-notes.txt`, [
    "Sources used:",
    "- Current workspace CMakeLists.txt: Qt6 Core, Widgets, Network modules and executable files.",
    "- Current workspace logindialog.cpp/.h: server address input, captcha generation, LOGIN protocol authentication.",
    "- Current workspace mainwindow.cpp/.h: QTcpSocket connection, JOIN/USERLIST handling, message buffer splitting, chat bubble display.",
    "- Previous generated experiment report: VChat project framing and course report narrative.",
    "- Source PPTX template: 商业计划书.pptx, reused for dark technology style and slide rhythm.",
    "",
    "Claim boundary:",
    "- The current repository contains the Qt client. Server/MySQL parts are described as integration dependencies/protocol counterparts unless shown in the workspace.",
  ].join("\n"), "utf8");
  console.log(FINAL);
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
