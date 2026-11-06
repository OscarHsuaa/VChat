from docx import Document
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.enum.table import WD_CELL_VERTICAL_ALIGNMENT
from docx.shared import Pt, RGBColor
from docx.oxml.ns import qn


INPUT = "成都锦城学院实验报告_工作副本.docx"
OUTPUT = "成都锦城学院实验报告_VChat_已填写.docx"


def set_run_font(run, size=10.5, bold=False, color=None, font="宋体"):
    run.font.name = font
    run._element.rPr.rFonts.set(qn("w:eastAsia"), font)
    run.font.size = Pt(size)
    run.bold = bold
    if color:
        run.font.color.rgb = RGBColor(*color)


def add_para(cell, text="", size=10.5, bold=False, align=None, before=0, after=4, color=None):
    p = cell.add_paragraph()
    p.paragraph_format.space_before = Pt(before)
    p.paragraph_format.space_after = Pt(after)
    p.paragraph_format.line_spacing = 1.15
    if align is not None:
        p.alignment = align
    r = p.add_run(text)
    set_run_font(r, size=size, bold=bold, color=color)
    return p


def add_heading(cell, text):
    return add_para(cell, text, size=11, bold=True, before=5, after=3, color=(31, 78, 121))


def add_body(cell, text):
    return add_para(cell, text, size=10.5, after=4)


def add_code(cell, text):
    p = cell.add_paragraph()
    p.paragraph_format.space_before = Pt(2)
    p.paragraph_format.space_after = Pt(4)
    p.paragraph_format.line_spacing = 1.0
    r = p.add_run(text)
    set_run_font(r, size=8.5, font="Consolas")
    return p


def clear_cell(cell):
    for paragraph in cell.paragraphs:
        paragraph._element.getparent().remove(paragraph._element)


def set_cell_text(cell, text, size=10.5, bold=False, align=WD_ALIGN_PARAGRAPH.CENTER):
    clear_cell(cell)
    p = cell.add_paragraph()
    p.alignment = align
    p.paragraph_format.space_after = Pt(0)
    r = p.add_run(text)
    set_run_font(r, size=size, bold=bold)
    cell.vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER


def fill_content_cell(cell):
    clear_cell(cell)
    add_heading(cell, "1、选题背景")
    add_body(cell, "本项目选题为“VChat 学生群体聊天系统”。在课程学习和小组协作中，学生经常需要进行即时沟通、资料讨论和项目进度同步，因此设计一个面向校园学习场景的轻量级聊天系统具有现实意义。系统采用 Qt6 开发桌面客户端，通过 TCP 与聊天服务器通信，服务器负责账号验证、消息转发、在线用户维护和数据库记录保存。")
    add_body(cell, "应用场景包括：小组课程设计讨论、实验室局域网即时沟通、课堂演示环境下的多用户消息收发，以及后续扩展为班级通知、历史消息查询和智能问答入口等功能。")

    add_heading(cell, "2、需求分析")
    add_body(cell, "功能需求：（1）用户启动程序后先进入登录界面；（2）支持服务器地址输入，便于连接本机或局域网服务器；（3）登录时完成账号、密码和验证码校验；（4）登录成功后进入聊天主界面；（5）支持发送消息、接收服务器转发消息、显示系统提示；（6）支持在线用户列表与在线人数刷新；（7）支持断开连接、重新连接和修改昵称等常用操作。")
    add_body(cell, "非功能需求：界面布局应符合用户习惯，输入框、聊天记录区、在线用户列表应清晰分区；网络通信应能处理 TCP 分包问题；用户输入显示到 HTML 聊天气泡前需要转义，避免特殊字符破坏界面；连接异常要有明确提示，提升系统可用性。")

    add_heading(cell, "3、系统整体设计")
    add_body(cell, "系统采用 C/S 架构。客户端由 LoginDialog 和 MainWindow 两个核心界面组成：LoginDialog 负责服务器地址、账号密码和验证码输入，MainWindow 负责聊天显示、消息发送、在线用户展示和连接状态管理。客户端通过 QTcpSocket 与 ChatServer 通信，服务器端再连接 MySQL 数据库完成 users 表验证和消息记录保存。")
    add_body(cell, "主要模块设计：登录验证模块、随机验证码模块、网络连接模块、消息收发模块、在线用户模块、界面展示模块。客户端与服务器之间采用简单文本协议，例如 LOGIN|username|password 表示登录请求，JOIN|nickname 表示用户上线，USERLIST|A,B,C 表示在线用户列表，普通聊天消息以换行符结束。")
    add_body(cell, "数据库设计思路：users 表保存用户账号、密码和昵称；messages 表保存发送者、消息内容、发送时间等字段。通过数据库持久化，可以为后续实现聊天记录查询、用户管理和权限控制预留空间。")

    add_heading(cell, "4、核心流程设计")
    add_body(cell, "登录流程：程序启动后显示登录窗口，用户输入服务器地址、账号、密码和验证码。客户端先检查账号密码是否为空，再用不区分大小写的方式校验本地验证码。验证码通过后，客户端建立临时 TCP 连接，向服务器发送 LOGIN 请求；服务器查询 MySQL 后返回 LOGIN_OK 或 LOGIN_FAIL。若登录成功，登录窗口关闭并打开主聊天窗口。")
    add_body(cell, "聊天流程：主窗口创建后根据登录界面传入的服务器地址自动连接服务器。连接成功后发送 JOIN|昵称，服务器据此更新在线列表。用户点击发送按钮或在输入框按回车时，客户端先判断连接状态，再将“昵称: 消息内容”写入 socket。收到服务器数据时，客户端先缓存字节流，再按换行符拆分完整消息，分别处理在线列表、系统消息和普通聊天消息。")

    add_heading(cell, "5、系统实现")
    add_body(cell, "（1）随机验证码实现：验证码使用 QRandomGenerator 生成 5 位字符，并去掉 I、O、0、1 等易混淆字符，降低用户输入错误概率。")
    add_code(cell, "QString LoginDialog::createCaptcha() const\n{\n    static const QString chars = QStringLiteral(\"ABCDEFGHJKLMNPQRSTUVWXYZ23456789\");\n    QString code;\n    for (int i = 0; i < 5; ++i) {\n        int index = QRandomGenerator::global()->bounded(chars.size());\n        code.append(chars.at(index));\n    }\n    return code;\n}")
    add_body(cell, "（2）登录认证实现：客户端使用临时 QTcpSocket 连接服务器，发送 LOGIN|用户名|密码 协议数据，并等待服务器返回 LOGIN_OK 或 LOGIN_FAIL。该设计将数据库访问集中在服务器端，客户端不直接暴露数据库连接信息。")
    add_code(cell, "const QString request = QStringLiteral(\"LOGIN|%1|%2\\n\").arg(username, password);\nsocket.write(request.toUtf8());\nif (response.startsWith(QStringLiteral(\"LOGIN_OK|\"))) {\n    loggedInNickname = response.mid(QStringLiteral(\"LOGIN_OK|\").size()).trimmed();\n    return true;\n}")
    add_body(cell, "（3）消息收发实现：TCP 是流式协议，不能假设一次 readyRead 就是一条完整消息。因此程序使用 receiveBuffer 缓冲数据，再按 '\\n' 拆分完整行，避免半包、粘包导致消息显示异常。")
    add_code(cell, "receiveBuffer.append(socket->readAll());\nwhile ((lineEnd = receiveBuffer.indexOf('\\n')) != -1) {\n    QByteArray lineBytes = receiveBuffer.left(lineEnd).trimmed();\n    receiveBuffer.remove(0, lineEnd + 1);\n    appendIncomingLine(QString::fromUtf8(lineBytes));\n}")
    add_body(cell, "运行效果：登录窗口可以输入服务器地址并刷新验证码；验证码、账号密码验证通过后进入 VChat 主界面；主界面显示连接状态、聊天记录、消息输入框和在线用户列表；发送消息后本地以右侧绿色气泡显示，接收消息以左侧蓝色气泡显示，系统提示居中显示。")


def fill_result_cell(cell):
    clear_cell(cell)
    add_heading(cell, "项目总结")
    add_body(cell, "本次综合实战完成了 VChat 学生群体聊天系统客户端的主要功能，实现了登录验证、随机验证码、TCP 服务器连接、聊天消息发送与接收、在线用户列表刷新、连接状态提示和昵称修改等内容。项目能够较完整地体现 Qt 界面开发、信号槽机制、网络编程和客户端交互设计的综合应用。")
    add_heading(cell, "结果分析")
    add_body(cell, "从功能测试结果看，系统在服务器正常运行时可以完成账号登录和进入聊天界面；连接成功后会自动发送 JOIN 消息并刷新在线状态；发送按钮和回车发送均可触发消息发送；断开服务器或地址格式错误时，界面能够给出提示。对 TCP 数据采用缓存拆行处理后，消息接收过程更加稳定。")
    add_body(cell, "项目亮点在于登录流程和聊天流程分离：登录阶段使用临时 socket 完成账号验证，主窗口再建立正式聊天连接；同时验证码在客户端本地完成，减少服务器压力。聊天显示使用 HTML 气泡样式，并对用户内容进行 toHtmlEscaped 转义，兼顾了界面美观和安全性。")
    add_heading(cell, "存在问题与展望")
    add_body(cell, "当前系统仍有可改进之处：密码传输仍为明文文本协议，后续可加入哈希或 TLS 加密；数据库表可继续扩展用户头像、班级、角色等字段；聊天记录查询、离线消息、文件发送和 AI 问答入口也可以作为后续功能。若进一步完善服务器端部署与异常重连机制，系统的实用性会更强。")
    add_heading(cell, "个人心得体会")
    add_body(cell, "通过本次实验，我进一步熟悉了 Qt 项目的整体开发流程，也体会到界面、网络和数据处理需要协同设计。开发中遇到的主要问题包括验证码刷新后输入框状态处理、服务器地址格式校验、TCP 消息拆包以及断线后的界面状态同步。通过逐步调试和阅读 Qt 文档，我将这些问题拆分到不同函数中解决，使程序结构更加清晰。")
    add_body(cell, "小组协作方面，项目需要先统一协议格式和界面流程，再分别实现登录、聊天、数据库验证等模块。这个过程让我认识到需求分析和接口约定的重要性，也提高了我从实际项目角度组织代码、测试功能和总结问题的能力。")


def main():
    doc = Document(INPUT)

    # Normalize the checkbox glyph to avoid emoji fallback in Word renderers.
    for table in doc.tables:
        for row in table.rows:
            for cell in row.cells:
                for p in cell.paragraphs:
                    for run in p.runs:
                        run.text = run.text.replace("☑️", "☑")

    report_table = doc.tables[1]
    set_cell_text(report_table.rows[3].cells[1], "VChat 学生群体聊天系统", bold=True)
    fill_content_cell(report_table.rows[8].cells[0])

    result_table = doc.tables[2]
    fill_result_cell(result_table.rows[0].cells[0])

    # Light formatting pass for existing table cells.
    for table in doc.tables:
        for row in table.rows:
            for cell in row.cells:
                cell.vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER
                for p in cell.paragraphs:
                    for run in p.runs:
                        if run.text.strip():
                            if not run.font.size:
                                set_run_font(run, size=10.5)

    doc.save(OUTPUT)
    print(OUTPUT)


if __name__ == "__main__":
    main()
