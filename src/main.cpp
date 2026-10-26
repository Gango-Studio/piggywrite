#include <algorithm>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "editor.hpp"
#include "terminal.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace {

using piggywrite::Codepoint;
using piggywrite::EditorApp;
using piggywrite::EditorMode;
using piggywrite::ExitReason;
using piggywrite::Terminal;
using piggywrite::TextLine;

std::u32string ascii(std::string_view text) {
    std::u32string out;
    out.reserve(text.size());
    for (char ch : text) {
        out.push_back(static_cast<unsigned char>(ch));
    }
    return out;
}

std::u32string fromUtf8Text(std::string_view text) {
    return piggywrite::fromUtf8(text);
}

void appendPosition(std::u32string& frame, int row, int column) {
    frame += U"\x1b[";
    frame += ascii(std::to_string(row));
    frame += U";";
    frame += ascii(std::to_string(column));
    frame += U"H";
}

std::u32string padOrClip(std::u32string_view text, int width, bool pad = true) {
    if (width <= 0) {
        return {};
    }

    std::u32string out;
    std::size_t cells = 0;
    for (Codepoint cp : text) {
        Codepoint shown = cp;
        std::size_t cellsForCharacter = 0;

        if (cp == U'\t') {
            cellsForCharacter = piggywrite::cellWidth(cp, cells);
            if (cells + cellsForCharacter > static_cast<std::size_t>(width)) {
                break;
            }
            out.append(cellsForCharacter, U' ');
            cells += cellsForCharacter;
            continue;
        }

        if (cp < 0x20U || cp == 0x7FU) {
            shown = U'?';
        }
        cellsForCharacter = piggywrite::cellWidth(shown, cells);
        if (cells + cellsForCharacter > static_cast<std::size_t>(width)) {
            break;
        }
        out.push_back(shown);
        cells += cellsForCharacter;
    }

    if (pad && cells < static_cast<std::size_t>(width)) {
        out.append(static_cast<std::size_t>(width) - cells, U' ');
    }
    return out;
}

std::u32string repeatCharacter(Codepoint cp, int count) {
    if (count <= 0) {
        return {};
    }
    return std::u32string(static_cast<std::size_t>(count), cp);
}

std::u32string renderTextLine(const TextLine& text,
                              std::size_t leftColumn,
                              int maxWidth) {
    if (maxWidth <= 0) {
        return {};
    }

    std::u32string out;
    std::size_t absColumn = 0;
    std::size_t index = 0;
    std::size_t used = 0;

    while (index < text.size()) {
        const Codepoint cp = text[index];
        const std::size_t width = piggywrite::cellWidth(cp, absColumn);
        if (absColumn + width <= leftColumn) {
            absColumn += width;
            ++index;
            continue;
        }
        if (absColumn < leftColumn) {
            const std::size_t visible = absColumn + width - leftColumn;
            out.append(visible, U' ');
            used += visible;
            absColumn += width;
            ++index;
        }
        break;
    }

    while (index < text.size() && used < static_cast<std::size_t>(maxWidth)) {
        const Codepoint cp = text[index];
        const std::size_t width = piggywrite::cellWidth(cp, absColumn);
        if (width > 0 &&
            used + width > static_cast<std::size_t>(maxWidth)) {
            break;
        }

        if (cp == U'\t') {
            out.append(width, U' ');
        } else if (cp < 0x20U || cp == 0x7FU) {
            out.push_back(U'?');
        } else {
            out.push_back(cp);
        }
        used += width;
        absColumn += width;
        ++index;
    }

    return out;
}

std::size_t visualPrefixWidth(const TextLine& text, std::size_t column) {
    std::size_t cells = 0;
    const std::size_t limit = std::min(column, text.size());
    for (std::size_t index = 0; index < limit; ++index) {
        cells += piggywrite::cellWidth(text[index], cells);
    }
    return cells;
}

class UiRenderer {
public:
    explicit UiRenderer(Terminal& terminal) : terminal_(terminal) {}

    void draw(const EditorApp& app, int columns, int rows) {
        const int width = std::max(1, columns);
        const int height = std::max(1, rows);
        std::u32string frame;
        frame.reserve(static_cast<std::size_t>(width) * 4U + 512U);
        frame += U"\x1b[?25l";

        if (width < 24 || height < 6) {
            appendPosition(frame, 1, 1);
            frame += padOrClip(U"piggywrite：终端窗口太小（至少 24 × 6）", width);
            frame += U"\x1b[0m\x1b[?25l";
            terminal_.write(piggywrite::toUtf8(frame));
            return;
        }

        const auto& document = app.document();
        const piggywrite::Cursor cursor = app.cursor();
        const bool editing = app.mode() == EditorMode::Edit;

        const std::size_t lineCount = document.lineCount();
        const int bodyRows = height - 4;

        if (cursor.row < topLine_) {
            topLine_ = cursor.row;
        } else if (cursor.row >= topLine_ + static_cast<std::size_t>(bodyRows)) {
            topLine_ = cursor.row - static_cast<std::size_t>(bodyRows) + 1;
        }
        const std::size_t maximumTop =
            lineCount > static_cast<std::size_t>(bodyRows)
                ? lineCount - static_cast<std::size_t>(bodyRows)
                : 0;
        topLine_ = std::min(topLine_, maximumTop);

        std::size_t numberDigits =
            std::to_string(std::max<std::size_t>(1, lineCount)).size();
        numberDigits = std::max<std::size_t>(2, numberDigits);
        const int gutterCells = static_cast<int>(numberDigits) + 3;
        const int textCells = width - gutterCells;

        const TextLine& cursorLine = document.line(cursor.row);
        const std::size_t cursorVisual =
            visualPrefixWidth(cursorLine, cursor.column);
        const std::size_t cursorLineWidth = piggywrite::visualWidth(cursorLine);
        if (cursorLineWidth <= static_cast<std::size_t>(textCells)) {
            leftColumn_ = 0;
        } else if (cursorVisual < leftColumn_) {
            leftColumn_ = cursorVisual;
        } else if (cursorVisual >=
                   leftColumn_ + static_cast<std::size_t>(textCells)) {
            leftColumn_ = cursorVisual - static_cast<std::size_t>(textCells) + 1;
        }
        const std::size_t maximumLeft =
            cursorLineWidth > static_cast<std::size_t>(textCells)
                ? cursorLineWidth - static_cast<std::size_t>(textCells)
                : 0;
        leftColumn_ = std::min(leftColumn_, maximumLeft);

        // Header.
        std::u32string title = U" piggywrite  ·  ";
        title += fromUtf8Text(app.path().u8string());
        if (document.dirty()) {
            title += U"  [未保存]";
        }
        appendPosition(frame, 1, 1);
        frame += U"\x1b[1;36m";
        frame += padOrClip(title, width);
        frame += U"\x1b[0m\x1b[K";

        // Separator.
        appendPosition(frame, 2, 1);
        frame += U"\x1b[90m";
        frame += repeatCharacter(U'─', width);
        frame += U"\x1b[0m";

        // Document area.
        for (int offset = 0; offset < bodyRows; ++offset) {
            const int screenRow = 3 + offset;
            const std::size_t row =
                topLine_ + static_cast<std::size_t>(offset);
            appendPosition(frame, screenRow, 1);

            if (row >= lineCount) {
                frame += U"\x1b[K";
                continue;
            }

            const bool current = row == cursor.row;
            std::u32string number = ascii(std::to_string(row + 1));
            if (number.size() < numberDigits) {
                number.insert(number.begin(),
                              numberDigits - number.size(), U' ');
            }

            frame += current ? U"\x1b[1;33m" : U"\x1b[90m";
            frame += number;
            frame += U" │ ";
            frame += U"\x1b[0m";
            frame += renderTextLine(document.line(row), leftColumn_, textCells);
            frame += U"\x1b[K";
        }

        // Status bar.
        std::u32string status = editing ? U" 编辑 " : U" 浏览 ";
        status += U"│ 行 ";
        status += ascii(std::to_string(cursor.row + 1));
        status += U", 列 ";
        status += ascii(std::to_string(cursor.column + 1));
        status += document.dirty() ? U"  │ ● 未保存" : U"  │ 就绪";

        if (app.hashPending()) {
            status += U"  │ 命令前缀 #：按 s 保存并退出，按 q 退出不保存";
        } else if (!app.message().empty()) {
            status += U"  │ ";
            status += fromUtf8Text(app.message());
        }

        status = padOrClip(status, width);
        appendPosition(frame, height - 1, 1);
        if (app.messageIsError()) {
            frame += U"\x1b[97;41m";
        } else if (app.hashPending()) {
            frame += U"\x1b[30;43m";
        } else {
            frame += U"\x1b[7m";
        }
        frame += status;
        frame += U"\x1b[0m\x1b[K";

        // Help line.
        std::u32string help;
        if (app.hashPending()) {
            help = U"s 保存并退出   ·   q 退出不保存   ·   其他键取消命令";
        } else if (editing) {
            help = U"Esc 返回浏览后可用 #s / #q  ·  Ctrl+S 保存退出  ·  "
                   U"方向键移动  ·  Enter 换行  ·  Backspace 删除";
        } else {
            help = U"t 进入编辑  ·  #s 保存并退出  ·  #q 退出不保存  ·  "
                   U"方向键查看  ·  Esc 退出";
        }
        appendPosition(frame, height, 1);
        frame += U"\x1b[90m";
        frame += padOrClip(help, width);
        frame += U"\x1b[0m\x1b[K";

        if (editing) {
            const std::size_t screenRow = 3 + (cursor.row - topLine_);
            const std::size_t screenColumn =
                static_cast<std::size_t>(gutterCells) + 1 +
                (cursorVisual > leftColumn_ ? cursorVisual - leftColumn_ : 0);
            const int cursorRow = static_cast<int>(std::min<std::size_t>(
                screenRow, static_cast<std::size_t>(height)));
            const int cursorColumn = static_cast<int>(std::min<std::size_t>(
                screenColumn, static_cast<std::size_t>(width)));
            appendPosition(frame, cursorRow, cursorColumn);
            frame += U"\x1b[?25h";
        } else {
            frame += U"\x1b[?25l";
        }
        frame += U"\x1b[0m";

        terminal_.write(piggywrite::toUtf8(frame));
    }

private:
    Terminal& terminal_;
    std::size_t topLine_ = 0;
    std::size_t leftColumn_ = 0;
};

std::vector<std::filesystem::path> commandLineArguments(int argc, char** argv) {
    std::vector<std::filesystem::path> result;

#ifdef _WIN32
    (void)argc;
    (void)argv;
    int wideCount = 0;
    LPWSTR* wideArguments = CommandLineToArgvW(GetCommandLineW(), &wideCount);
    if (wideArguments != nullptr) {
        for (int index = 1; index < wideCount; ++index) {
            result.emplace_back(wideArguments[index]);
        }
        LocalFree(wideArguments);
        return result;
    }
#endif

    for (int index = 1; index < argc; ++index) {
        result.emplace_back(argv[index]);
    }
    return result;
}

void printHelp() {
    std::cout
        << "piggywrite 1.0 \n"
        << "用法: piggywrite [文件]\n\n"
        << "按键:\n"
        << "  t           从浏览模式进入编辑模式\n"
        << "  Esc         编辑模式：返回浏览模式\n"
        << "              浏览模式：退出（有未保存修改时需再按一次）\n"
        << "  #s          在浏览模式输入：保存并退出\n"
        << "  #q          在浏览模式输入：退出，不保存\n"
        << "  Enter       换行\n"
        << "  Backspace   删除光标前一个字符\n"
        << "  Delete      删除光标处字符\n"
        << "  方向键      移动光标\n"
        << "  Home / End  行首 / 行尾\n"
        << "  Ctrl+S      保存并退出（同 #s）\n"
        << "  Ctrl+Q      退出不保存（同 #q）\n\n"
        << "提示: 不带文件名时保存到当前目录的 untitled.txt。\n"
        << "      文件按 UTF-8 读写，保留原有的 BOM 和换行风格。\n\n"
        << "By Gango Studios & Sam Mu\n";
}

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    bool showHelp = false;
    std::filesystem::path path;
    bool pathGiven = false;

    for (const auto& argument : commandLineArguments(argc, argv)) {
        const std::string text = argument.u8string();
        if (text == "--help" || text == "-h" || text == "/?") {
            showHelp = true;
            continue;
        }
        if (pathGiven) {
            std::cerr << "错误：只能指定一个文件。\n";
            printHelp();
            return 1;
        }
        path = argument;
        pathGiven = true;
    }

    if (showHelp) {
        printHelp();
        return 0;
    }

    if (!pathGiven) {
        path = "untitled.txt";
    }

    std::error_code absoluteError;
    const std::filesystem::path absolutePath =
        std::filesystem::absolute(path, absoluteError);
    if (!absoluteError) {
        path = absolutePath;
    }

    EditorApp app(path);
    std::string error;
    if (!app.loadIfExists(error)) {
        std::cerr << "无法打开 " << path.u8string() << "：" << error << "\n";
        return 1;
    }

    Terminal terminal;
    if (!terminal.init(error)) {
        std::cerr << "无法启动编辑器：" << error << "\n";
        return 1;
    }

    try {
        UiRenderer renderer(terminal);
        bool running = true;
        while (running) {
            renderer.draw(app, terminal.columns(), terminal.rows());
            const piggywrite::Key key = terminal.readKey();
            running = !app.handleKey(key);
        }
    } catch (const std::exception& exception) {
        terminal.shutdown();
        std::cerr << "编辑器异常退出：" << exception.what() << "\n";
        return 1;
    }

    const ExitReason reason = app.exitReason();
    const bool wasDirty = app.document().dirty();
    terminal.shutdown();

    if (reason == ExitReason::Saved) {
        std::cout << "已保存: " << app.path().u8string() << "\n";
    } else if (reason == ExitReason::Discarded && wasDirty) {
        std::cout << "已退出，未保存修改。\n";
    }

    return 0;
}
