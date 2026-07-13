from __future__ import annotations

import html
import re
from collections import Counter
from datetime import datetime
from pathlib import Path

from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_LEFT
from reportlab.lib.pagesizes import A4, landscape
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (
    HRFlowable,
    LongTable,
    PageBreak,
    Paragraph,
    SimpleDocTemplate,
    Spacer,
)


ROOT = Path(__file__).resolve().parents[1]
OUTPUT_PDF = ROOT / "doc" / "gomoku_code_guide_tablet.pdf"

SOURCE_FILES = [
    ROOT / "CMakeLists.txt",
    ROOT / "res.qrc",
    ROOT / "src" / "app" / "main.cpp",
    ROOT / "src" / "common" / "config.h",
    ROOT / "src" / "common" / "gomoku_types.h",
    ROOT / "src" / "core" / "game_rule.h",
    ROOT / "src" / "core" / "game_rule.cpp",
    ROOT / "src" / "core" / "game_controller.h",
    ROOT / "src" / "core" / "game_controller.cpp",
    ROOT / "src" / "ui" / "chessboard_widget.h",
    ROOT / "src" / "ui" / "chessboard_widget.cpp",
    ROOT / "src" / "ui" / "mainwindow.h",
    ROOT / "src" / "ui" / "mainwindow.cpp",
    ROOT / "src" / "ui" / "mainwindow.ui",
    ROOT / "src" / "ai" / "board_evaluator.h",
    ROOT / "src" / "ai" / "ai_engine.h",
    ROOT / "src" / "network" / "protocol.h",
    ROOT / "src" / "network" / "network_manager.h",
    ROOT / "src" / "network" / "game_server.h",
    ROOT / "src" / "network" / "game_room.h",
    ROOT / "src" / "record" / "record_manager.h",
    ROOT / "src" / "data" / "models.h",
    ROOT / "src" / "data" / "database_manager.h",
]

FILE_SUMMARY = {
    "CMakeLists.txt": "工程构建入口，决定 Qt 模块、源文件、资源和安装方式。",
    "res.qrc": "Qt 资源清单，把棋盘底图和棋子图片打包进程序。",
    "main.cpp": "程序启动入口，创建应用对象并显示主窗口。",
    "config.h": "全局常量配置，统一窗口、棋盘和默认参数。",
    "gomoku_types.h": "跨模块共享的枚举与数据结构定义。",
    "game_rule.h": "棋盘规则判断接口，负责边界、合法性、五连和胜负。",
    "game_rule.cpp": "棋盘规则的具体实现。",
    "game_controller.h": "对局控制器接口，统一管理棋盘、回合、历史和结束状态。",
    "game_controller.cpp": "对局控制器的具体实现。",
    "chessboard_widget.h": "棋盘自绘控件接口，只负责显示和输入转发。",
    "chessboard_widget.cpp": "棋盘绘制、坐标换算、鼠标交互和资源贴图。",
    "mainwindow.h": "主窗口接口，负责页面切换和界面联动。",
    "mainwindow.cpp": "主窗口实现，连接 UI 与控制器。",
    "mainwindow.ui": "Qt Designer 设计文件，定义首页和对局页布局。",
    "board_evaluator.h": "局面评分器接口，为 AI 提供评估能力。",
    "ai_engine.h": "AI 搜索引擎接口，负责选点与难度配置。",
    "protocol.h": "网络消息协议定义。",
    "network_manager.h": "客户端网络管理接口。",
    "game_server.h": "本地服务器接口，用于对战房间监听。",
    "game_room.h": "房间状态容器，保存局面与对局元数据。",
    "record_manager.h": "对局记录管理接口。",
    "models.h": "数据库表对应的数据结构。",
    "database_manager.h": "SQLite 数据库连接与 schema 管理接口。",
}


def register_fonts() -> None:
    font_candidates = [
        Path(r"C:\Windows\Fonts\simhei.ttf"),
        Path(r"C:\Windows\Fonts\Noto Sans SC (TrueType).otf"),
        Path(r"C:\Windows\Fonts\NotoSansSC-VF.ttf"),
    ]
    for font_path in font_candidates:
        if font_path.exists():
            pdfmetrics.registerFont(TTFont("GomokuFont", str(font_path)))
            return
    raise FileNotFoundError("No usable CJK font found in C:\\Windows\\Fonts.")


def read_text(path: Path) -> str:
    encodings = ["utf-8-sig", "utf-8", "gb18030", "cp936"]
    for encoding in encodings:
        try:
            return path.read_text(encoding=encoding)
        except UnicodeDecodeError:
            continue
    return path.read_text(encoding="utf-8", errors="replace")


def escape_code_line(text: str) -> str:
    text = text.expandtabs(4)
    leading_spaces = len(text) - len(text.lstrip(" "))
    return "&nbsp;" * leading_spaces + html.escape(text.lstrip(" "))


def summarize_comment(text: str) -> str:
    text = text.lstrip("/*").lstrip("/").strip()
    if not text:
        return "注释，用于补充说明代码意图。"
    return f"注释：{text}"


def explain_cmake(line: str) -> str:
    s = line.strip()
    if not s:
        return "空行，分隔构建步骤。"
    if s.startswith("#"):
        return summarize_comment(s)
    if s.startswith("cmake_minimum_required"):
        return "声明最低 CMake 版本，保证构建环境一致。"
    if s.startswith("project("):
        return "定义项目名称、版本和语言。"
    if s.startswith("set(CMAKE_AUTOUIC"):
        return "开启自动处理 .ui 文件。"
    if s.startswith("set(CMAKE_AUTOMOC"):
        return "开启 Qt 元对象代码自动生成。"
    if s.startswith("set(CMAKE_AUTORCC"):
        return "开启 Qt 资源文件自动打包。"
    if s.startswith("set(CMAKE_CXX_STANDARD"):
        return "指定使用 C++17。"
    if s.startswith("find_package(QT"):
        return "查找 Qt Widgets、Network 和 Sql 模块。"
    if s.startswith("find_package(Qt"):
        return "根据 Qt 主版本继续查找所需组件。"
    if s.startswith("set(PROJECT_SOURCES"):
        return "开始声明项目源码清单。"
    if re.search(r"\.(cpp|cxx|cc|h|hpp|ui)$", s) or s.endswith("res.qrc"):
        return "把该文件纳入工程编译。"
    if s == ")":
        return "结束当前列表或函数调用。"
    if "qt_add_executable" in s:
        return "Qt 6 下创建可执行程序目标。"
    if s.startswith("add_library("):
        return "Android 平台下以库形式构建。"
    if s.startswith("add_executable("):
        return "Qt 5 或非 Android 平台下创建可执行程序。"
    if s.startswith("target_link_libraries("):
        return "把 Qt 模块链接到目标程序。"
    if s.startswith("target_include_directories("):
        return "把工程根目录加入头文件搜索路径。"
    if s.startswith("set_target_properties("):
        return "设置窗口程序、版本和平台相关属性。"
    if s.startswith("include(GNUInstallDirs)"):
        return "引入标准安装目录变量。"
    if s.startswith("install("):
        return "定义安装规则。"
    if s.startswith("qt_finalize_executable("):
        return "Qt 6 下完成可执行程序收尾配置。"
    return "构建脚本语句。"


def explain_qrc(line: str) -> str:
    s = line.strip()
    if not s:
        return "空行，分隔资源定义。"
    if s.startswith("<RCC"):
        return "Qt 资源系统根节点。"
    if s.startswith("<qresource"):
        return "定义资源路径前缀。"
    if s.startswith("<file"):
        return "把本地文件打包到 Qt 资源系统中。"
    if s.startswith("</qresource") or s.startswith("</RCC"):
        return "关闭资源定义节点。"
    return "资源描述行。"


def explain_ui(line: str) -> str:
    s = line.strip()
    if not s:
        return "空行，用来拉开 UI 结构层级。"
    name = re.search(r'name="([^"]+)"', s)
    widget = re.search(r'class="([^"]+)"', s)
    if s.startswith("<?xml"):
        return "XML 文件头。"
    if s.startswith("<ui"):
        return "Qt Designer 的界面描述文件根节点。"
    if s.startswith("<class>"):
        return "声明 UI 类名。"
    if s.startswith("<resources/>"):
        return "预留资源引用区域。"
    if s.startswith("<connections/>"):
        return "预留信号槽连接区域。"
    if s.startswith("<widget"):
        if name:
            special = {
                "MainWindow": "主窗口容器",
                "centralwidget": "主窗口中央容器",
                "stackedWidget": "页面切换容器",
                "homePage": "首页页面",
                "gamePage": "对局页面",
                "boardPlaceholder": "棋盘占位容器",
                "infoFrame": "信息侧栏",
                "modeLabel": "当前模式显示",
                "playerLabel": "当前玩家显示",
                "stepLabel": "步数显示",
                "statusLabel": "状态显示",
                "startGameButton": "开始对局按钮",
                "historyButton": "历史棋局按钮",
                "settingsButton": "系统设置按钮",
                "exitButton": "退出按钮",
                "backButton": "返回首页按钮",
            }.get(name.group(1), name.group(1))
            if widget:
                return f"定义 {widget.group(1)}，名称是 {special}。"
        return "定义界面控件。"
    if s.startswith("<layout"):
        return "定义布局容器，用于排列界面元素。"
    if s.startswith("<property"):
        return "设置控件属性。"
    if s.startswith("<item"):
        return "布局中的一个子项。"
    if s.startswith("<spacer"):
        return "留出弹性空白，让布局更稳定。"
    if s.startswith("<enum>"):
        return "枚举属性值。"
    if s.startswith("<number>"):
        return "数值型属性值。"
    if s.startswith("<string"):
        return "文本型属性值。"
    if s.startswith("</"):
        return "关闭当前 XML 结构。"
    return "UI 描述行。"


def explain_cpp(path: Path, line: str, idx: int) -> str:
    s = line.strip()
    if not s:
        return "空行，用来分隔逻辑段。"
    if s.startswith("#include"):
        include = re.findall(r'"([^"]+)"|<([^>]+)>', s)
        target = next((a or b for a, b in include), "")
        return f"引入 {target}，供当前文件使用。"
    if s.startswith("#pragma once"):
        return "防止头文件被重复包含。"
    if s.startswith("#ifndef") or s.startswith("#define") or s.startswith("#endif"):
        return "头文件保护宏。"
    if s.startswith("namespace ") and s.endswith("{"):
        return "开启命名空间，避免符号冲突。"
    if s == "} // namespace":
        return "关闭命名空间。"
    if s == "Q_OBJECT":
        return "Qt 元对象宏，启用信号槽和反射能力。"
    if s in {"public:", "private:", "signals:", "private slots:"}:
        return "声明访问区域。"
    if s.startswith("class ") or s.startswith("struct "):
        return "声明类或结构体。"
    if s.startswith("explicit ") and "(" in s:
        return "声明构造函数，避免隐式类型转换。"
    if s == "{":
        return "函数体或代码块开始。"
    if s == "}":
        return "函数体或代码块结束。"
    if re.match(r"^[A-Za-z_].*\)\s*$", s) and "::" not in s:
        return "声明一个函数原型或局部辅助函数。"
    if "::" in s and s.endswith(")"):
        return "定义类成员函数。"
    if "emit " in s:
        return "发出 Qt 信号，通知界面或上层逻辑。"
    if s.startswith("return "):
        return "返回当前计算结果。"
    if s.startswith("if ") or s.startswith("if("):
        return "条件判断，控制后续流程。"
    if s.startswith("for ") or s.startswith("for("):
        return "循环遍历数据或棋盘。"
    if s.startswith("while "):
        return "循环处理直到条件不成立。"
    if s.startswith("switch "):
        return "分支选择，不同情况走不同逻辑。"
    if "connect(" in s:
        return "把信号和槽连接起来。"
    if "setWindowTitle" in s:
        return "设置窗口标题。"
    if "resize(" in s:
        return "设置窗口初始尺寸。"
    if "setMinimumSize" in s:
        return "设置窗口最小尺寸。"
    if "QMessageBox" in s:
        return "弹出提示框，给用户明确反馈。"
    if "boardWidget_" in s and "new ChessBoardWidget" in s:
        return "创建自定义棋盘控件并挂到占位容器里。"
    if "controller_->resetGame" in s:
        return "重置对局状态，清空棋盘和历史。"
    if "setCurrentIndex" in s:
        return "切换页面。"
    if "placeStone" in s:
        return "尝试在棋盘上落子。"
    if "setBoard" in s:
        return "把控制器里的棋盘状态同步给界面。"
    if "setLastMove" in s:
        return "高亮最后一步。"
    if "clearMarkers" in s:
        return "清空标记。"
    if "showGameOverPrompt" in s:
        return "游戏结束后弹出结果提示和操作选择。"
    if "QStringLiteral" in s:
        return "静态字符串，便于 Qt 处理和国际化。"
    if "PieceColor::Black" in s:
        return "这里在处理黑子相关逻辑。"
    if "PieceColor::White" in s:
        return "这里在处理白子相关逻辑。"
    if "QVector" in s and "=" in s:
        return "初始化或更新容器数据。"
    if "board_.resize" in s:
        return "调整棋盘二维数组大小。"
    if "moveHistory_" in s and "push_back" in s:
        return "把一步棋记录进历史。"
    if "board_" in s and "[" in s and "=" in s:
        return "修改棋盘某个位置的棋子状态。"
    if "Q_UNUSED" in s:
        return "显式标记未使用参数，避免警告。"
    return "实现该模块的业务逻辑。"


def explain_line(path: Path, line: str, idx: int) -> str:
    suffix = path.suffix.lower()
    if path.name == "CMakeLists.txt":
        return explain_cmake(line)
    if suffix == ".qrc":
        return explain_qrc(line)
    if suffix == ".ui":
        return explain_ui(line)
    return explain_cpp(path, line, idx)


def file_role(path: Path) -> str:
    return FILE_SUMMARY.get(path.name, "项目源码文件。")


def make_styles():
    styles = getSampleStyleSheet()
    styles.add(
        ParagraphStyle(
            name="GTitle",
            fontName="GomokuFont",
            fontSize=20,
            leading=24,
            alignment=TA_CENTER,
            spaceAfter=8,
        )
    )
    styles.add(
        ParagraphStyle(
            name="GSubtitle",
            fontName="GomokuFont",
            fontSize=10,
            leading=14,
            alignment=TA_CENTER,
            textColor=colors.HexColor("#4d4d4d"),
        )
    )
    styles.add(
        ParagraphStyle(
            name="GSection",
            fontName="GomokuFont",
            fontSize=15,
            leading=18,
            spaceBefore=8,
            spaceAfter=6,
            textColor=colors.HexColor("#183a2c"),
        )
    )
    styles.add(
        ParagraphStyle(
            name="GBody",
            fontName="GomokuFont",
            fontSize=9,
            leading=12,
            alignment=TA_LEFT,
        )
    )
    styles.add(
        ParagraphStyle(
            name="GSmall",
            fontName="GomokuFont",
            fontSize=8,
            leading=10,
            alignment=TA_LEFT,
        )
    )
    styles.add(
        ParagraphStyle(
            name="GTable",
            fontName="GomokuFont",
            fontSize=7.6,
            leading=9,
            alignment=TA_LEFT,
            wordWrap="CJK",
        )
    )
    return styles


def header_footer(canvas, doc):
    canvas.saveState()
    canvas.setStrokeColor(colors.HexColor("#c8c8c8"))
    canvas.setLineWidth(0.5)
    canvas.line(doc.leftMargin, doc.pagesize[1] - 20 * mm, doc.pagesize[0] - doc.rightMargin, doc.pagesize[1] - 20 * mm)
    canvas.line(doc.leftMargin, 14 * mm, doc.pagesize[0] - doc.rightMargin, 14 * mm)
    canvas.setFont("GomokuFont", 8)
    canvas.setFillColor(colors.HexColor("#555555"))
    canvas.drawString(doc.leftMargin, doc.pagesize[1] - 14 * mm, "Gomoku 源码逐行讲解")
    canvas.drawRightString(doc.pagesize[0] - doc.rightMargin, doc.pagesize[1] - 14 * mm, datetime.now().strftime("%Y-%m-%d %H:%M"))
    canvas.drawString(doc.leftMargin, 8 * mm, f"第 {canvas.getPageNumber()} 页")
    canvas.drawRightString(doc.pagesize[0] - doc.rightMargin, 8 * mm, "适合平板批注阅读")
    canvas.restoreState()


def build_story(styles):
    story = []
    story.append(Spacer(1, 8 * mm))
    story.append(Paragraph("Gomoku 源码逐行讲解", styles["GTitle"]))
    story.append(Paragraph("范围：工程源码与构建文件，不包含 build 目录与二进制资源。", styles["GSubtitle"]))
    story.append(Paragraph("阅读方式：左侧是原始代码行，右侧是对应解释，适合直接在平板上做笔记。", styles["GSubtitle"]))
    story.append(Spacer(1, 4 * mm))

    module_counter = Counter()
    for source in SOURCE_FILES:
        module_counter[source.parent.name] += 1
    overview = " · ".join(f"{module}: {count} 个文件" for module, count in sorted(module_counter.items()))
    story.append(Paragraph(f"文件分布：{overview}", styles["GBody"]))
    story.append(Spacer(1, 4 * mm))
    story.append(HRFlowable(width="100%", thickness=0.8, color=colors.HexColor("#9aa7a2")))
    story.append(Spacer(1, 5 * mm))

    for index, path in enumerate(SOURCE_FILES):
        text = read_text(path)
        lines = text.splitlines()
        story.append(Paragraph(f"{path.relative_to(ROOT).as_posix()}", styles["GSection"]))
        story.append(Paragraph(file_role(path), styles["GBody"]))
        story.append(Spacer(1, 2 * mm))

        table_rows = [
            [
                Paragraph("<b>行号</b>", styles["GTable"]),
                Paragraph("<b>代码</b>", styles["GTable"]),
                Paragraph("<b>说明</b>", styles["GTable"]),
            ]
        ]

        for line_no, line in enumerate(lines, 1):
            table_rows.append(
                [
                    Paragraph(str(line_no), styles["GTable"]),
                    Paragraph(escape_code_line(line) or "&nbsp;", styles["GTable"]),
                    Paragraph(html.escape(explain_line(path, line, line_no)), styles["GTable"]),
                ]
            )

        table = LongTable(
            table_rows,
            colWidths=[14 * mm, 140 * mm, 123 * mm],
            repeatRows=1,
        )
        table.setStyle(
            [
                ("FONTNAME", (0, 0), (-1, -1), "GomokuFont"),
                ("FONTSIZE", (0, 0), (-1, -1), 7.4),
                ("LEADING", (0, 0), (-1, -1), 9),
                ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#dbe8e2")),
                ("TEXTCOLOR", (0, 0), (-1, 0), colors.HexColor("#183a2c")),
                ("GRID", (0, 0), (-1, -1), 0.25, colors.HexColor("#b9c7c1")),
                ("VALIGN", (0, 0), (-1, -1), "TOP"),
                ("LEFTPADDING", (0, 0), (-1, -1), 4),
                ("RIGHTPADDING", (0, 0), (-1, -1), 4),
                ("TOPPADDING", (0, 0), (-1, -1), 3),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 3),
            ]
        )

        for row_idx in range(1, len(table_rows)):
            if row_idx % 2 == 0:
                table.setStyle([("BACKGROUND", (0, row_idx), (-1, row_idx), colors.HexColor("#f8fbfa"))])

        story.append(table)
        if index != len(SOURCE_FILES) - 1:
            story.append(PageBreak())

    return story


def main():
    register_fonts()
    styles = make_styles()
    OUTPUT_PDF.parent.mkdir(parents=True, exist_ok=True)

    doc = SimpleDocTemplate(
        str(OUTPUT_PDF),
        pagesize=landscape(A4),
        leftMargin=10 * mm,
        rightMargin=10 * mm,
        topMargin=22 * mm,
        bottomMargin=16 * mm,
        title="Gomoku 源码逐行讲解",
        author="Codex",
    )
    story = build_story(styles)
    doc.build(story, onFirstPage=header_footer, onLaterPages=header_footer)
    print(str(OUTPUT_PDF))


if __name__ == "__main__":
    main()
