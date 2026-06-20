// termacs — INTERNAL text-editing model (§5.11), backed by vendored librope.
// Pure model: text + cursor + selection + edits + navigation. NO rendering.
// Offsets are CODEPOINT indices (librope's char positions); shared by LineEdit
// (single-line) and TextArea (multi-line).
#pragma once
#include <string>
#include <utility>

namespace termacs {

class TextBuffer {
public:
    TextBuffer();
    ~TextBuffer();
    TextBuffer(const TextBuffer&) = delete;
    TextBuffer& operator=(const TextBuffer&) = delete;

    // ---- content ----
    void        setText(const std::string& utf8);
    std::string text() const;
    int         length() const;                  // codepoint count

    // ---- cursor / selection (codepoint offsets) ----
    int  cursor() const { return cursor_; }
    int  anchor() const { return anchor_; }
    bool hasSelection() const { return cursor_ != anchor_; }
    std::pair<int, int> selectionRange() const;  // ordered {lo, hi}
    std::string selectedText() const;
    void selectAll();
    void clearSelection();
    void setSelection(int anchor, int cursor);

    // ---- movement (extend = shift-select) ----
    void moveTo(int pos, bool extend);
    void moveLeft(bool extend);      void moveRight(bool extend);
    void moveWordLeft(bool extend);  void moveWordRight(bool extend);
    void moveHome(bool extend);      void moveEnd(bool extend);     // line
    void moveDocStart(bool extend);  void moveDocEnd(bool extend);
    void moveUp(bool extend);        void moveDown(bool extend);    // multi-line

    // ---- editing (insert replaces a non-empty selection) ----
    void insert(const std::string& utf8);
    void backspace();        void del();
    void deleteWordLeft();   void deleteWordRight();

    // ---- clipboard helpers ----
    std::string copy() const;                    // selection (or "")
    std::string cut();                           // removes + returns selection
    void        paste(const std::string& utf8);  // replaces selection

    // ---- rendering helpers ----
    bool        multiline() const { return multiline_; }
    void        setMultiline(bool m) { multiline_ = m; }
    int         lineCount() const;
    std::string lineAt(int row) const;
    std::pair<int, int> cursorRowCol() const;    // {row, col}
    std::pair<int, int> rowColOf(int pos) const;
    int         offsetOf(int row, int col) const;

private:
    const std::u32string& cps() const;           // lazily-rebuilt codepoint cache
    void rawDelete(int lo, int hi);              // on rope + invalidate cache
    void rawInsert(int pos, const std::string& utf8);
    void deleteSelection();                       // collapses to lo
    void place(int pos, bool extend);             // set cursor, maybe keep anchor

    void*                  r_ = nullptr;   // librope rope* (opaque here)
    int                    cursor_ = 0, anchor_ = 0;
    bool                   multiline_ = false;
    mutable std::u32string cache_;
    mutable bool           dirty_ = true;
};

} // namespace termacs
