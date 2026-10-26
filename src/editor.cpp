#include "editor.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <system_error>
#include <utility>

namespace piggywrite {
namespace {

constexpr Codepoint kReplacementCharacter = U'\uFFFD';

bool isContinuation(unsigned char byte) noexcept {
    return (byte & 0xC0U) == 0x80U;
}

bool decodeOne(std::string_view text, std::size_t& index, Codepoint& cp) noexcept {
    const auto first = static_cast<unsigned char>(text[index]);
    if (first < 0x80U) {
        cp = static_cast<Codepoint>(first);
        ++index;
        return true;
    }

    std::size_t length = 0;
    Codepoint value = 0;
    Codepoint minimum = 0;
    if (first >= 0xC2U && first <= 0xDFU) {
        length = 2;
        value = first & 0x1FU;
        minimum = 0x80U;
    } else if (first >= 0xE0U && first <= 0xEFU) {
        length = 3;
        value = first & 0x0FU;
        minimum = 0x800U;
    } else if (first >= 0xF0U && first <= 0xF4U) {
        length = 4;
        value = first & 0x07U;
        minimum = 0x10000U;
    } else {
        ++index;
        cp = kReplacementCharacter;
        return false;
    }

    if (index + length > text.size()) {
        ++index;
        cp = kReplacementCharacter;
        return false;
    }

    for (std::size_t offset = 1; offset < length; ++offset) {
        const auto byte = static_cast<unsigned char>(text[index + offset]);
        if (!isContinuation(byte)) {
            ++index;
            cp = kReplacementCharacter;
            return false;
        }
        value = (value << 6U) | (byte & 0x3FU);
    }

    const bool overlong = value < minimum;
    const bool surrogate = value >= 0xD800U && value <= 0xDFFFU;
    const bool tooLarge = value > 0x10FFFFU;
    if (overlong || surrogate || tooLarge) {
        ++index;
        cp = kReplacementCharacter;
        return false;
    }

    index += length;
    cp = value;
    return true;
}

bool isCombining(Codepoint cp) noexcept {
    static constexpr Codepoint ranges[][2] = {
        {0x0300, 0x036F}, {0x0483, 0x0489}, {0x0591, 0x05BD},
        {0x05BF, 0x05BF}, {0x05C1, 0x05C2}, {0x05C4, 0x05C5},
        {0x05C7, 0x05C7}, {0x0610, 0x061A}, {0x064B, 0x065F},
        {0x0670, 0x0670}, {0x06D6, 0x06DC}, {0x06DF, 0x06E4},
        {0x06E7, 0x06E8}, {0x06EA, 0x06ED}, {0x0711, 0x0711},
        {0x0730, 0x074A}, {0x07A6, 0x07B0}, {0x07EB, 0x07F3},
        {0x0816, 0x0819}, {0x081B, 0x0823}, {0x0825, 0x0827},
        {0x0829, 0x082D}, {0x0859, 0x085B}, {0x08D3, 0x0903},
        {0x093A, 0x093A}, {0x093C, 0x093C}, {0x0941, 0x0948},
        {0x094D, 0x094D}, {0x0951, 0x0957}, {0x0962, 0x0963},
        {0x0981, 0x0981}, {0x09BC, 0x09BC}, {0x09C1, 0x09C4},
        {0x09CD, 0x09CD}, {0x09E2, 0x09E3}, {0x0A01, 0x0A02},
        {0x0A3C, 0x0A3C}, {0x0A41, 0x0A42}, {0x0A47, 0x0A48},
        {0x0A4B, 0x0A4D}, {0x0A51, 0x0A51}, {0x0A70, 0x0A71},
        {0x0A75, 0x0A75}, {0x0A81, 0x0A82}, {0x0ABC, 0x0ABC},
        {0x0AC1, 0x0AC5}, {0x0AC7, 0x0AC8}, {0x0ACD, 0x0ACD},
        {0x0AE2, 0x0AE3}, {0x0B01, 0x0B01}, {0x0B3C, 0x0B3C},
        {0x0B3F, 0x0B3F}, {0x0B41, 0x0B44}, {0x0B4D, 0x0B4D},
        {0x0B56, 0x0B56}, {0x0B62, 0x0B63}, {0x0B82, 0x0B82},
        {0x0BC0, 0x0BC0}, {0x0BCD, 0x0BCD}, {0x0C00, 0x0C00},
        {0x0C3E, 0x0C40}, {0x0C46, 0x0C48}, {0x0C4A, 0x0C4D},
        {0x0C55, 0x0C56}, {0x0C62, 0x0C63}, {0x0C81, 0x0C81},
        {0x0CBC, 0x0CBC}, {0x0CBF, 0x0CBF}, {0x0CC6, 0x0CC6},
        {0x0CCC, 0x0CCD}, {0x0CE2, 0x0CE3}, {0x0D00, 0x0D01},
        {0x0D3B, 0x0D3C}, {0x0D41, 0x0D44}, {0x0D4D, 0x0D4D},
        {0x0D62, 0x0D63}, {0x0DCA, 0x0DCA}, {0x0DD2, 0x0DD4},
        {0x0DD6, 0x0DD6}, {0x0E31, 0x0E31}, {0x0E34, 0x0E3A},
        {0x0E47, 0x0E4E}, {0x0EB1, 0x0EB1}, {0x0EB4, 0x0EBC},
        {0x0EC8, 0x0ECD}, {0x0F18, 0x0F19}, {0x0F35, 0x0F35},
        {0x0F37, 0x0F37}, {0x0F39, 0x0F39}, {0x0F71, 0x0F7E},
        {0x0F80, 0x0F84}, {0x0F86, 0x0F87}, {0x0F8D, 0x0F97},
        {0x0F99, 0x0FBC}, {0x0FC6, 0x0FC6}, {0x102D, 0x1030},
        {0x1032, 0x1037}, {0x1039, 0x103A}, {0x1058, 0x1059},
        {0x1160, 0x11FF}, {0x135D, 0x135F}, {0x1712, 0x1714},
        {0x1732, 0x1734}, {0x1752, 0x1753}, {0x1772, 0x1773},
        {0x17B4, 0x17B5}, {0x17B7, 0x17BD}, {0x17C6, 0x17C6},
        {0x17C9, 0x17D3}, {0x17DD, 0x17DD}, {0x180B, 0x180D},
        {0x18A9, 0x18A9}, {0x1920, 0x1922}, {0x1927, 0x1928},
        {0x1932, 0x1932}, {0x1939, 0x193B}, {0x1A17, 0x1A18},
        {0x1A1B, 0x1A1B}, {0x1A56, 0x1A56}, {0x1A58, 0x1A5E},
        {0x1A60, 0x1A60}, {0x1A62, 0x1A62}, {0x1A65, 0x1A6C},
        {0x1A73, 0x1A7C}, {0x1A7F, 0x1A7F}, {0x1AB0, 0x1AFF},
        {0x1B00, 0x1B03}, {0x1B34, 0x1B34}, {0x1B36, 0x1B3A},
        {0x1B3C, 0x1B3C}, {0x1B42, 0x1B42}, {0x1B6B, 0x1B73},
        {0x1D165, 0x1D169}, {0x1D16D, 0x1D172}, {0x1D17B, 0x1D182},
        {0x1D185, 0x1D18B}, {0x1D1AA, 0x1D1AD}, {0x1D242, 0x1D244},
        {0x1DC0, 0x1DFF}, {0x20D0, 0x20FF}, {0x2CEF, 0x2CF1},
        {0x2DE0, 0x2DFF}, {0x302A, 0x302D}, {0x3099, 0x309A},
        {0xA66F, 0xA672}, {0xA674, 0xA67D}, {0xA69E, 0xA69F},
        {0xA6F0, 0xA6F1}, {0xA8E0, 0xA8F1}, {0xFE00, 0xFE0F},
        {0xFE20, 0xFE2F}, {0x1E8D0, 0x1E8D6}, {0x1E944, 0x1E94A},
        {0xE0100, 0xE01EF},
    };
    for (const auto& range : ranges) {
        if (cp >= range[0] && cp <= range[1]) {
            return true;
        }
    }
    return false;
}

bool isZeroWidth(Codepoint cp) noexcept {
    return (cp >= 0x200BU && cp <= 0x200FU) ||
           (cp >= 0x202AU && cp <= 0x202EU) ||
           (cp >= 0x2060U && cp <= 0x2064U) ||
           (cp >= 0x2066U && cp <= 0x206FU) ||
           cp == 0xFEFFU;
}

bool isWide(Codepoint cp) noexcept {
    return (cp >= 0x1100U && cp <= 0x115FU) ||
           (cp >= 0x231AU && cp <= 0x231BU) ||
           (cp >= 0x2329U && cp <= 0x232AU) ||
           (cp >= 0x2E80U && cp <= 0xA4CFU) ||
           (cp >= 0xA960U && cp <= 0xA97FU) ||
           (cp >= 0xAC00U && cp <= 0xD7A3U) ||
           (cp >= 0xF900U && cp <= 0xFAFFU) ||
           (cp >= 0xFE10U && cp <= 0xFE19U) ||
           (cp >= 0xFE30U && cp <= 0xFE6FU) ||
           (cp >= 0xFF00U && cp <= 0xFF60U) ||
           (cp >= 0xFFE0U && cp <= 0xFFE6U) ||
           (cp >= 0x1F004U && cp <= 0x1F004U) ||
           (cp >= 0x1F0CFU && cp <= 0x1F0CFU) ||
           (cp >= 0x1F18EU && cp <= 0x1F18EU) ||
           (cp >= 0x1F191U && cp <= 0x1F19AU) ||
           (cp >= 0x1F200U && cp <= 0x1F2FFU) ||
           (cp >= 0x1F300U && cp <= 0x1F64FU) ||
           (cp >= 0x1F680U && cp <= 0x1F6FFU) ||
           (cp >= 0x1F900U && cp <= 0x1F9FFU) ||
           (cp >= 0x1FA70U && cp <= 0x1FAFFU) ||
           (cp >= 0x20000U && cp <= 0x2FFFDU) ||
           (cp >= 0x30000U && cp <= 0x3FFFDU);
}

std::string errnoText() {
    return std::strerror(errno);
}

}  // namespace

std::u32string fromUtf8(std::string_view text, bool* valid, std::size_t* badOffset) {
    std::u32string out;
    out.reserve(text.size());

    bool allValid = true;
    std::size_t firstBad = 0;
    std::size_t index = 0;
    while (index < text.size()) {
        Codepoint cp = 0;
        const std::size_t begin = index;
        if (decodeOne(text, index, cp)) {
            out.push_back(cp);
        } else {
            if (allValid) {
                firstBad = begin;
            }
            allValid = false;
            out.push_back(kReplacementCharacter);
        }
    }

    if (valid != nullptr) {
        *valid = allValid;
    }
    if (badOffset != nullptr && !allValid) {
        *badOffset = firstBad;
    }
    return out;
}

std::string toUtf8(std::u32string_view text) {
    std::string out;
    out.reserve(text.size() * 2);
    for (Codepoint cp : text) {
        if (cp > 0x10FFFFU || (cp >= 0xD800U && cp <= 0xDFFFU)) {
            cp = kReplacementCharacter;
        }
        if (cp < 0x80U) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800U) {
            out.push_back(static_cast<char>(0xC0U | (cp >> 6U)));
            out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
        } else if (cp < 0x10000U) {
            out.push_back(static_cast<char>(0xE0U | (cp >> 12U)));
            out.push_back(static_cast<char>(0x80U | ((cp >> 6U) & 0x3FU)));
            out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
        } else {
            out.push_back(static_cast<char>(0xF0U | (cp >> 18U)));
            out.push_back(static_cast<char>(0x80U | ((cp >> 12U) & 0x3FU)));
            out.push_back(static_cast<char>(0x80U | ((cp >> 6U) & 0x3FU)));
            out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
        }
    }
    return out;
}

std::size_t cellWidth(Codepoint cp, std::size_t column) noexcept {
    if (cp == U'\t') {
        return 4U - (column % 4U);
    }
    if (cp < 0x20U || (cp >= 0x7FU && cp < 0xA0U)) {
        return 1U;
    }
    if (isZeroWidth(cp) || isCombining(cp)) {
        return 0U;
    }
    if (isWide(cp)) {
        return 2U;
    }
    return 1U;
}

std::size_t visualWidth(std::u32string_view text) noexcept {
    std::size_t column = 0;
    for (Codepoint cp : text) {
        column += cellWidth(cp, column);
    }
    return column;
}

bool Document::load(const std::filesystem::path& path, std::string& error) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = "无法打开文件：" + errnoText();
        return false;
    }

    std::string bytes((std::istreambuf_iterator<char>(input)),
                      std::istreambuf_iterator<char>());
    if (input.bad()) {
        error = "读取文件失败：" + errnoText();
        return false;
    }

    bool bom = false;
    if (bytes.size() >= 3 &&
        static_cast<unsigned char>(bytes[0]) == 0xEFU &&
        static_cast<unsigned char>(bytes[1]) == 0xBBU &&
        static_cast<unsigned char>(bytes[2]) == 0xBFU) {
        bom = true;
        bytes.erase(0, 3);
    }

    std::string newline = "\n";
    const std::size_t newlinePosition = bytes.find('\n');
    if (newlinePosition != std::string::npos && newlinePosition > 0 &&
        bytes[newlinePosition - 1] == '\r') {
        newline = "\r\n";
    }

    bool valid = true;
    std::size_t badOffset = 0;
    std::u32string decoded = fromUtf8(bytes, &valid, &badOffset);
    if (!valid) {
        error = "文件不是有效的 UTF-8（字节偏移 " +
                std::to_string(badOffset + (bom ? 3U : 0U)) + "）";
        return false;
    }

    std::vector<TextLine> loaded;
    TextLine current;
    for (Codepoint cp : decoded) {
        if (cp == U'\n') {
            if (!current.empty() && current.back() == U'\r') {
                current.pop_back();
            }
            loaded.push_back(std::move(current));
            current.clear();
        } else {
            current.push_back(cp);
        }
    }
    if (!current.empty() && current.back() == U'\r') {
        current.pop_back();
    }
    loaded.push_back(std::move(current));

    lines_ = std::move(loaded);
    eol_ = newline;
    hasBom_ = bom;
    dirty_ = false;
    return true;
}

bool Document::save(const std::filesystem::path& path, std::string& error) const {
    std::string bytes;
    if (hasBom_) {
        bytes.append("\xEF\xBB\xBF");
    }

    for (std::size_t row = 0; row < lines_.size(); ++row) {
        bytes += toUtf8(lines_[row]);
        if (row + 1 < lines_.size()) {
            bytes += eol_;
        }
    }

    errno = 0;
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        error = "无法写入文件：" + errnoText();
        return false;
    }

    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    output.flush();
    if (!output) {
        error = "写入文件失败：" + errnoText();
        return false;
    }
    output.close();
    if (!output) {
        error = "关闭文件失败：" + errnoText();
        return false;
    }
    return true;
}

const TextLine& Document::line(std::size_t row) const {
    static const TextLine empty;
    if (row >= lines_.size()) {
        return empty;
    }
    return lines_[row];
}

std::size_t Document::lineLength(std::size_t row) const {
    return line(row).size();
}

Cursor Document::clamp(Cursor position) const noexcept {
    if (lines_.empty()) {
        return {};
    }
    position.row = std::min(position.row, lines_.size() - 1);
    position.column = std::min(position.column, lines_[position.row].size());
    return position;
}

bool Document::isCursorValid(Cursor position) const noexcept {
    return position.row < lines_.size() &&
           position.column <= lines_[position.row].size();
}

Cursor Document::insert(Cursor position, Codepoint cp) {
    position = clamp(position);
    lines_[position.row].insert(lines_[position.row].begin() +
                                    static_cast<std::ptrdiff_t>(position.column),
                                cp);
    ++position.column;
    dirty_ = true;
    return position;
}

Cursor Document::insertText(Cursor position, std::u32string_view text) {
    for (Codepoint cp : text) {
        if (cp == U'\n') {
            position = insertNewline(position);
        } else {
            position = insert(position, cp);
        }
    }
    return position;
}

Cursor Document::insertNewline(Cursor position) {
    position = clamp(position);
    TextLine& current = lines_[position.row];
    TextLine remainder(current.begin() +
                           static_cast<std::ptrdiff_t>(position.column),
                       current.end());
    current.erase(current.begin() + static_cast<std::ptrdiff_t>(position.column),
                  current.end());
    lines_.insert(lines_.begin() + static_cast<std::ptrdiff_t>(position.row + 1),
                  std::move(remainder));
    ++position.row;
    position.column = 0;
    dirty_ = true;
    return position;
}

Cursor Document::backspace(Cursor position) {
    position = clamp(position);
    if (position.column > 0) {
        TextLine& current = lines_[position.row];
        current.erase(current.begin() +
                          static_cast<std::ptrdiff_t>(position.column - 1));
        --position.column;
        dirty_ = true;
        return position;
    }

    if (position.row == 0) {
        return position;
    }

    const std::size_t previousRow = position.row - 1;
    const std::size_t previousLength = lines_[previousRow].size();
    lines_[previousRow] += lines_[position.row];
    lines_.erase(lines_.begin() + static_cast<std::ptrdiff_t>(position.row));
    position.row = previousRow;
    position.column = previousLength;
    dirty_ = true;
    return position;
}

Cursor Document::erase(Cursor position) {
    position = clamp(position);
    TextLine& current = lines_[position.row];
    if (position.column < current.size()) {
        current.erase(current.begin() +
                      static_cast<std::ptrdiff_t>(position.column));
        dirty_ = true;
        return position;
    }

    if (position.row + 1 >= lines_.size()) {
        return position;
    }

    lines_[position.row] += lines_[position.row + 1];
    lines_.erase(lines_.begin() + static_cast<std::ptrdiff_t>(position.row + 1));
    dirty_ = true;
    return position;
}

EditorApp::EditorApp(std::filesystem::path path) : path_(std::move(path)) {}

bool EditorApp::loadIfExists(std::string& error) {
    std::error_code ec;
    const bool exists = std::filesystem::exists(path_, ec);
    if (ec) {
        error = "无法访问文件：" + ec.message();
        return false;
    }

    if (!exists) {
        message_ = "新文件：按 t 编辑；退出编辑后输入 #s 保存退出";
        messageIsError_ = false;
        return true;
    }

    if (!std::filesystem::is_regular_file(path_, ec)) {
        if (ec) {
            error = "无法访问文件：" + ec.message();
        } else {
            error = "目标路径不是普通文件";
        }
        return false;
    }

    if (!document_.load(path_, error)) {
        return false;
    }

    cursor_ = {};
    goalColumn_ = 0;
    message_.clear();
    return true;
}

bool EditorApp::handleKey(Key key) {
    if (key.kind == KeyKind::Ignore) {
        return false;
    }
    if (key.kind == KeyKind::Eof) {
        quit(ExitReason::Discarded);
        return true;
    }

    message_.clear();
    messageIsError_ = false;

    if (key.kind != KeyKind::Escape) {
        escapeArmed_ = false;
    }

    if (hashPending_) {
        hashPending_ = false;
        if (key.kind == KeyKind::Character && key.character == U's') {
            return saveAndExit();
        }
        if (key.kind == KeyKind::Character && key.character == U'q') {
            quit(ExitReason::Discarded);
            return true;
        }
        // Any other key cancels the command prefix and is processed normally.
    }

    if (key.kind == KeyKind::CtrlS) {
        return saveAndExit();
    }
    if (key.kind == KeyKind::CtrlQ) {
        quit(ExitReason::Discarded);
        return true;
    }

    if (key.kind == KeyKind::Escape) {
        if (mode_ == EditorMode::Edit) {
            leaveEditMode();
            return false;
        }

        if (document_.dirty() && !escapeArmed_) {
            escapeArmed_ = true;
            message_ = "有未保存的修改；再按一次 Esc 放弃修改并退出";
            messageIsError_ = true;
            return false;
        }

        quit(ExitReason::Discarded);
        return true;
    }

    if (mode_ == EditorMode::Browse && key.kind == KeyKind::Character &&
        key.character == U'#') {
        hashPending_ = true;
        return false;
    }

    if (mode_ == EditorMode::Edit) {
        switch (key.kind) {
            case KeyKind::Character:
                if (key.character >= 0x20U && key.character != 0x7FU) {
                    cursor_ = document_.insert(cursor_, key.character);
                    rememberGoal();
                }
                return false;
            case KeyKind::Enter:
                cursor_ = document_.insertNewline(cursor_);
                rememberGoal();
                return false;
            case KeyKind::Tab:
                cursor_ = document_.insert(cursor_, U'\t');
                rememberGoal();
                return false;
            case KeyKind::Backspace:
                cursor_ = document_.backspace(cursor_);
                rememberGoal();
                return false;
            case KeyKind::Delete:
                cursor_ = document_.erase(cursor_);
                rememberGoal();
                return false;
            case KeyKind::Left:
                moveLeft();
                return false;
            case KeyKind::Right:
                moveRight();
                return false;
            case KeyKind::Up:
                moveUp();
                return false;
            case KeyKind::Down:
                moveDown();
                return false;
            case KeyKind::Home:
                moveHome();
                return false;
            case KeyKind::End:
                moveEnd();
                return false;
            case KeyKind::PageUp:
                movePage(-1);
                return false;
            case KeyKind::PageDown:
                movePage(1);
                return false;
            default:
                return false;
        }
    }

    // Browse mode: read-only navigation plus t/#s/#q.
    switch (key.kind) {
        case KeyKind::Character:
            if (key.character == U't' || key.character == U'T') {
                enterEditMode();
            } else {
                message_ = "按 t 编辑；输入 #s 保存退出；输入 #q 不保存退出";
            }
            return false;
        case KeyKind::Left:
            moveLeft();
            return false;
        case KeyKind::Right:
            moveRight();
            return false;
        case KeyKind::Up:
            moveUp();
            return false;
        case KeyKind::Down:
            moveDown();
            return false;
        case KeyKind::Home:
            moveHome();
            return false;
        case KeyKind::End:
            moveEnd();
            return false;
        case KeyKind::PageUp:
            movePage(-1);
            return false;
        case KeyKind::PageDown:
            movePage(1);
            return false;
        default:
            return false;
    }
}

bool EditorApp::saveAndExit() {
    std::string error;
    if (!document_.save(path_, error)) {
        message_ = "保存失败：" + error;
        messageIsError_ = true;
        return false;
    }

    document_.markSaved();
    exitReason_ = ExitReason::Saved;
    return true;
}

void EditorApp::enterEditMode() {
    mode_ = EditorMode::Edit;
    escapeArmed_ = false;
    message_ = "编辑模式：Esc 返回浏览";
}

void EditorApp::leaveEditMode() {
    mode_ = EditorMode::Browse;
    escapeArmed_ = false;
    message_ = "已返回浏览模式";
}

void EditorApp::moveLeft() {
    if (cursor_.column > 0) {
        --cursor_.column;
    } else if (cursor_.row > 0) {
        --cursor_.row;
        cursor_.column = document_.lineLength(cursor_.row);
    }
    rememberGoal();
}

void EditorApp::moveRight() {
    if (cursor_.column < document_.lineLength(cursor_.row)) {
        ++cursor_.column;
    } else if (cursor_.row + 1 < document_.lineCount()) {
        ++cursor_.row;
        cursor_.column = 0;
    }
    rememberGoal();
}

void EditorApp::moveUp() {
    moveVertical(-1);
}

void EditorApp::moveDown() {
    moveVertical(1);
}

void EditorApp::moveVertical(int delta) {
    if (delta < 0) {
        const std::size_t amount =
            std::min<std::size_t>(cursor_.row, static_cast<std::size_t>(-delta));
        if (amount == 0) {
            return;
        }
        cursor_.row -= amount;
    } else {
        const std::size_t amount =
            std::min<std::size_t>(document_.lineCount() - 1 - cursor_.row,
                                  static_cast<std::size_t>(delta));
        if (amount == 0) {
            return;
        }
        cursor_.row += amount;
    }
    setColumnFromVisual(goalColumn_);
}

void EditorApp::moveHome() {
    cursor_.column = 0;
    rememberGoal();
}

void EditorApp::moveEnd() {
    cursor_.column = document_.lineLength(cursor_.row);
    rememberGoal();
}

void EditorApp::movePage(int direction) {
    constexpr std::size_t kPageLines = 10;
    if (direction < 0) {
        cursor_.row = cursor_.row > kPageLines ? cursor_.row - kPageLines : 0;
    } else {
        cursor_.row = std::min(cursor_.row + kPageLines,
                               document_.lineCount() - 1);
    }
    setColumnFromVisual(goalColumn_);
}

void EditorApp::setColumnFromVisual(std::size_t visualColumn) {
    const TextLine& current = document_.line(cursor_.row);
    std::size_t column = 0;
    std::size_t cells = 0;
    while (column < current.size()) {
        const std::size_t width = cellWidth(current[column], cells);
        if (cells + width > visualColumn) {
            break;
        }
        cells += width;
        ++column;
    }
    cursor_.column = column;
}

std::size_t EditorApp::currentVisualColumn() const noexcept {
    const TextLine& current = document_.line(cursor_.row);
    std::size_t cells = 0;
    for (std::size_t index = 0; index < cursor_.column && index < current.size();
         ++index) {
        cells += cellWidth(current[index], cells);
    }
    return cells;
}

void EditorApp::rememberGoal() {
    goalColumn_ = currentVisualColumn();
}

void EditorApp::quit(ExitReason reason) {
    exitReason_ = reason;
}

}  // namespace piggywrite
