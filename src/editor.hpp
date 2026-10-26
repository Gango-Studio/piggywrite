#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace piggywrite {

using Codepoint = char32_t;
using TextLine = std::u32string;

enum class KeyKind {
    Character,
    Enter,
    Tab,
    Backspace,
    Delete,
    Left,
    Right,
    Up,
    Down,
    Home,
    End,
    PageUp,
    PageDown,
    Escape,
    CtrlS,
    CtrlQ,
    Eof,
    Ignore,
};

struct Key {
    KeyKind kind = KeyKind::Ignore;
    Codepoint character = U'\0';

    static constexpr Key ch(Codepoint value) noexcept {
        return Key{KeyKind::Character, value};
    }

    static constexpr Key special(KeyKind kind) noexcept {
        return Key{kind, U'\0'};
    }
};

struct Cursor {
    std::size_t row = 0;
    std::size_t column = 0;

    friend bool operator==(const Cursor& lhs, const Cursor& rhs) noexcept {
        return lhs.row == rhs.row && lhs.column == rhs.column;
    }

    friend bool operator!=(const Cursor& lhs, const Cursor& rhs) noexcept {
        return !(lhs == rhs);
    }
};

// Text is UTF-8 on the outside and UTF-32 internally, so cursor movement and
// deletion always work on complete Unicode code points.
std::u32string fromUtf8(std::string_view text,
                        bool* valid = nullptr,
                        std::size_t* badOffset = nullptr);
std::string toUtf8(std::u32string_view text);

// Terminal cell width (2 for CJK/emoji, 1 for most characters, 0 for combining
// marks). Tabs expand to the next multiple of four cells.
std::size_t cellWidth(Codepoint cp, std::size_t column) noexcept;
std::size_t visualWidth(std::u32string_view text) noexcept;

class Document {
public:
    Document() = default;

    bool load(const std::filesystem::path& path, std::string& error);
    bool save(const std::filesystem::path& path, std::string& error) const;

    const std::vector<TextLine>& lines() const noexcept { return lines_; }
    std::size_t lineCount() const noexcept { return lines_.size(); }
    const TextLine& line(std::size_t row) const;
    std::size_t lineLength(std::size_t row) const;

    bool dirty() const noexcept { return dirty_; }
    void markSaved() noexcept { dirty_ = false; }
    bool hasBom() const noexcept { return hasBom_; }
    const std::string& eol() const noexcept { return eol_; }

    Cursor insert(Cursor position, Codepoint cp);
    Cursor insertText(Cursor position, std::u32string_view text);
    Cursor insertNewline(Cursor position);
    Cursor backspace(Cursor position);
    Cursor erase(Cursor position);

    bool isCursorValid(Cursor position) const noexcept;
    Cursor clamp(Cursor position) const noexcept;

private:
    std::vector<TextLine> lines_{TextLine{}};
    std::string eol_ = "\n";
    bool hasBom_ = false;
    bool dirty_ = false;
};

enum class EditorMode {
    Browse,
    Edit,
};

enum class ExitReason {
    None,
    Saved,
    Discarded,
};

class EditorApp {
public:
    explicit EditorApp(std::filesystem::path path);

    bool loadIfExists(std::string& error);

    // Returns true when the application should leave its main loop.
    bool handleKey(Key key);

    EditorMode mode() const noexcept { return mode_; }
    const Document& document() const noexcept { return document_; }
    const Cursor& cursor() const noexcept { return cursor_; }
    const std::filesystem::path& path() const noexcept { return path_; }
    bool hashPending() const noexcept { return hashPending_; }
    bool escapeArmed() const noexcept { return escapeArmed_; }
    const std::string& message() const noexcept { return message_; }
    bool messageIsError() const noexcept { return messageIsError_; }
    ExitReason exitReason() const noexcept { return exitReason_; }

private:
    bool saveAndExit();
    void enterEditMode();
    void leaveEditMode();
    void moveLeft();
    void moveRight();
    void moveUp();
    void moveDown();
    void moveVertical(int delta);
    void moveHome();
    void moveEnd();
    void movePage(int direction);
    void setColumnFromVisual(std::size_t visualColumn);
    std::size_t currentVisualColumn() const noexcept;
    void rememberGoal();
    void quit(ExitReason reason);

    std::filesystem::path path_;
    Document document_;
    Cursor cursor_{};
    std::size_t goalColumn_ = 0;
    EditorMode mode_ = EditorMode::Browse;
    bool hashPending_ = false;
    bool escapeArmed_ = false;
    bool messageIsError_ = false;
    std::string message_;
    ExitReason exitReason_ = ExitReason::None;
};

}  // namespace piggywrite
