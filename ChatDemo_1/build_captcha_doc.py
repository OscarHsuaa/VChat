from docx import Document
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.enum.table import WD_TABLE_ALIGNMENT, WD_CELL_VERTICAL_ALIGNMENT
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Inches, Pt, RGBColor


OUT_PATH = "随机验证码功能的实现_整理版.docx"


def set_run_font(run, name="Microsoft YaHei", size=None, bold=None, color=None):
    run.font.name = name
    run._element.rPr.rFonts.set(qn("w:eastAsia"), name)
    if size:
        run.font.size = Pt(size)
    if bold is not None:
        run.bold = bold
    if color:
        run.font.color.rgb = RGBColor.from_string(color)


def set_paragraph_font(paragraph, name="Microsoft YaHei", size=11, color=None):
    for run in paragraph.runs:
        set_run_font(run, name=name, size=size, color=color)


def shade_cell(cell, fill):
    tc_pr = cell._tc.get_or_add_tcPr()
    shd = OxmlElement("w:shd")
    shd.set(qn("w:fill"), fill)
    tc_pr.append(shd)


def set_cell_text(cell, text, bold=False, fill=None):
    cell.text = ""
    p = cell.paragraphs[0]
    run = p.add_run(text)
    set_run_font(run, size=10.5, bold=bold)
    if fill:
        shade_cell(cell, fill)
    cell.vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER


def add_heading(doc, text, level=1):
    p = doc.add_heading(level=level)
    p.paragraph_format.space_before = Pt(10 if level == 1 else 6)
    p.paragraph_format.space_after = Pt(6)
    run = p.add_run(text)
    set_run_font(run, size=16 if level == 1 else 13, bold=True, color="1F4D78")
    return p


def add_body(doc, text):
    p = doc.add_paragraph()
    p.paragraph_format.line_spacing = 1.2
    p.paragraph_format.space_after = Pt(6)
    run = p.add_run(text)
    set_run_font(run, size=11)
    return p


def add_bullet(doc, text):
    p = doc.add_paragraph(style="List Bullet")
    p.paragraph_format.space_after = Pt(4)
    run = p.add_run(text)
    set_run_font(run, size=11)
    return p


def add_number(doc, text):
    p = doc.add_paragraph(style="List Number")
    p.paragraph_format.space_after = Pt(4)
    run = p.add_run(text)
    set_run_font(run, size=11)
    return p


def add_code_block(doc, code):
    table = doc.add_table(rows=1, cols=1)
    table.alignment = WD_TABLE_ALIGNMENT.CENTER
    table.autofit = False
    table.columns[0].width = Inches(6.3)
    cell = table.cell(0, 0)
    shade_cell(cell, "F3F4F6")
    cell.vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER
    p = cell.paragraphs[0]
    p.paragraph_format.space_before = Pt(4)
    p.paragraph_format.space_after = Pt(4)
    for i, line in enumerate(code.splitlines()):
        if i:
            p.add_run().add_break()
        run = p.add_run(line)
        set_run_font(run, name="Consolas", size=9.5, color="111827")
    doc.add_paragraph()


def build():
    doc = Document()

    section = doc.sections[0]
    section.top_margin = Inches(0.8)
    section.bottom_margin = Inches(0.8)
    section.left_margin = Inches(0.9)
    section.right_margin = Inches(0.9)

    styles = doc.styles
    normal = styles["Normal"]
    normal.font.name = "Microsoft YaHei"
    normal._element.rPr.rFonts.set(qn("w:eastAsia"), "Microsoft YaHei")
    normal.font.size = Pt(11)

    title = doc.add_paragraph()
    title.alignment = WD_ALIGN_PARAGRAPH.CENTER
    title.paragraph_format.space_after = Pt(8)
    run = title.add_run("随机验证码功能的实现")
    set_run_font(run, size=24, bold=True, color="0B2545")

    subtitle = doc.add_paragraph()
    subtitle.alignment = WD_ALIGN_PARAGRAPH.CENTER
    subtitle.paragraph_format.space_after = Pt(14)
    run = subtitle.add_run("学生群体聊天系统课后分享文档")
    set_run_font(run, size=13, color="555555")

    info = doc.add_table(rows=4, cols=2)
    info.alignment = WD_TABLE_ALIGNMENT.CENTER
    info.autofit = False
    labels = ["项目名称", "分享功能点", "技术关键词", "演示目标"]
    values = [
        "学生群体聊天系统",
        "登录界面的随机验证码功能",
        "Qt Widgets、QDialog、QLineEdit、QLabel、QRandomGenerator、信号与槽",
        "说明验证码如何生成、如何刷新、如何参与登录校验，并现场展示错误与正确输入效果。",
    ]
    for row, label, value in zip(info.rows, labels, values):
        row.cells[0].width = Inches(1.3)
        row.cells[1].width = Inches(5.0)
        set_cell_text(row.cells[0], label, bold=True, fill="E8EEF5")
        set_cell_text(row.cells[1], value)

    add_heading(doc, "一、为什么选择随机验证码作为分享点", 1)
    add_body(
        doc,
        "本次课后分享选择“随机验证码功能”作为项目中的小功能点。它虽然功能范围不大，但包含界面设计、随机数生成、用户输入校验、信号与槽响应等多个 Qt 开发中的常用知识点，适合作为课堂展示内容。"
    )
    add_bullet(doc, "从用户角度看：验证码可以防止随意登录，增强登录流程的真实感。")
    add_bullet(doc, "从开发角度看：验证码功能能体现界面控件、业务逻辑和事件响应之间的配合。")
    add_bullet(doc, "从展示角度看：可以现场演示“刷新验证码、输错验证码、输对验证码后进入聊天界面”的完整过程。")

    add_heading(doc, "二、功能需求拆解", 1)
    table = doc.add_table(rows=1, cols=3)
    table.alignment = WD_TABLE_ALIGNMENT.CENTER
    hdr = table.rows[0].cells
    set_cell_text(hdr[0], "需求", bold=True, fill="E8EEF5")
    set_cell_text(hdr[1], "实现方式", bold=True, fill="E8EEF5")
    set_cell_text(hdr[2], "对应控件/代码", bold=True, fill="E8EEF5")
    rows = [
        ("显示验证码", "程序生成 5 位随机字符并显示到标签中", "captchaCodeLabel"),
        ("刷新验证码", "点击刷新按钮后重新生成随机字符串", "refreshCaptchaButton -> refreshCaptcha()"),
        ("输入验证码", "用户在输入框中填写看到的验证码", "captchaEdit"),
        ("登录校验", "比较用户输入与当前验证码，错误则提示并刷新", "tryLogin()"),
        ("进入聊天界面", "账号、密码、验证码都通过后关闭登录框", "accept()"),
    ]
    for need, impl, code in rows:
        cells = table.add_row().cells
        set_cell_text(cells[0], need)
        set_cell_text(cells[1], impl)
        set_cell_text(cells[2], code)

    add_heading(doc, "三、整体实现思路", 1)
    add_body(doc, "验证码功能可以按“生成、显示、输入、比较、反馈”五步理解：")
    for text in [
        "生成：准备一个字符池，例如大写字母和数字。",
        "随机：使用 QRandomGenerator 从字符池中随机取字符。",
        "显示：把生成结果保存到成员变量 captchaCode，并显示到 QLabel。",
        "输入：用户在 QLineEdit 中输入验证码。",
        "比较：点击登录时，将用户输入和 captchaCode 做比较；通过后进入聊天界面，失败则弹出提示并刷新验证码。",
    ]:
        add_number(doc, text)

    flow = (
        "打开程序\n"
        "  ↓\n"
        "显示登录窗口\n"
        "  ↓\n"
        "生成并显示随机验证码\n"
        "  ↓\n"
        "用户输入账号、密码、验证码\n"
        "  ↓\n"
        "点击登录进行校验\n"
        "  ↓\n"
        "校验成功：进入聊天界面；校验失败：提示错误并刷新验证码"
    )
    add_code_block(doc, flow)

    add_heading(doc, "四、界面设计说明", 1)
    add_body(
        doc,
        "登录界面使用 Qt Designer 的 .ui 文件进行设计，文件名为 logindialog.ui。这样做的好处是界面布局可以在 Forms 中直接看到，控件名称也能与 C++ 逻辑代码清晰对应。"
    )
    add_bullet(doc, "usernameEdit：账号输入框。")
    add_bullet(doc, "passwordEdit：密码输入框，设置为 Password 模式。")
    add_bullet(doc, "captchaEdit：验证码输入框。")
    add_bullet(doc, "captchaCodeLabel：显示当前随机验证码。")
    add_bullet(doc, "refreshCaptchaButton：刷新验证码按钮。")
    add_bullet(doc, "loginButton / cancelButton：登录和取消按钮。")

    add_heading(doc, "五、核心代码讲解", 1)
    add_heading(doc, "1. 生成随机验证码", 2)
    add_body(
        doc,
        "验证码生成函数的核心是字符池和随机下标。程序先准备一组不容易混淆的字符，再循环 5 次，每次随机取一个字符拼接成验证码。"
    )
    add_code_block(
        doc,
        """QString LoginDialog::createCaptcha() const
{
    static const QString chars = QStringLiteral("ABCDEFGHJKLMNPQRSTUVWXYZ23456789");
    QString code;
    code.reserve(5);

    for (int i = 0; i < 5; ++i) {
        const int index = QRandomGenerator::global()->bounded(chars.size());
        code.append(chars.at(index));
    }

    return code;
}"""
    )
    add_body(
        doc,
        "这里没有使用容易看错的 I、O、0、1，是为了减少用户输入时的识别困难。QRandomGenerator::global()->bounded(chars.size()) 会生成一个合法范围内的随机下标。"
    )

    add_heading(doc, "2. 刷新并显示验证码", 2)
    add_code_block(
        doc,
        """void LoginDialog::refreshCaptcha()
{
    captchaCode = createCaptcha();
    ui->captchaCodeLabel->setText(captchaCode);
    ui->captchaEdit->clear();
}"""
    )
    add_body(
        doc,
        "refreshCaptcha() 做了三件事：生成新的验证码、把验证码显示到界面标签、清空用户之前输入的验证码。这样每次刷新后，用户必须重新输入当前显示的验证码。"
    )

    add_heading(doc, "3. 登录时校验验证码", 2)
    add_code_block(
        doc,
        """if (inputCaptcha.compare(captchaCode, Qt::CaseInsensitive) != 0) {
    QMessageBox::warning(this, QStringLiteral("登录失败"),
                         QStringLiteral("验证码错误，请重新输入。"));
    refreshCaptcha();
    ui->captchaEdit->setFocus();
    return;
}"""
    )
    add_body(
        doc,
        "这里使用 Qt::CaseInsensitive 表示不区分大小写，例如验证码显示为 5U8N8，用户输入 5u8n8 也可以通过。验证码错误时，程序会弹出提示、刷新验证码，并把输入焦点重新放回验证码输入框。"
    )

    add_heading(doc, "六、信号与槽连接", 1)
    add_body(doc, "按钮和输入框的行为通过 Qt 的信号与槽机制连接：")
    add_code_block(
        doc,
        """connect(ui->refreshCaptchaButton, &QPushButton::clicked,
        this, &LoginDialog::refreshCaptcha);
connect(ui->loginButton, &QPushButton::clicked,
        this, &LoginDialog::tryLogin);
connect(ui->captchaEdit, &QLineEdit::returnPressed,
        this, &LoginDialog::tryLogin);"""
    )
    add_bullet(doc, "点击“刷新”按钮时，调用 refreshCaptcha()。")
    add_bullet(doc, "点击“登录”按钮时，调用 tryLogin()。")
    add_bullet(doc, "在验证码输入框按回车，也可以直接触发登录校验。")

    add_heading(doc, "七、课堂展示步骤", 1)
    for text in [
        "启动程序，先展示登录界面，说明此时还不能进入聊天界面。",
        "介绍验证码显示区域和刷新按钮，点击刷新，观察验证码变化。",
        "输入错误验证码，点击登录，展示错误提示和验证码刷新效果。",
        "输入正确账号、密码和验证码，点击登录，展示进入聊天主界面。",
        "打开 logindialog.ui，说明界面控件是在 Designer 中设计的。",
        "打开 logindialog.cpp，讲解 createCaptcha()、refreshCaptcha() 和 tryLogin() 三个函数。",
    ]:
        add_number(doc, text)

    add_heading(doc, "八、可以扩展的方向", 1)
    add_bullet(doc, "把验证码从纯文本改成带干扰线、旋转字符的图片验证码。")
    add_bullet(doc, "限制验证码错误次数，例如连续输错 3 次后暂停登录。")
    add_bullet(doc, "把账号密码改为数据库校验，而不是固定测试账号。")
    add_bullet(doc, "增加验证码有效时间，例如 60 秒后自动失效。")
    add_bullet(doc, "配合网络登录接口，把验证码作为登录请求的一部分发送给服务器。")

    add_heading(doc, "九、分享总结", 1)
    add_body(
        doc,
        "随机验证码功能虽然代码量不大，但它完整体现了一个小功能从界面设计到业务逻辑的实现过程：先在 .ui 文件中设计登录界面，再通过 C++ 生成随机验证码，最后在登录按钮的槽函数中完成输入校验。这个功能适合课堂展示，因为它既能现场运行，也能结合代码清楚说明实现步骤。"
    )
    add_body(
        doc,
        "本项目当前演示账号为 student，密码为 123456。实际项目中可以进一步接入数据库或服务器接口，让登录验证更加真实。"
    )

    doc.save(OUT_PATH)


if __name__ == "__main__":
    build()
