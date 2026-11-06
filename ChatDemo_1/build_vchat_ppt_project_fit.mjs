import fs from "node:fs/promises";
import { Presentation, PresentationFile } from "file:///C:/Users/%E5%BE%90%E4%B8%80%E8%B1%AA/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/@oai/artifact-tool/dist/artifact_tool.mjs";

const OUT_DIR = "D:/qtcode/ChatDemo_1/outputs";
const QA_DIR = "D:/qtcode/ChatDemo_1/tmp/ppt_project_fit/qa";
const FINAL = `${OUT_DIR}/VChat_PPT_项目贴合版.pptx`;

const W = 960;
const H = 540;
const C = {
  bg1: "#0E2029",
  bg2: "#162C37",
  panel: "#142B36",
  panel2: "#0B1B23",
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

function setBg(slide) {
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

function rect(slide, x, y, w, h, fill = C.panel, line = C.line) {
  return slide.shapes.add({
    geometry: "roundRect",
    position: { left: x, top: y, width: w, height: h },
    fill,
    line: { style: "solid", fill: line, width: 1 },
    borderRadius: "rounded-lg",
  });
}

function line(slide, x, y, w, color = C.accent) {
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
  line(slide, 70, 118, 100);
  if (subtitle) text(slide, subtitle, 70, 132, 790, 30, { size: 16, color: C.muted });
}

function bulletList(slide, items, x, y, w, gap = 48, size = 17) {
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

function card(slide, x, y, w, h, head, body, color = C.accent2) {
  rect(slide, x, y, w, h, "rgba(20,43,54,0.92)", "#275A6E");
  text(slide, head, x + 18, y + 16, w - 36, 26, { size: 21, bold: true, color });
  text(slide, body, x + 18, y + 52, w - 36, h - 62, { size: 16, color: C.muted });
}

function code(slide, value, x, y, w, h, head) {
  rect(slide, x, y, w, h, "rgba(7,18,25,0.96)", "#24566B");
  text(slide, head, x + 18, y + 12, w - 36, 22, { size: 15, bold: true, color: C.accent2 });
  text(slide, value, x + 18, y + 42, w - 36, h - 52, {
    size: 14,
    color: "#D9F5FF",
    face: "Consolas",
  });
}

function protocol(slide, label, x, y, w = 160, color = C.accent2) {
  rect(slide, x, y, w, 58, "rgba(20,43,54,0.94)", color);
  text(slide, label, x + 10, y + 17, w - 20, 24, { size: 17, bold: true, color, align: "center" });
}

function addSlide(p, no, build) {
  const s = p.slides.add();
  setBg(s);
  build(s, no);
  return s;
}

async function main() {
  await fs.mkdir(OUT_DIR, { recursive: true });
  await fs.mkdir(QA_DIR, { recursive: true });

  const p = Presentation.create({ slideSize: { width: W, height: H } });

  addSlide(p, 1, (s) => {
    text(s, "VChat", 555, 172, 240, 66, { size: 58, bold: true });
    text(s, "学生群体聊天系统", 558, 242, 300, 34, { size: 26, bold: true, color: C.accent2 });
    line(s, 558, 296, 180);
    text(s, "基于 Qt6 的客户端实现与 C/S 通信答辩", 558, 318, 330, 26, { size: 17, color: C.muted });
    text(s, "徐一豪 233020798 / 伍锐 233020799", 558, 352, 330, 24, { size: 15, color: C.muted });
    text(s, "专业综合实战-1", 558, 382, 260, 22, { size: 14, color: C.muted });
  });

  addSlide(p, 2, (s, no) => {
    title(s, "这版 PPT 如何贴合项目", "先讲清楚当前仓库完成度，再用真实函数解释核心代码。", no);
    card(s, 84, 190, 230, 110, "当前仓库", "Qt 客户端：登录窗口、聊天主界面、TCP 通信和 UI 展示。", C.green);
    card(s, 365, 190, 230, 110, "联调依赖", "ChatServer 与 MySQL 是协议对端，答辩中按 C/S 架构说明。", C.orange);
    card(s, 646, 190, 230, 110, "代码口径", "PPT 代码来自项目核心片段，保留函数名和关键逻辑。", C.accent2);
    bulletList(s, [
      "不再把项目讲成泛泛的商业计划书，而是围绕程序运行流程展开。",
      "明确说明 8080 是默认测试端口，不是固定要求。",
      "把客户端、服务器、数据库的连接边界讲清楚。"
    ], 118, 348, 740, 38, 17);
  });

  addSlide(p, 3, (s, no) => {
    title(s, "答辩路线", "按“需求 -> 架构 -> 代码 -> 演示 -> 总结”的顺序讲。", no);
    const items = [
      ["01", "项目背景", "为什么做 VChat"],
      ["02", "真实架构", "客户端/服务器/数据库"],
      ["03", "核心流程", "登录、连接、收发消息"],
      ["04", "代码实现", "Qt 类、socket、协议解析"],
      ["05", "测试总结", "完成度、问题与后续"]
    ];
    items.forEach(([n, h, b], i) => {
      const x = 86 + (i % 3) * 270;
      const y = 185 + Math.floor(i / 3) * 118;
      card(s, x, y, 225, 84, h, `${n}  ${b}`, C.accent2);
    });
  });

  addSlide(p, 4, (s, no) => {
    title(s, "应用场景与需求", "面向课堂项目、小组协作和局域网即时沟通。", no);
    card(s, 82, 180, 380, 105, "功能需求", "登录验证、服务器地址输入、聊天消息发送/接收、在线用户列表、连接状态提示。", C.accent2);
    card(s, 502, 180, 380, 105, "非功能需求", "界面清晰、错误提示明确、TCP 数据处理稳定、用户输入显示前转义。", C.green);
    card(s, 82, 325, 380, 105, "演示需求", "可以在本机或局域网服务器上连接，默认地址为 127.0.0.1:8080。", C.orange);
    card(s, 502, 325, 380, 105, "扩展需求", "后续可加入聊天记录查询、离线消息、文件发送和 AI 问答入口。", C.accent2);
  });

  addSlide(p, 5, (s, no) => {
    title(s, "当前程序结构", "仓库中的核心代码集中在两个窗口类和一个 CMake 配置。", no);
    code(s, `ChatDemo_1
├─ CMakeLists.txt
├─ main.cpp
├─ logindialog.h / .cpp / .ui
└─ mainwindow.h  / .cpp / .ui`, 96, 178, 360, 220, "项目文件");
    bulletList(s, [
      "LoginDialog：服务器地址、账号密码、验证码、登录认证。",
      "MainWindow：聊天主界面、socket 连接、消息收发、在线列表。",
      "CMakeLists：链接 Qt::Core、Qt::Widgets、Qt::Network。"
    ], 526, 188, 340, 62, 17);
  });

  addSlide(p, 6, (s, no) => {
    title(s, "客户端、服务器、数据库怎么连", "客户端不直接连数据库，所有数据库访问应由服务器端完成。", no);
    arrow(s, 250, 250, 380, 250);
    arrow(s, 580, 250, 710, 250);
    protocol(s, "Qt 客户端", 90, 220, 160, C.accent2);
    protocol(s, "ChatServer", 390, 220, 180, C.green);
    protocol(s, "MySQL", 720, 220, 140, C.orange);
    text(s, "QTcpSocket\nIP:端口", 280, 205, 85, 48, { size: 16, color: C.muted, align: "center" });
    text(s, "SQL 查询\nusers/messages", 612, 205, 90, 56, { size: 16, color: C.muted, align: "center" });
    bulletList(s, [
      "客户端负责界面输入、TCP 通信和消息显示。",
      "服务器负责登录判断、消息转发、在线用户维护和数据库访问。",
      "数据库保存用户信息和聊天记录，客户端不暴露数据库账号。"
    ], 120, 340, 735, 38, 17);
  });

  addSlide(p, 7, (s, no) => {
    title(s, "端口号为什么是 8080", "8080 只是默认测试端口，真正要求是客户端和服务器端口一致。", no);
    code(s, `serverEdit->setText(QStringLiteral("127.0.0.1:8080"));
serverEdit->setPlaceholderText(QStringLiteral("例如 127.0.0.1:8080"));
ui->formLayout->insertRow(0, QStringLiteral("服务器:"), serverEdit);`, 80, 170, 535, 150, "LoginDialog 默认地址");
    card(s, 650, 170, 210, 78, "不是固定", "服务器监听 9000，客户端也填 9000。", C.orange);
    card(s, 650, 270, 210, 78, "方便演示", "本机调试时 127.0.0.1:8080 最直观。", C.green);
    text(s, "答辩表达：8080 是我们项目的默认测试端口，不是协议规定。", 95, 370, 720, 36, { size: 21, bold: true, color: C.accent2, align: "center" });
  });

  addSlide(p, 8, (s, no) => {
    title(s, "登录流程对应真实代码", "先做本地输入检查和验证码校验，再连接服务器验证账号密码。", no);
    protocol(s, "输入信息", 80, 215, 120);
    arrow(s, 200, 244, 260, 244);
    protocol(s, "验证码校验", 270, 215, 145, C.green);
    arrow(s, 415, 244, 475, 244);
    protocol(s, "LOGIN 请求", 485, 215, 145, C.orange);
    arrow(s, 630, 244, 690, 244);
    protocol(s, "进入主界面", 700, 215, 150, C.accent2);
    bulletList(s, [
      "tryLogin() 读取 usernameEdit、passwordEdit、captchaEdit。",
      "验证码通过后调用 authenticateWithServer(name, password, &errorMessage)。",
      "认证成功执行 accept()，main.cpp 再创建 MainWindow。"
    ], 110, 335, 730, 42, 17);
  });

  addSlide(p, 9, (s, no) => {
    title(s, "代码 1：验证码生成", "使用 QRandomGenerator 生成 5 位字符，去掉容易混淆的 I/O/0/1。", no);
    code(s, `QString LoginDialog::createCaptcha() const
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
}`, 78, 150, 555, 330, "logindialog.cpp");
    card(s, 670, 178, 185, 84, "5 位验证码", "长度固定，适合课堂演示。", C.accent2);
    card(s, 670, 288, 185, 84, "本地校验", "不区分大小写，错误后刷新。", C.green);
  });

  addSlide(p, 10, (s, no) => {
    title(s, "代码 2：登录校验", "PPT 中保留真实分支：空输入、验证码错误、服务器认证失败。", no);
    code(s, `if (name.isEmpty() || password.isEmpty()) {
    QMessageBox::warning(this, "登录失败", "请输入账号和密码。");
    return;
}

if (inputCaptcha.compare(captchaCode, Qt::CaseInsensitive) != 0) {
    QMessageBox::warning(this, "登录失败", "验证码错误，请重新输入。");
    refreshCaptcha();
    ui->captchaEdit->setFocus();
    return;
}

if (!authenticateWithServer(name, password, &errorMessage)) {
    QMessageBox::warning(this, "登录失败", errorMessage);
    refreshCaptcha();
    ui->passwordEdit->clear();
    return;
}
accept();`, 74, 145, 585, 345, "tryLogin()");
    bulletList(s, [
      "验证码仍在客户端判断。",
      "账号密码交给 ChatServer 查询 users 表。",
      "成功后关闭登录框并进入主窗口。"
    ], 690, 186, 210, 64, 16);
  });

  addSlide(p, 11, (s, no) => {
    title(s, "代码 3：服务器地址解析", "项目要求地址格式为 IP:端口，端口会转成 quint16。", no);
    code(s, `const int separatorIndex = address.lastIndexOf(':');
if (separatorIndex <= 0 || separatorIndex == address.size() - 1) {
    *errorMessage = "服务器地址格式应为 IP:端口";
    return false;
}

const QString host = address.left(separatorIndex).trimmed();
bool portOk = false;
const quint16 port =
    address.mid(separatorIndex + 1).trimmed().toUShort(&portOk);

if (!portOk || port == 0) {
    *errorMessage = "服务器端口不正确。";
    return false;
}`, 74, 150, 560, 325, "authenticateWithServer()");
    card(s, 670, 178, 190, 94, "host", "例如 127.0.0.1 或局域网 IP。", C.accent2);
    card(s, 670, 305, 190, 94, "port", "必须和服务器 listen 端口一致。", C.orange);
  });

  addSlide(p, 12, (s, no) => {
    title(s, "代码 4：LOGIN 协议", "登录阶段使用临时 QTcpSocket，只负责账号认证。", no);
    code(s, `QTcpSocket socket;
socket.connectToHost(host, port);
if (!socket.waitForConnected(3000)) {
    *errorMessage = "连接登录服务器失败：" + socket.errorString();
    return false;
}

const QString request =
    QStringLiteral("LOGIN|%1|%2\\n").arg(username, password);
socket.write(request.toUtf8());

if (response.startsWith(QStringLiteral("LOGIN_OK|"))) {
    loggedInNickname =
        response.mid(QStringLiteral("LOGIN_OK|").size()).trimmed();
    return true;
}`, 70, 145, 575, 335, "authenticateWithServer()");
    protocol(s, "LOGIN|账号|密码", 685, 182, 190, C.orange);
    arrow(s, 780, 245, 780, 290);
    protocol(s, "LOGIN_OK|昵称", 685, 305, 190, C.green);
    text(s, "失败时返回 LOGIN_FAIL|原因", 662, 390, 235, 28, { size: 16, color: C.muted, align: "center" });
  });

  addSlide(p, 13, (s, no) => {
    title(s, "代码 5：主窗口自动连接", "登录成功后，登录窗口把昵称和服务器地址传给 MainWindow。", no);
    code(s, `MainWindow::MainWindow(const QString &loginName,
                       const QString &serverAddress,
                       QWidget *parent)
    : QMainWindow(parent)
    , nickname(loginName.trimmed().isEmpty()
        ? QStringLiteral("User") : loginName.trimmed())
    , isConnected(false)
    , socket(new QTcpSocket(this))
{
    ui->setupUi(this);
    setupUI();
    connectSlots();
    updateUserList();

    if (!serverAddress.trimmed().isEmpty()) {
        connectToServerAddress(serverAddress, false);
    }
}`, 70, 145, 585, 335, "MainWindow 构造函数");
    bulletList(s, [
      "nickname 用于消息发送者和在线列表。",
      "socket 是正式聊天连接。",
      "serverAddress 来自登录窗口。"
    ], 685, 188, 210, 64, 16);
  });

  addSlide(p, 14, (s, no) => {
    title(s, "代码 6：网络信号槽", "连接成功、断开、可读、错误分别对应不同界面状态。", no);
    code(s, `connect(socket, &QTcpSocket::connected, this, [this]() {
    isConnected = true;
    ui->serverStatusLabel->setText("服务器状态: 已连接");
    appendSystemMessage("已连接到聊天服务器");
    sendJoinMessage();
});

connect(socket, &QTcpSocket::disconnected, this, [this]() {
    isConnected = false;
    ui->serverStatusLabel->setText("服务器状态: 未连接");
    updateUserList();
});

connect(socket, &QTcpSocket::readyRead,
        this, &MainWindow::on_socketReadyRead);`, 72, 150, 585, 330, "connectSlots()");
    card(s, 690, 172, 180, 78, "connected", "更新状态并发送 JOIN。", C.green);
    card(s, 690, 272, 180, 78, "disconnected", "恢复未连接状态。", C.orange);
    card(s, 690, 372, 180, 78, "readyRead", "读取服务器消息。", C.accent2);
  });

  addSlide(p, 15, (s, no) => {
    title(s, "代码 7：连接服务器函数", "手动连接和自动连接都复用 connectToServerAddress()。", no);
    code(s, `bool MainWindow::connectToServerAddress(
    const QString &address, bool showErrorMessage)
{
    const QString serverAddress = address.trimmed();
    const int separatorIndex = serverAddress.lastIndexOf(':');
    const QString host = serverAddress.left(separatorIndex).trimmed();
    const quint16 port =
        serverAddress.mid(separatorIndex + 1).trimmed().toUShort(&portOk);

    if (socket->state() != QAbstractSocket::UnconnectedState) {
        socket->abort();
    }

    appendSystemMessage("正在连接服务器: " + host + ":" + QString::number(port));
    socket->connectToHost(host, port);
    return true;
}`, 72, 145, 595, 340, "mainwindow.cpp");
    bulletList(s, [
      "格式不对会提示。",
      "旧连接存在时先 abort。",
      "真正连接由 connectToHost 发起。"
    ], 700, 190, 195, 62, 16);
  });

  addSlide(p, 16, (s, no) => {
    title(s, "代码 8：发送消息", "发送前检查连接状态，消息以换行符作为边界。", no);
    code(s, `const QString message = ui->messageInput->text().trimmed();
if (message.isEmpty()) {
    return;
}

if (!isConnected ||
    socket->state() != QAbstractSocket::ConnectedState) {
    appendSystemMessage("请先连接服务器");
    return;
}

const QString networkMessage = nickname + ": " + message + "\\n";
if (socket->write(networkMessage.toUtf8()) == -1) {
    appendSystemMessage("消息发送失败: " + socket->errorString());
    return;
}

appendMessage(nickname, message);
ui->messageInput->clear();`, 72, 145, 595, 340, "on_sendButton_clicked()");
    card(s, 700, 176, 180, 82, "关键点 1", "空消息不发送。", C.green);
    card(s, 700, 284, 180, 82, "关键点 2", "未连接时提示用户。", C.orange);
    card(s, 700, 392, 180, 82, "关键点 3", "\\n 作为消息边界。", C.accent2);
  });

  addSlide(p, 17, (s, no) => {
    title(s, "代码 9：TCP 拆包处理", "TCP 是流式传输，所以项目用 receiveBuffer 缓存再按行拆分。", no);
    code(s, `receiveBuffer.append(socket->readAll());

int lineEnd = -1;
while ((lineEnd = receiveBuffer.indexOf('\\n')) != -1) {
    const QByteArray lineBytes =
        receiveBuffer.left(lineEnd).trimmed();
    receiveBuffer.remove(0, lineEnd + 1);

    if (!lineBytes.isEmpty()) {
        appendIncomingLine(QString::fromUtf8(lineBytes));
    }
}`, 76, 158, 540, 300, "on_socketReadyRead()");
    protocol(s, "readAll()", 675, 170, 150);
    arrow(s, 750, 230, 750, 274);
    protocol(s, "receiveBuffer", 675, 288, 150, C.green);
    arrow(s, 750, 348, 750, 392);
    protocol(s, "按 \\n 拆行", 675, 406, 150, C.orange);
  });

  addSlide(p, 18, (s, no) => {
    title(s, "代码 10：消息分发", "服务器发来的行数据被分成三类处理。", no);
    code(s, `if (line.startsWith("USERLIST|")) {
    QStringList users =
        line.mid(QStringLiteral("USERLIST|").size())
            .split(',', Qt::SkipEmptyParts);
    updateUserList(users);
    return;
}

if (line.startsWith("[System]")) {
    appendSystemMessage(line);
    return;
}

appendMessage("服务器转发", line);`, 80, 155, 520, 300, "appendIncomingLine()");
    card(s, 650, 170, 210, 72, "USERLIST", "刷新右侧在线用户列表。", C.accent2);
    card(s, 650, 274, 210, 72, "[System]", "显示服务器系统提示。", C.orange);
    card(s, 650, 378, 210, 72, "普通消息", "进入聊天气泡显示。", C.green);
  });

  addSlide(p, 19, (s, no) => {
    title(s, "代码 11：界面展示与安全细节", "聊天内容转成 HTML 前先转义，避免特殊字符破坏气泡布局。", no);
    code(s, `const QString safeUser = user.toHtmlEscaped();
const QString safeMessage = message.toHtmlEscaped();

if (user == nickname) {
    // 自己发的消息靠右显示，绿色气泡
} else {
    // 其他人的消息靠左显示，蓝色气泡
}

ui->chatDisplay->append(html);
ui->chatDisplay->verticalScrollBar()
    ->setValue(ui->chatDisplay->verticalScrollBar()->maximum());`, 78, 160, 560, 292, "appendMessage()");
    card(s, 680, 180, 185, 84, "toHtmlEscaped", "防止 < > & 等字符影响 HTML。", C.green);
    card(s, 680, 305, 185, 84, "自动滚动", "新消息出现后滚到最新位置。", C.accent2);
  });

  addSlide(p, 20, (s, no) => {
    title(s, "演示路径", "课堂答辩时可以按这条路线现场讲解。", no);
    const steps = [
      ["1", "启动程序", "main.cpp 先显示 LoginDialog"],
      ["2", "输入地址", "默认 127.0.0.1:8080，可改为局域网 IP"],
      ["3", "验证码登录", "本地验证码 + 服务器账号认证"],
      ["4", "进入聊天", "MainWindow 自动连接并发送 JOIN"],
      ["5", "发送消息", "socket 写入，服务端转发，客户端显示"]
    ];
    steps.forEach(([n, h, b], i) => {
      const x = 74 + i * 170;
      rect(s, x, 210, 132, 118, "rgba(20,43,54,0.94)", "#275A6E");
      text(s, n, x + 43, 220, 45, 36, { size: 30, bold: true, color: C.accent2, align: "center" });
      text(s, h, x + 16, 262, 100, 26, { size: 19, bold: true, align: "center" });
      text(s, b, x + 14, 294, 104, 34, { size: 12, color: C.muted, align: "center" });
      if (i < steps.length - 1) arrow(s, x + 132, 270, x + 165, 270);
    });
  });

  addSlide(p, 21, (s, no) => {
    title(s, "完成度与边界", "这页用来回答老师问“做到哪一步了”。", no);
    card(s, 82, 175, 245, 108, "已完成", "Qt 客户端登录界面、验证码、地址输入、主窗口、发送/接收消息处理。", C.green);
    card(s, 358, 175, 245, 108, "需要联调", "ChatServer 运行、MySQL users/messages 表、局域网多客户端测试。", C.orange);
    card(s, 634, 175, 245, 108, "可扩展", "密码加密、重连机制、历史消息、文件发送、AI 助手入口。", C.accent2);
    bulletList(s, [
      "当前 PPT 中涉及服务器和数据库的内容，按协议对端和课程要求说明。",
      "当前仓库可直接证明的是 Qt 客户端代码和通信协议适配。",
      "答辩时不要说客户端直接连数据库，应该说服务器端负责数据库访问。"
    ], 112, 338, 760, 40, 17);
  });

  addSlide(p, 22, (s, no) => {
    title(s, "项目总结", "VChat 的价值在于把 Qt 界面、网络编程和项目流程串起来。", no);
    card(s, 88, 178, 350, 100, "技术收获", "熟悉 Qt6 Widgets、信号槽、QTcpSocket、CMake 和 UI 文件协作。", C.accent2);
    card(s, 522, 178, 350, 100, "工程收获", "围绕协议拆分登录、连接、收发、列表刷新和异常提示。", C.green);
    card(s, 88, 324, 350, 100, "调试重点", "验证码刷新、服务器地址格式、TCP 粘包/半包、断线状态同步。", C.orange);
    card(s, 522, 324, 350, 100, "后续方向", "补齐服务端部署、安全传输、消息持久化查询和更完整的多端测试。", C.accent2);
  });

  addSlide(p, 23, (s) => {
    text(s, "THANK YOU", 82, 190, 430, 70, { size: 56, bold: true });
    text(s, "欢迎老师和同学提问", 86, 270, 360, 34, { size: 28, color: C.accent2, bold: true });
    line(s, 86, 328, 170);
    text(s, "VChat 学生群体聊天系统 / Qt6 客户端实现", 86, 354, 470, 28, { size: 17, color: C.muted });
  });

  for (const [i, slide] of p.slides.items.entries()) {
    slide.speakerNotes.textFrame.setText(`第 ${i + 1} 页讲稿提示：围绕页面标题讲，代码页强调它和当前 Qt 项目文件的对应关系。`);
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
    "",
    "Boundary:",
    "- Current workspace contains Qt client code. Server/MySQL are described as protocol/integration counterparts, not as code present in this repository."
  ].join("\n"), "utf8");

  const pptx = await PresentationFile.exportPptx(p);
  await pptx.save(FINAL);
  console.log(FINAL);
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
