// termacs — TvisionBackend: drives magiblot/tvision purely as a cell-buffer + input
// source via THardwareInfo (no TApplication/TView). This is the §6 backend seam impl.
#define Uses_TEvent
#define Uses_TKeys
#define Uses_TText
#define Uses_THardwareInfo
#include <tvision/tv.h>
#include <tvision/hardware.h>

#include "termacs/tvision_backend.hpp"
#include <string>
#include <vector>

namespace termacs {
namespace {

TColorDesired toDesired(const Color& c, bool fg) {
    if (c.isDefault) return fg ? TColorRGB(192, 192, 192) : TColorRGB(0, 0, 0);
    return TColorRGB(c.r, c.g, c.b);
}

TColorAttr toAttr(const Style& s) {
    Color fg = s.fg, bg = s.bg;
    if (s.reverse) std::swap(fg, bg);
    ushort style = 0;
    if (s.bold) style |= slBold;
    if (s.underline) style |= slUnderline;
    return TColorAttr(toDesired(fg, true), toDesired(bg, false), style);
}

void encodeUtf8(char32_t c, std::string& out) {
    if (c < 0x80) out.push_back((char)c);
    else if (c < 0x800) { out.push_back((char)(0xC0 | (c >> 6))); out.push_back((char)(0x80 | (c & 0x3F))); }
    else if (c < 0x10000) { out.push_back((char)(0xE0 | (c >> 12))); out.push_back((char)(0x80 | ((c >> 6) & 0x3F))); out.push_back((char)(0x80 | (c & 0x3F))); }
    else { out.push_back((char)(0xF0 | (c >> 18))); out.push_back((char)(0x80 | ((c >> 12) & 0x3F))); out.push_back((char)(0x80 | ((c >> 6) & 0x3F))); out.push_back((char)(0x80 | (c & 0x3F))); }
}

Key mapKey(ushort kc) {
    switch (kc) {
        case kbEnter:    return Key::Enter;
        case kbEsc:      return Key::Escape;
        case kbTab:      return Key::Tab;
        case kbShiftTab: return Key::BackTab;
        case kbBack:     return Key::Backspace;
        case kbDel:      return Key::Delete;
        case kbIns:      return Key::Insert;
        case kbUp:       return Key::Up;
        case kbDown:     return Key::Down;
        case kbLeft:     return Key::Left;
        case kbRight:    return Key::Right;
        case kbHome:     return Key::Home;
        case kbEnd:      return Key::End;
        case kbPgUp:     return Key::PageUp;
        case kbPgDn:     return Key::PageDown;
        case kbF1:       return Key::F1;  case kbF2: return Key::F2;  case kbF3: return Key::F3;
        case kbF4:       return Key::F4;  case kbF5: return Key::F5;  case kbF6: return Key::F6;
        case kbF7:       return Key::F7;  case kbF8: return Key::F8;  case kbF9: return Key::F9;
        case kbF10:      return Key::F10;
        default:         return Key::None;
    }
}

class TvisionBackend : public Backend {
public:
    TvisionBackend() {
        // Constructing THardwareInfo initializes its static platform pointer
        // (platf = &Platform::getInstance()); the static methods segfault without it.
        THardwareInfo::setUpConsole();
        // TScreen's ctor does setUpConsole THEN allocateScreenBuffer — the second call
        // sizes tvision's internal display buffer (what screenWrite blits into) and the
        // reported screen dimensions. Without it nothing paints.
        THardwareInfo::allocateScreenBuffer();
        last_ = curSize();
    }
    ~TvisionBackend() override { THardwareInfo::restoreConsole(); }

    Size size() override { return curSize(); }

    Capabilities caps() override {
        return {ColorDepth::TrueColor, THardwareInfo::getButtonCount() > 0, true};
    }

    int measureWidth(std::string_view utf8) override {
        return (int)TText::width(TStringView(utf8.data(), utf8.size()));
    }

    void present(const CellBuffer& buf) override {
        int w = buf.width(), h = buf.height();
        if (w <= 0 || h <= 0) return;
        std::vector<TScreenCell> row(w);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w;) {
                const Cell& c = buf.at(x, y);
                if (c.wideTrail) { ++x; continue; }
                std::string u;
                encodeUtf8(c.ch, u);
                size_t adv = TText::drawStr(TSpan<TScreenCell>(&row[x], w - x), TStringView(u), toAttr(c.style));
                x += (adv > 0 ? (int)adv : 1);
            }
            THardwareInfo::screenWrite(0, y, row.data(), w);
        }
        THardwareInfo::flushScreen();
    }

    bool pollInput(InputEvent& out, int timeoutMs) override {
        THardwareInfo::waitForEvents(timeoutMs);
        TEvent ev;
        if (THardwareInfo::getKeyEvent(ev) && ev.what == evKeyDown) {
            out.kind = InputKind::Key;
            KeyEvent& k = out.key;
            k.mods.shift = (ev.keyDown.controlKeyState & kbShift) != 0;
            k.mods.ctrl  = (ev.keyDown.controlKeyState & kbCtrlShift) != 0;
            k.mods.alt   = (ev.keyDown.controlKeyState & kbAltShift) != 0;
            Key mapped = mapKey(ev.keyDown.keyCode);
            if (mapped != Key::None) { k.key = mapped; return true; }
            // printable: take the text payload
            if (ev.keyDown.textLength > 0) {
                k.key = Key::Char;
                k.text.assign(ev.keyDown.text, ev.keyDown.textLength);
                k.ch = (unsigned char)ev.keyDown.text[0];   // ASCII-simple
                return true;
            }
            return false;
        }
        MouseEventType m;
        if (THardwareInfo::getMouseEvent(m)) {
            out.kind = InputKind::Mouse;
            out.mouse.x = m.where.x; out.mouse.y = m.where.y;
            out.mouse.left = (m.buttons & mbLeftButton) != 0;
            out.mouse.right = (m.buttons & mbRightButton) != 0;
            out.mouse.wheel = (m.wheel & mwUp) ? 1 : ((m.wheel & mwDown) ? -1 : 0);
            return true;
        }
        THardwareInfo::allocateScreenBuffer();    // re-read size; resizes the display buffer
        Size now = curSize();
        if (now.w != last_.w || now.h != last_.h) {
            last_ = now;
            out.kind = InputKind::Resize;
            out.resize = {now.w, now.h};
            return true;
        }
        return false;
    }

    void wake() override { THardwareInfo::interruptEventWait(); }

    void setCursor(int x, int y, bool visible) override {
        if (visible) { THardwareInfo::setCaretPosition((ushort)x, (ushort)y); }
    }

    void shutdown() override { THardwareInfo::restoreConsole(); }

private:
    THardwareInfo hw_;     // MUST exist: its ctor initializes the static platform pointer
    Size          last_{};
    static Size curSize() { return {(int)THardwareInfo::getScreenCols(), (int)THardwareInfo::getScreenRows()}; }
};

} // namespace

std::unique_ptr<Backend> makeTerminalBackend() {
    return std::make_unique<TvisionBackend>();
}

} // namespace termacs
