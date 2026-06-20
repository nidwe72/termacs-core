#include "text_buffer.hpp"
#include <rope.h>
#include <algorithm>
#include <cstdint>
#include <cstdlib>

#define RP ((rope*)r_)   // r_ is stored as void* (librope's rope is an anonymous-struct typedef)

namespace termacs {
namespace {

// --- UTF-8 helpers --------------------------------------------------------
std::u32string decode(const std::string& s) {
    std::u32string out;
    size_t i = 0, n = s.size();
    while (i < n) {
        unsigned char c = s[i];
        char32_t cp; int len;
        if (c < 0x80)            { cp = c;            len = 1; }
        else if ((c >> 5) == 6)  { cp = c & 0x1F;     len = 2; }
        else if ((c >> 4) == 14) { cp = c & 0x0F;     len = 3; }
        else if ((c >> 3) == 30) { cp = c & 0x07;     len = 4; }
        else                     { cp = c;            len = 1; }   // tolerate junk
        for (int k = 1; k < len && i + k < n; ++k) cp = (cp << 6) | (s[i + k] & 0x3F);
        out.push_back(cp);
        i += len;
    }
    return out;
}
void encodeCp(char32_t c, std::string& out) {
    if (c < 0x80) out.push_back((char)c);
    else if (c < 0x800) { out.push_back((char)(0xC0 | (c >> 6))); out.push_back((char)(0x80 | (c & 0x3F))); }
    else if (c < 0x10000) { out.push_back((char)(0xE0 | (c >> 12))); out.push_back((char)(0x80 | ((c >> 6) & 0x3F))); out.push_back((char)(0x80 | (c & 0x3F))); }
    else { out.push_back((char)(0xF0 | (c >> 18))); out.push_back((char)(0x80 | ((c >> 12) & 0x3F))); out.push_back((char)(0x80 | ((c >> 6) & 0x3F))); out.push_back((char)(0x80 | (c & 0x3F))); }
}
std::string encode(const std::u32string& s, int lo, int hi) {
    std::string out;
    for (int i = lo; i < hi && i < (int)s.size(); ++i) encodeCp(s[i], out);
    return out;
}
int cpCount(const std::string& utf8) { return (int)decode(utf8).size(); }
bool isWord(char32_t c) { return c == U'_' || (c > 127) || (c >= U'0' && c <= U'9') || (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z'); }

} // namespace

TextBuffer::TextBuffer() { r_ = rope_new(); }
TextBuffer::~TextBuffer() { if (r_) rope_free(RP); }

const std::u32string& TextBuffer::cps() const {
    if (dirty_) {
        uint8_t* s = rope_create_cstr(RP);
        cache_ = decode(reinterpret_cast<const char*>(s));
        free(s);
        dirty_ = false;
    }
    return cache_;
}

void TextBuffer::rawInsert(int pos, const std::string& utf8) {
    rope_insert(RP, (size_t)pos, reinterpret_cast<const uint8_t*>(utf8.c_str()));
    dirty_ = true;
}
void TextBuffer::rawDelete(int lo, int hi) {
    if (hi > lo) { rope_del(RP, (size_t)lo, (size_t)(hi - lo)); dirty_ = true; }
}

void TextBuffer::setText(const std::string& utf8) {
    if (r_) rope_free(RP);
    r_ = rope_new_with_utf8(reinterpret_cast<const uint8_t*>(utf8.c_str()));
    dirty_ = true;
    cursor_ = anchor_ = 0;
}
std::string TextBuffer::text() const {
    uint8_t* s = rope_create_cstr(RP);
    std::string out(reinterpret_cast<const char*>(s));
    free(s);
    return out;
}
int TextBuffer::length() const { return (int)rope_char_count(RP); }

std::pair<int, int> TextBuffer::selectionRange() const {
    return {std::min(cursor_, anchor_), std::max(cursor_, anchor_)};
}
std::string TextBuffer::selectedText() const {
    auto [lo, hi] = selectionRange();
    return encode(cps(), lo, hi);
}
void TextBuffer::selectAll()        { anchor_ = 0; cursor_ = length(); }
void TextBuffer::clearSelection()   { anchor_ = cursor_; }
void TextBuffer::setSelection(int a, int c) {
    int n = length();
    anchor_ = std::max(0, std::min(a, n));
    cursor_ = std::max(0, std::min(c, n));
}

void TextBuffer::place(int pos, bool extend) {
    int n = length();
    cursor_ = std::max(0, std::min(pos, n));
    if (!extend) anchor_ = cursor_;
}
void TextBuffer::moveTo(int pos, bool extend) { place(pos, extend); }

void TextBuffer::moveLeft(bool extend) {
    if (hasSelection() && !extend) { cursor_ = anchor_ = selectionRange().first; return; }
    place(cursor_ - 1, extend);
}
void TextBuffer::moveRight(bool extend) {
    if (hasSelection() && !extend) { cursor_ = anchor_ = selectionRange().second; return; }
    place(cursor_ + 1, extend);
}
void TextBuffer::moveWordLeft(bool extend) {
    const auto& s = cps(); int p = cursor_;
    while (p > 0 && !isWord(s[p - 1])) --p;
    while (p > 0 &&  isWord(s[p - 1])) --p;
    place(p, extend);
}
void TextBuffer::moveWordRight(bool extend) {
    const auto& s = cps(); int p = cursor_, n = (int)s.size();
    while (p < n &&  isWord(s[p])) ++p;
    while (p < n && !isWord(s[p])) ++p;
    place(p, extend);
}
void TextBuffer::moveHome(bool extend) {
    const auto& s = cps(); int p = cursor_;
    while (p > 0 && s[p - 1] != U'\n') --p;
    place(p, extend);
}
void TextBuffer::moveEnd(bool extend) {
    const auto& s = cps(); int p = cursor_, n = (int)s.size();
    while (p < n && s[p] != U'\n') ++p;
    place(p, extend);
}
void TextBuffer::moveDocStart(bool extend) { place(0, extend); }
void TextBuffer::moveDocEnd(bool extend)   { place(length(), extend); }

std::pair<int, int> TextBuffer::rowColOf(int pos) const {
    const auto& s = cps(); int row = 0, col = 0;
    for (int i = 0; i < pos && i < (int)s.size(); ++i) {
        if (s[i] == U'\n') { ++row; col = 0; } else ++col;
    }
    return {row, col};
}
std::pair<int, int> TextBuffer::cursorRowCol() const { return rowColOf(cursor_); }

int TextBuffer::offsetOf(int row, int col) const {
    const auto& s = cps(); int r = 0, i = 0, n = (int)s.size();
    while (i < n && r < row) { if (s[i] == U'\n') ++r; ++i; }
    int c = 0;
    while (i < n && c < col && s[i] != U'\n') { ++i; ++c; }
    return i;
}
void TextBuffer::moveUp(bool extend) {
    auto [row, col] = cursorRowCol();
    if (row == 0) { place(0, extend); return; }
    place(offsetOf(row - 1, col), extend);
}
void TextBuffer::moveDown(bool extend) {
    auto [row, col] = cursorRowCol();
    if (row >= lineCount() - 1) { place(length(), extend); return; }
    place(offsetOf(row + 1, col), extend);
}

void TextBuffer::deleteSelection() {
    auto [lo, hi] = selectionRange();
    rawDelete(lo, hi);
    cursor_ = anchor_ = lo;
}
void TextBuffer::insert(const std::string& utf8) {
    if (hasSelection()) deleteSelection();
    std::string s = multiline_ ? utf8 : std::string();   // strip newlines for single-line
    if (!multiline_) { for (char c : utf8) if (c != '\n' && c != '\r') s.push_back(c); }
    else s = utf8;
    rawInsert(cursor_, s);
    cursor_ += cpCount(s);
    anchor_ = cursor_;
}
void TextBuffer::backspace() {
    if (hasSelection()) { deleteSelection(); return; }
    if (cursor_ > 0) { rawDelete(cursor_ - 1, cursor_); --cursor_; anchor_ = cursor_; }
}
void TextBuffer::del() {
    if (hasSelection()) { deleteSelection(); return; }
    if (cursor_ < length()) rawDelete(cursor_, cursor_ + 1);
}
void TextBuffer::deleteWordLeft() {
    if (hasSelection()) { deleteSelection(); return; }
    int end = cursor_; moveWordLeft(false); rawDelete(cursor_, end); anchor_ = cursor_;
}
void TextBuffer::deleteWordRight() {
    if (hasSelection()) { deleteSelection(); return; }
    int start = cursor_; moveWordRight(false); rawDelete(start, cursor_); cursor_ = anchor_ = start;
}

std::string TextBuffer::copy() const { return selectedText(); }
std::string TextBuffer::cut() {
    std::string s = selectedText();
    if (hasSelection()) deleteSelection();
    return s;
}
void TextBuffer::paste(const std::string& utf8) { insert(utf8); }

int TextBuffer::lineCount() const {
    const auto& s = cps(); int n = 1;
    for (char32_t c : s) if (c == U'\n') ++n;
    return n;
}
std::string TextBuffer::lineAt(int row) const {
    const auto& s = cps(); int r = 0, i = 0, n = (int)s.size();
    while (i < n && r < row) { if (s[i] == U'\n') ++r; ++i; }
    int start = i;
    while (i < n && s[i] != U'\n') ++i;
    return encode(s, start, i);
}

} // namespace termacs
