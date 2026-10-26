#include "terminal.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <conio.h>
#include <cwchar>
#else
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace piggywrite {
namespace {

constexpr const char* kEnterScreen = "\x1b[?1049h\x1b[2J\x1b[H\x1b[?25l\x1b[?7l";
constexpr const char* kLeaveScreen = "\x1b[0m\x1b[?7h\x1b[?25h\x1b[?1049l";

#ifdef _WIN32
constexpr unsigned long kEnableVirtualTerminalProcessing = 0x0004UL;

Key extendedKey(wint_t scan) {
    switch (scan) {
        case 72:
            return Key::special(KeyKind::Up);
        case 80:
            return Key::special(KeyKind::Down);
        case 75:
            return Key::special(KeyKind::Left);
        case 77:
            return Key::special(KeyKind::Right);
        case 71:
            return Key::special(KeyKind::Home);
        case 79:
            return Key::special(KeyKind::End);
        case 73:
            return Key::special(KeyKind::PageUp);
        case 81:
            return Key::special(KeyKind::PageDown);
        case 83:
            return Key::special(KeyKind::Delete);
        default:
            return Key::special(KeyKind::Ignore);
    }
}
#else
Key csiKey(const std::string& sequence) {
    if (sequence.empty()) {
        return Key::special(KeyKind::Ignore);
    }

    const char final = sequence.back();
    switch (final) {
        case 'A':
            return Key::special(KeyKind::Up);
        case 'B':
            return Key::special(KeyKind::Down);
        case 'C':
            return Key::special(KeyKind::Right);
        case 'D':
            return Key::special(KeyKind::Left);
        case 'H':
            return Key::special(KeyKind::Home);
        case 'F':
            return Key::special(KeyKind::End);
        case '~': {
            std::size_t cursor = 0;
            while (cursor < sequence.size() &&
                   (sequence[cursor] < '0' || sequence[cursor] > '9')) {
                ++cursor;
            }
            int number = 0;
            while (cursor < sequence.size() && sequence[cursor] >= '0' &&
                   sequence[cursor] <= '9') {
                number = number * 10 + (sequence[cursor] - '0');
                ++cursor;
            }
            switch (number) {
                case 1:
                case 7:
                    return Key::special(KeyKind::Home);
                case 3:
                    return Key::special(KeyKind::Delete);
                case 4:
                case 8:
                    return Key::special(KeyKind::End);
                case 5:
                    return Key::special(KeyKind::PageUp);
                case 6:
                    return Key::special(KeyKind::PageDown);
                default:
                    return Key::special(KeyKind::Ignore);
            }
        }
        default:
            return Key::special(KeyKind::Ignore);
    }
}
#endif

}  // namespace

Terminal::~Terminal() {
    shutdown();
}

bool Terminal::init(std::string& error) {
    if (initialized_) {
        return true;
    }

#ifdef _WIN32
    auto input = GetStdHandle(STD_INPUT_HANDLE);
    auto output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (input == INVALID_HANDLE_VALUE || input == nullptr ||
        output == INVALID_HANDLE_VALUE || output == nullptr) {
        error = "无法获取控制台句柄";
        return false;
    }

    DWORD inputMode = 0;
    DWORD outputMode = 0;
    if (!GetConsoleMode(input, &inputMode)) {
        error = "标准输入不是控制台，请在交互式终端中运行";
        return false;
    }
    if (!GetConsoleMode(output, &outputMode)) {
        error = "标准输出不是控制台，请在交互式终端中运行";
        return false;
    }

    inputHandle_ = input;
    outputHandle_ = output;
    oldInputMode_ = inputMode;
    oldOutputMode_ = outputMode;

    DWORD newInputMode = inputMode;
    newInputMode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT |
                      ENABLE_PROCESSED_INPUT | ENABLE_QUICK_EDIT_MODE |
                      ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT);
    newInputMode |= ENABLE_EXTENDED_FLAGS;

    if (!SetConsoleMode(input, newInputMode)) {
        error = "无法切换控制台输入模式";
        return false;
    }

    if (!SetConsoleMode(output, outputMode | kEnableVirtualTerminalProcessing)) {
        SetConsoleMode(input, oldInputMode_);
        error = "当前终端不支持 ANSI/VT 控制序列";
        return false;
    }

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    initialized_ = true;
    write(kEnterScreen);
    return true;
#else
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        error = "请在交互式终端中运行（当前输入或输出被重定向）";
        return false;
    }

    if (tcgetattr(STDIN_FILENO, &oldTermios_) != 0) {
        error = std::string("读取终端设置失败：") + std::strerror(errno);
        return false;
    }

    struct termios raw = oldTermios_;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= CS8;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) {
        error = std::string("切换终端原始模式失败：") + std::strerror(errno);
        return false;
    }

    termiosSaved_ = true;
    initialized_ = true;
    write(kEnterScreen);
    return true;
#endif
}

void Terminal::shutdown() {
    if (!initialized_) {
        return;
    }

    write(kLeaveScreen);
    initialized_ = false;

#ifdef _WIN32
    if (inputHandle_ != nullptr) {
        SetConsoleMode(static_cast<HANDLE>(inputHandle_), oldInputMode_);
    }
    if (outputHandle_ != nullptr) {
        SetConsoleMode(static_cast<HANDLE>(outputHandle_), oldOutputMode_);
    }
    inputHandle_ = nullptr;
    outputHandle_ = nullptr;
#else
    if (termiosSaved_) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &oldTermios_);
        termiosSaved_ = false;
    }
#endif
}

Key Terminal::readKey() {
#ifdef _WIN32
    wint_t value = _getwch();
    if (value == WEOF) {
        return Key::special(KeyKind::Eof);
    }

    if (value == 0 || value == 0xE0) {
        wint_t scan = _getwch();
        if (scan == WEOF) {
            return Key::special(KeyKind::Eof);
        }
        return extendedKey(scan);
    }

    switch (value) {
        case 3:
            return Key::special(KeyKind::Escape);
        case 8:
        case 127:
            return Key::special(KeyKind::Backspace);
        case 9:
            return Key::special(KeyKind::Tab);
        case 10:
        case 13:
            return Key::special(KeyKind::Enter);
        case 17:
            return Key::special(KeyKind::CtrlQ);
        case 19:
            return Key::special(KeyKind::CtrlS);
        case 27:
            return Key::special(KeyKind::Escape);
        default:
            break;
    }

    if (value < 0x20) {
        return Key::special(KeyKind::Ignore);
    }

    char32_t cp = static_cast<char32_t>(value);
    if (cp >= 0xD800U && cp <= 0xDBFFU) {
        const wint_t low = _getwch();
        if (low == WEOF) {
            return Key::special(KeyKind::Eof);
        }
        if (low >= 0xDC00U && low <= 0xDFFFU) {
            cp = 0x10000U + ((cp - 0xD800U) << 10U) +
                 (static_cast<char32_t>(low) - 0xDC00U);
        } else {
            return Key::special(KeyKind::Ignore);
        }
    }

    return Key::ch(cp);
#else
    auto readByte = [](unsigned char& byte) -> bool {
        for (;;) {
            const ssize_t count = ::read(STDIN_FILENO, &byte, 1);
            if (count == 1) {
                return true;
            }
            if (count < 0 && errno == EINTR) {
                continue;
            }
            return false;
        }
    };

    unsigned char byte = 0;
    if (!readByte(byte)) {
        return Key::special(KeyKind::Eof);
    }

    if (byte == 27) {
        struct pollfd descriptor {};
        descriptor.fd = STDIN_FILENO;
        descriptor.events = POLLIN;
        const int ready = ::poll(&descriptor, 1, 30);
        if (ready <= 0) {
            return Key::special(KeyKind::Escape);
        }

        unsigned char next = 0;
        if (!readByte(next)) {
            return Key::special(KeyKind::Escape);
        }

        if (next != '[' && next != 'O') {
            return Key::special(KeyKind::Escape);
        }

        if (next == 'O') {
            unsigned char code = 0;
            if (!readByte(code)) {
                return Key::special(KeyKind::Ignore);
            }
            switch (code) {
                case 'A':
                    return Key::special(KeyKind::Up);
                case 'B':
                    return Key::special(KeyKind::Down);
                case 'C':
                    return Key::special(KeyKind::Right);
                case 'D':
                    return Key::special(KeyKind::Left);
                case 'H':
                    return Key::special(KeyKind::Home);
                case 'F':
                    return Key::special(KeyKind::End);
                default:
                    return Key::special(KeyKind::Ignore);
            }
        }

        std::string sequence;
        for (std::size_t index = 0; index < 16; ++index) {
            unsigned char code = 0;
            if (!readByte(code)) {
                break;
            }
            sequence.push_back(static_cast<char>(code));
            if (code >= 0x40U && code <= 0x7EU) {
                break;
            }
        }
        return csiKey(sequence);
    }

    if (byte == 8 || byte == 127) {
        return Key::special(KeyKind::Backspace);
    }
    if (byte == 9) {
        return Key::special(KeyKind::Tab);
    }
    if (byte == 10 || byte == 13) {
        return Key::special(KeyKind::Enter);
    }
    if (byte == 3) {
        return Key::special(KeyKind::Escape);
    }
    if (byte == 17) {
        return Key::special(KeyKind::CtrlQ);
    }
    if (byte == 19) {
        return Key::special(KeyKind::CtrlS);
    }
    if (byte < 0x20U) {
        return Key::special(KeyKind::Ignore);
    }

    std::string encoded(1, static_cast<char>(byte));
    std::size_t expected = 0;
    if ((byte & 0xE0U) == 0xC0U) {
        expected = 2;
    } else if ((byte & 0xF0U) == 0xE0U) {
        expected = 3;
    } else if ((byte & 0xF8U) == 0xF0U) {
        expected = 4;
    } else if ((byte & 0x80U) != 0) {
        return Key::special(KeyKind::Ignore);
    }

    for (std::size_t index = 1; index < expected; ++index) {
        unsigned char continuation = 0;
        if (!readByte(continuation)) {
            return Key::special(KeyKind::Eof);
        }
        encoded.push_back(static_cast<char>(continuation));
    }

    if (expected == 0) {
        return Key::ch(static_cast<unsigned char>(byte));
    }

    bool valid = false;
    const std::u32string decoded = fromUtf8(encoded, &valid);
    if (!valid || decoded.size() != 1) {
        return Key::special(KeyKind::Ignore);
    }
    return Key::ch(decoded.front());
#endif
}

int Terminal::columns() const {
#ifdef _WIN32
    if (outputHandle_ == nullptr) {
        return 80;
    }
    CONSOLE_SCREEN_BUFFER_INFO info {};
    if (!GetConsoleScreenBufferInfo(static_cast<HANDLE>(outputHandle_), &info)) {
        return 80;
    }
    return static_cast<int>(info.srWindow.Right - info.srWindow.Left + 1);
#else
    struct winsize size {};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_col > 0) {
        return static_cast<int>(size.ws_col);
    }
    return 80;
#endif
}

int Terminal::rows() const {
#ifdef _WIN32
    if (outputHandle_ == nullptr) {
        return 24;
    }
    CONSOLE_SCREEN_BUFFER_INFO info {};
    if (!GetConsoleScreenBufferInfo(static_cast<HANDLE>(outputHandle_), &info)) {
        return 24;
    }
    return static_cast<int>(info.srWindow.Bottom - info.srWindow.Top + 1);
#else
    struct winsize size {};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_row > 0) {
        return static_cast<int>(size.ws_row);
    }
    return 24;
#endif
}

void Terminal::write(const std::string& utf8) {
    if (utf8.empty()) {
        return;
    }

#ifdef _WIN32
    if (outputHandle_ == nullptr) {
        return;
    }

    int wideLength = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                          utf8.data(),
                                          static_cast<int>(utf8.size()), nullptr,
                                          0);
    if (wideLength <= 0) {
        wideLength = MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                                         static_cast<int>(utf8.size()), nullptr,
                                         0);
    }
    if (wideLength <= 0) {
        return;
    }

    std::wstring wide(static_cast<std::size_t>(wideLength), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()),
                        wide.data(), wideLength);

    HANDLE output = static_cast<HANDLE>(outputHandle_);
    std::size_t offset = 0;
    while (offset < wide.size()) {
        const DWORD chunk = static_cast<DWORD>(
            std::min<std::size_t>(wide.size() - offset, 16384));
        DWORD written = 0;
        if (!WriteConsoleW(output, wide.data() + offset, chunk, &written,
                           nullptr) ||
            written == 0) {
            break;
        }
        offset += written;
    }
#else
    std::size_t offset = 0;
    while (offset < utf8.size()) {
        const ssize_t written =
            ::write(STDOUT_FILENO, utf8.data() + offset, utf8.size() - offset);
        if (written > 0) {
            offset += static_cast<std::size_t>(written);
        } else if (written < 0 && errno == EINTR) {
            continue;
        } else {
            break;
        }
    }
#endif
}

}  // namespace piggywrite
