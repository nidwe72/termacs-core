// termacs — C ABI implementation (§7). Thin projection of the C++ facades.
#include "termacs/c/termacs.h"

#include "termacs/application.hpp"
#include "termacs/headless_backend.hpp"
#include "termacs/widget.hpp"
#include "internal.hpp"

#include <cstring>
#include <string>

using namespace termacs;

struct tm_app {
    std::unique_ptr<Application> app;
    CellBuffer                   frame;
};

namespace {

thread_local TmStatus   g_status = TM_OK;
thread_local std::string g_msg;

void clearErr() { g_status = TM_OK; g_msg.clear(); }
void setErr(TmStatus s, const char* m) { g_status = s; g_msg = m ? m : ""; }

Core*    CO(tm_app* a) { return &a->app->core(); }
NodeId   NI(tm_widget h) { return {(uint32_t)(h & 0xffffffffu), (uint32_t)(h >> 32)}; }
tm_widget PK(NodeId id) { return ((uint64_t)id.gen << 32) | id.index; }

size_t copyOut(const std::string& s, char* buf, size_t cap) {
    if (buf && cap) {
        size_t n = s.size() < cap - 1 ? s.size() : cap - 1;
        std::memcpy(buf, s.data(), n);
        buf[n] = 0;
    }
    return s.size();
}

Key mapKey(TmKey k) {
    switch (k) {
        case TM_KEY_CHAR: return Key::Char;
        case TM_KEY_ENTER: return Key::Enter;
        case TM_KEY_ESCAPE: return Key::Escape;
        case TM_KEY_TAB: return Key::Tab;
        case TM_KEY_BACKTAB: return Key::BackTab;
        case TM_KEY_BACKSPACE: return Key::Backspace;
        case TM_KEY_DELETE: return Key::Delete;
        case TM_KEY_INSERT: return Key::Insert;
        case TM_KEY_UP: return Key::Up;
        case TM_KEY_DOWN: return Key::Down;
        case TM_KEY_LEFT: return Key::Left;
        case TM_KEY_RIGHT: return Key::Right;
        case TM_KEY_HOME: return Key::Home;
        case TM_KEY_END: return Key::End;
        case TM_KEY_PAGEUP: return Key::PageUp;
        case TM_KEY_PAGEDOWN: return Key::PageDown;
        case TM_KEY_F1: return Key::F1; case TM_KEY_F2: return Key::F2; case TM_KEY_F3: return Key::F3;
        case TM_KEY_F4: return Key::F4; case TM_KEY_F5: return Key::F5; case TM_KEY_F6: return Key::F6;
        case TM_KEY_F7: return Key::F7; case TM_KEY_F8: return Key::F8; case TM_KEY_F9: return Key::F9;
        case TM_KEY_F10: return Key::F10; case TM_KEY_F11: return Key::F11; case TM_KEY_F12: return Key::F12;
        default: return Key::None;
    }
}

} // namespace

#define TM_BEGIN clearErr(); try {
#define TM_END_V(ret)                                                              \
    } catch (const InvalidHandle&) { setErr(TM_ERR_INVALID_HANDLE, "invalid handle"); return ret; } \
      catch (const std::exception& e) { setErr(TM_ERR_INTERNAL, e.what()); return ret; }
#define TM_END                                                                     \
    } catch (const InvalidHandle&) { setErr(TM_ERR_INVALID_HANDLE, "invalid handle"); } \
      catch (const std::exception& e) { setErr(TM_ERR_INTERNAL, e.what()); }

extern "C" {

int         tm_abi_version(void)      { return TM_ABI_VERSION; }
TmStatus    tm_last_status(void)      { return g_status; }
const char* tm_status_message(void)   { return g_msg.c_str(); }

tm_app* tm_app_new_headless(int w, int h) {
    TM_BEGIN
        auto* a = new tm_app();
        a->app = std::make_unique<Application>(std::make_unique<HeadlessBackend>(w, h));
        return a;
    TM_END_V(nullptr)
    return nullptr;
}
tm_app* tm_app_new_terminal(void) {
    TM_BEGIN
        auto* a = new tm_app();
        a->app = std::make_unique<Application>();
        return a;
    TM_END_V(nullptr)
    return nullptr;
}
void tm_app_free(tm_app* a) { delete a; }

void tm_app_set_theme(tm_app* a, TmTheme t) {
    TM_BEGIN a->app->setTheme(Theme::builtin(t == TM_THEME_LIGHT ? Theme::Builtin::Light : Theme::Builtin::Dark)); TM_END
}
int  tm_app_run(tm_app* a)            { TM_BEGIN return a->app->run(); TM_END_V(0) return 0; }
void tm_app_quit(tm_app* a, int code) { TM_BEGIN a->app->quit(code); TM_END }
void tm_app_post(tm_app* a, TmThunk fn, void* user) {
    TM_BEGIN a->app->postToUiThread([fn, user] { if (fn) fn(user); }); TM_END
}
uint64_t tm_app_start_timer(tm_app* a, int ms, TmThunk fn, void* user) {
    TM_BEGIN return a->app->startTimer(ms, [fn, user] { if (fn) fn(user); }); TM_END_V(0) return 0;
}
void tm_app_stop_timer(tm_app* a, uint64_t id) { TM_BEGIN a->app->stopTimer(id); TM_END }

void tm_app_render(tm_app* a)                 { TM_BEGIN a->frame = a->app->render(); TM_END }
void tm_app_screen_size(tm_app* a, int* w, int* h) {
    TM_BEGIN Size s = a->app->backend().size(); if (w) *w = s.w; if (h) *h = s.h; TM_END
}
size_t tm_app_row_text(tm_app* a, int y, char* buf, size_t cap) {
    TM_BEGIN return copyOut(a->frame.rowText(y), buf, cap); TM_END_V(0) return 0;
}
void tm_app_feed_key(tm_app* a, TmKey key, uint32_t ch) {
    TM_BEGIN
        InputEvent e; e.kind = InputKind::Key; e.key.key = mapKey(key);
        if (key == TM_KEY_CHAR) { e.key.ch = ch; e.key.text = std::string(1, (char)ch); }
        a->app->feed(e);
    TM_END
}

/* windows */
tm_widget tm_create_window(tm_app* a, const char* title) {
    TM_BEGIN return PK(a->app->createWindow(title ? title : "").id()); TM_END_V(0) return 0;
}
void tm_window_resize(tm_app* a, tm_widget w, int ww, int hh) { TM_BEGIN Window(CO(a), NI(w)).resize(ww, hh); TM_END }
tm_widget tm_window_set_menubar(tm_app* a, tm_widget w)      { TM_BEGIN return PK(Window(CO(a), NI(w)).setMenuBar().id()); TM_END_V(0) return 0; }
tm_widget tm_window_set_statusbar(tm_app* a, tm_widget w)    { TM_BEGIN return PK(Window(CO(a), NI(w)).setStatusBar().id()); TM_END_V(0) return 0; }
tm_widget tm_window_set_content_vbox(tm_app* a, tm_widget w) { TM_BEGIN return PK(Window(CO(a), NI(w)).setContentVBox().id()); TM_END_V(0) return 0; }
tm_widget tm_window_set_content_hbox(tm_app* a, tm_widget w) { TM_BEGIN return PK(Window(CO(a), NI(w)).setContentHBox().id()); TM_END_V(0) return 0; }
void tm_window_show(tm_app* a, tm_widget w)                  { TM_BEGIN Window(CO(a), NI(w)).show(); TM_END }

/* containers */
void tm_container_set_padding(tm_app* a, tm_widget c, int v) { TM_BEGIN VBox(CO(a), NI(c)).setPadding(v); TM_END }
void tm_container_set_spacing(tm_app* a, tm_widget c, int v) { TM_BEGIN VBox(CO(a), NI(c)).setSpacing(v); TM_END }
tm_widget tm_add_label(tm_app* a, tm_widget c, const char* t)  { TM_BEGIN return PK(VBox(CO(a), NI(c)).addLabel(t ? t : "").id()); TM_END_V(0) return 0; }
tm_widget tm_add_button(tm_app* a, tm_widget c, const char* t) { TM_BEGIN return PK(VBox(CO(a), NI(c)).addButton(t ? t : "").id()); TM_END_V(0) return 0; }
tm_widget tm_add_lineedit(tm_app* a, tm_widget c)              { TM_BEGIN return PK(VBox(CO(a), NI(c)).addLineEdit().id()); TM_END_V(0) return 0; }
tm_widget tm_add_listview(tm_app* a, tm_widget c)             { TM_BEGIN return PK(VBox(CO(a), NI(c)).addListView().id()); TM_END_V(0) return 0; }
tm_widget tm_add_hbox(tm_app* a, tm_widget c)                { TM_BEGIN return PK(VBox(CO(a), NI(c)).addHBox().id()); TM_END_V(0) return 0; }
tm_widget tm_add_vbox(tm_app* a, tm_widget c)                { TM_BEGIN return PK(VBox(CO(a), NI(c)).addVBox().id()); TM_END_V(0) return 0; }

/* common */
void tm_widget_set_sizing(tm_app* a, tm_widget w, TmAxis ax, int mn, int pref, int mx, int st) {
    TM_BEGIN
        Sizing s; s.min = mn; s.preferred = pref; s.max = (mx <= 0 ? Unbounded : mx); s.stretch = st;
        Widget(CO(a), NI(w)).setSizing(ax == TM_AXIS_V ? Axis::Vertical : Axis::Horizontal, s);
    TM_END
}
void tm_widget_set_focus(tm_app* a, tm_widget w)  { TM_BEGIN Widget(CO(a), NI(w)).setFocus(); TM_END }
void tm_widget_remove(tm_app* a, tm_widget w)     { TM_BEGIN Widget(CO(a), NI(w)).remove(); TM_END }

/* leaves */
void tm_label_set_text(tm_app* a, tm_widget w, const char* t)    { TM_BEGIN Label(CO(a), NI(w)).setText(t ? t : ""); TM_END }
void tm_button_set_text(tm_app* a, tm_widget w, const char* t)   { TM_BEGIN Button(CO(a), NI(w)).setText(t ? t : ""); TM_END }
void tm_lineedit_set_text(tm_app* a, tm_widget w, const char* t) { TM_BEGIN LineEdit(CO(a), NI(w)).setText(t ? t : ""); TM_END }
size_t tm_lineedit_text(tm_app* a, tm_widget w, char* buf, size_t cap) {
    TM_BEGIN return copyOut(LineEdit(CO(a), NI(w)).text(), buf, cap); TM_END_V(0) return 0;
}
void tm_listview_add_item(tm_app* a, tm_widget w, const char* t)  { TM_BEGIN ListView(CO(a), NI(w)).addItem(t ? t : ""); TM_END }
void tm_listview_remove_item(tm_app* a, tm_widget w, int row)     { TM_BEGIN ListView(CO(a), NI(w)).removeItem(row); TM_END }
int  tm_listview_count(tm_app* a, tm_widget w)                    { TM_BEGIN return ListView(CO(a), NI(w)).count(); TM_END_V(0) return 0; }
int  tm_listview_selected(tm_app* a, tm_widget w)                 { TM_BEGIN return ListView(CO(a), NI(w)).selected(); TM_END_V(0) return 0; }
size_t tm_listview_item_at(tm_app* a, tm_widget w, int row, char* buf, size_t cap) {
    TM_BEGIN return copyOut(ListView(CO(a), NI(w)).itemAt(row), buf, cap); TM_END_V(0) return 0;
}

/* menus & status bar */
tm_widget tm_menubar_add_menu(tm_app* a, tm_widget mb, const char* title) {
    TM_BEGIN return PK(MenuBar(CO(a), NI(mb)).addMenu(title ? title : "").id()); TM_END_V(0) return 0;
}
void tm_menu_add_item(tm_app* a, tm_widget menu, const char* label, TmSlot fn, void* user) {
    TM_BEGIN
        tm_widget src = menu;
        Menu(CO(a), NI(menu)).addItem(label ? label : "", [fn, user, src] {
            TmEvent ev{TM_EV_MENU, src, 0, nullptr};
            if (fn) fn(&ev, user);
        });
    TM_END
}
void tm_statusbar_set_text(tm_app* a, tm_widget w, const char* t)       { TM_BEGIN StatusBar(CO(a), NI(w)).setText(t ? t : ""); TM_END }
void tm_statusbar_set_right_text(tm_app* a, tm_widget w, const char* t) { TM_BEGIN StatusBar(CO(a), NI(w)).setRightText(t ? t : ""); TM_END }

/* signals */
void tm_on_clicked(tm_app* a, tm_widget btn, TmSlot fn, void* user) {
    TM_BEGIN tm_widget src = btn;
        Button(CO(a), NI(btn)).clicked().connect([fn, user, src] { TmEvent ev{TM_EV_CLICKED, src, 0, nullptr}; if (fn) fn(&ev, user); });
    TM_END
}
void tm_on_submitted(tm_app* a, tm_widget le, TmSlot fn, void* user) {
    TM_BEGIN tm_widget src = le;
        LineEdit(CO(a), NI(le)).submitted().connect([fn, user, src] { TmEvent ev{TM_EV_SUBMITTED, src, 0, nullptr}; if (fn) fn(&ev, user); });
    TM_END
}
void tm_on_text_changed(tm_app* a, tm_widget le, TmSlot fn, void* user) {
    TM_BEGIN tm_widget src = le;
        LineEdit(CO(a), NI(le)).textChanged().connect([fn, user, src](const std::string& s) {
            TmEvent ev{TM_EV_TEXT_CHANGED, src, 0, s.c_str()}; if (fn) fn(&ev, user);
        });
    TM_END
}
void tm_on_activated(tm_app* a, tm_widget lv, TmSlot fn, void* user) {
    TM_BEGIN tm_widget src = lv;
        ListView(CO(a), NI(lv)).activated().connect([fn, user, src](int row) {
            TmEvent ev{TM_EV_ACTIVATED, src, row, nullptr}; if (fn) fn(&ev, user);
        });
    TM_END
}

/* dialogs */
void tm_dialog_info(tm_app* a, tm_widget win, const char* msg) {
    TM_BEGIN a->app->dialogs().info(Window(CO(a), NI(win)), msg ? msg : ""); TM_END
}
void tm_dialog_confirm(tm_app* a, tm_widget win, const char* msg, TmSlot fn, void* user) {
    TM_BEGIN
        a->app->dialogs().confirm(Window(CO(a), NI(win)), msg ? msg : "", [fn, user](bool yes) {
            TmEvent ev{TM_EV_DIALOG, 0, yes ? 1 : 0, nullptr};
            if (fn) fn(&ev, user);
        });
    TM_END
}

} // extern "C"
