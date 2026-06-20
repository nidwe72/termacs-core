/* termacs — flat C ABI (§7). The single transport surface that Python (cffi),
 * Dart (dart:ffi), and the Java JNI shim bind to. Opaque handles, one variant
 * TmEvent + one slot type, copy-out strings, a thread-local error channel. */
#ifndef TERMACS_C_TERMACS_H
#define TERMACS_C_TERMACS_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#  define TM_EXPORT __declspec(dllexport)
#else
#  define TM_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ----- ABI version & error channel ----- */
#define TM_ABI_VERSION 2   /* 2: P5 selection & input widgets (§5.10) */
TM_EXPORT int         tm_abi_version(void);

typedef enum { TM_OK = 0, TM_ERR_INVALID_HANDLE = 1, TM_ERR_BAD_ARG = 2, TM_ERR_INTERNAL = 3 } TmStatus;
TM_EXPORT TmStatus    tm_last_status(void);
TM_EXPORT const char* tm_status_message(void);

/* ----- handles ----- */
typedef struct tm_app tm_app;     /* opaque Application */
typedef uint64_t      tm_widget;  /* packed node handle; 0 = null */

/* ----- enums (generated mirrors live in each binding) ----- */
typedef enum {
    TM_KEY_NONE = 0, TM_KEY_CHAR,
    TM_KEY_ENTER, TM_KEY_ESCAPE, TM_KEY_TAB, TM_KEY_BACKTAB, TM_KEY_BACKSPACE,
    TM_KEY_DELETE, TM_KEY_INSERT,
    TM_KEY_UP, TM_KEY_DOWN, TM_KEY_LEFT, TM_KEY_RIGHT,
    TM_KEY_HOME, TM_KEY_END, TM_KEY_PAGEUP, TM_KEY_PAGEDOWN,
    TM_KEY_F1, TM_KEY_F2, TM_KEY_F3, TM_KEY_F4, TM_KEY_F5, TM_KEY_F6,
    TM_KEY_F7, TM_KEY_F8, TM_KEY_F9, TM_KEY_F10, TM_KEY_F11, TM_KEY_F12
} TmKey;

typedef enum { TM_AXIS_H = 0, TM_AXIS_V = 1 } TmAxis;
typedef enum { TM_THEME_DARK = 0, TM_THEME_LIGHT = 1 } TmTheme;

/* ----- the one variant event + slot ----- */
typedef enum {
    TM_EV_CLICKED, TM_EV_TEXT_CHANGED, TM_EV_SUBMITTED,
    TM_EV_ACTIVATED, TM_EV_MENU, TM_EV_DIALOG,
    TM_EV_TOGGLED, TM_EV_SELECTION_CHANGED
} TmEventKind;

typedef struct {
    TmEventKind kind;
    tm_widget   source;
    int32_t     i;       /* activated row / dialog button / confirm(1=yes) */
    const char* text;    /* textChanged value (valid only during the call) */
} TmEvent;

typedef void (*TmSlot)(const TmEvent* ev, void* user);
typedef void (*TmThunk)(void* user);   /* postToUiThread / timers */

/* ----- application ----- */
TM_EXPORT tm_app*  tm_app_new_headless(int w, int h);
TM_EXPORT tm_app*  tm_app_new_terminal(void);
TM_EXPORT void     tm_app_free(tm_app*);
TM_EXPORT void     tm_app_set_theme(tm_app*, TmTheme);
TM_EXPORT int      tm_app_run(tm_app*);
TM_EXPORT void     tm_app_quit(tm_app*, int code);
TM_EXPORT void     tm_app_post(tm_app*, TmThunk fn, void* user);
TM_EXPORT uint64_t tm_app_start_timer(tm_app*, int interval_ms, TmThunk fn, void* user);
TM_EXPORT void     tm_app_stop_timer(tm_app*, uint64_t id);

/* testing / headless helpers */
TM_EXPORT void     tm_app_render(tm_app*);
TM_EXPORT void     tm_app_screen_size(tm_app*, int* w, int* h);
TM_EXPORT size_t   tm_app_row_text(tm_app*, int y, char* buf, size_t cap); /* copy-out; returns full length */
TM_EXPORT void     tm_app_feed_key(tm_app*, TmKey key, uint32_t ch);

/* ----- windows ----- */
TM_EXPORT tm_widget tm_create_window(tm_app*, const char* title);
TM_EXPORT void      tm_window_resize(tm_app*, tm_widget win, int w, int h);
TM_EXPORT tm_widget tm_window_set_menubar(tm_app*, tm_widget win);
TM_EXPORT tm_widget tm_window_set_statusbar(tm_app*, tm_widget win);
TM_EXPORT tm_widget tm_window_set_content_vbox(tm_app*, tm_widget win);
TM_EXPORT tm_widget tm_window_set_content_hbox(tm_app*, tm_widget win);
TM_EXPORT void      tm_window_show(tm_app*, tm_widget win);

/* ----- containers (VBox/HBox) ----- */
TM_EXPORT void      tm_container_set_padding(tm_app*, tm_widget c, int cells);
TM_EXPORT void      tm_container_set_spacing(tm_app*, tm_widget c, int cells);
TM_EXPORT tm_widget tm_add_label(tm_app*, tm_widget c, const char* text);
TM_EXPORT tm_widget tm_add_button(tm_app*, tm_widget c, const char* text);
TM_EXPORT tm_widget tm_add_lineedit(tm_app*, tm_widget c);
TM_EXPORT tm_widget tm_add_listview(tm_app*, tm_widget c);
TM_EXPORT tm_widget tm_add_hbox(tm_app*, tm_widget c);
TM_EXPORT tm_widget tm_add_vbox(tm_app*, tm_widget c);

/* ----- common widget ops ----- */
TM_EXPORT void tm_widget_set_sizing(tm_app*, tm_widget w, TmAxis axis, int min, int preferred, int max, int stretch);
TM_EXPORT void tm_widget_set_focus(tm_app*, tm_widget w);
TM_EXPORT void tm_widget_remove(tm_app*, tm_widget w);

/* ----- leaf widgets ----- */
TM_EXPORT void   tm_label_set_text(tm_app*, tm_widget, const char* text);
TM_EXPORT void   tm_button_set_text(tm_app*, tm_widget, const char* text);
TM_EXPORT void   tm_lineedit_set_text(tm_app*, tm_widget, const char* text);
TM_EXPORT size_t tm_lineedit_text(tm_app*, tm_widget, char* buf, size_t cap);  /* copy-out */
TM_EXPORT void   tm_listview_add_item(tm_app*, tm_widget, const char* text);
TM_EXPORT void   tm_listview_remove_item(tm_app*, tm_widget, int row);
TM_EXPORT int    tm_listview_count(tm_app*, tm_widget);
TM_EXPORT int    tm_listview_selected(tm_app*, tm_widget);
TM_EXPORT size_t tm_listview_item_at(tm_app*, tm_widget, int row, char* buf, size_t cap);  /* copy-out */

/* ----- menus & status bar ----- */
TM_EXPORT tm_widget tm_menubar_add_menu(tm_app*, tm_widget mb, const char* title);
TM_EXPORT void      tm_menu_add_item(tm_app*, tm_widget menu, const char* label, TmSlot fn, void* user);
TM_EXPORT void      tm_statusbar_set_text(tm_app*, tm_widget, const char* left);
TM_EXPORT void      tm_statusbar_set_right_text(tm_app*, tm_widget, const char* right);

/* ----- signals (one slot type for all) ----- */
TM_EXPORT void tm_on_clicked(tm_app*, tm_widget btn, TmSlot fn, void* user);
TM_EXPORT void tm_on_submitted(tm_app*, tm_widget le, TmSlot fn, void* user);
TM_EXPORT void tm_on_text_changed(tm_app*, tm_widget le, TmSlot fn, void* user);
TM_EXPORT void tm_on_activated(tm_app*, tm_widget lv, TmSlot fn, void* user);

/* ----- P5 selection & input widgets (§5.10) ----- */
TM_EXPORT void   tm_button_set_variant(tm_app*, tm_widget, int variant);  /* 0..3 */
TM_EXPORT void   tm_button_set_default(tm_app*, tm_widget, int isDefault);

TM_EXPORT tm_widget tm_add_checkbox(tm_app*, tm_widget c, const char* text);
TM_EXPORT void   tm_checkbox_set_checked(tm_app*, tm_widget, int checked);
TM_EXPORT int    tm_checkbox_is_checked(tm_app*, tm_widget);
TM_EXPORT void   tm_on_toggled(tm_app*, tm_widget, TmSlot fn, void* user);  /* ev.i: 1=checked */

TM_EXPORT tm_widget tm_add_optiongroup(tm_app*, tm_widget c, int mode);     /* 0 One, 1 Many */
TM_EXPORT void   tm_optiongroup_set_options(tm_app*, tm_widget, const char* nl_joined);
TM_EXPORT void   tm_optiongroup_set_orientation(tm_app*, tm_widget, TmAxis);
TM_EXPORT void   tm_optiongroup_set_selected_index(tm_app*, tm_widget, int i);
TM_EXPORT int    tm_optiongroup_selected_index(tm_app*, tm_widget);
TM_EXPORT void   tm_optiongroup_set_selected(tm_app*, tm_widget, int i, int on);
TM_EXPORT int    tm_optiongroup_is_selected(tm_app*, tm_widget, int i);
TM_EXPORT void   tm_on_option_changed(tm_app*, tm_widget, TmSlot fn, void* user);

TM_EXPORT tm_widget tm_add_combobox(tm_app*, tm_widget c);
TM_EXPORT void   tm_combobox_set_options(tm_app*, tm_widget, const char* nl_joined);
TM_EXPORT void   tm_combobox_set_selected_index(tm_app*, tm_widget, int i);
TM_EXPORT int    tm_combobox_selected_index(tm_app*, tm_widget);
TM_EXPORT size_t tm_combobox_selected_text(tm_app*, tm_widget, char* buf, size_t cap);
TM_EXPORT void   tm_combobox_set_placeholder(tm_app*, tm_widget, const char* text);
TM_EXPORT void   tm_on_combo_changed(tm_app*, tm_widget, TmSlot fn, void* user);

TM_EXPORT tm_widget tm_add_progressbar(tm_app*, tm_widget c);
TM_EXPORT void   tm_progressbar_set_value(tm_app*, tm_widget, int percent);
TM_EXPORT int    tm_progressbar_value(tm_app*, tm_widget);

TM_EXPORT tm_widget tm_add_textarea(tm_app*, tm_widget c);
TM_EXPORT void   tm_textarea_set_text(tm_app*, tm_widget, const char* text);
TM_EXPORT size_t tm_textarea_text(tm_app*, tm_widget, char* buf, size_t cap);
TM_EXPORT void   tm_textarea_append_line(tm_app*, tm_widget, const char* text);
TM_EXPORT void   tm_textarea_set_readonly(tm_app*, tm_widget, int ro);
TM_EXPORT void   tm_textarea_set_wordwrap(tm_app*, tm_widget, int wrap);
TM_EXPORT void   tm_textarea_set_placeholder(tm_app*, tm_widget, const char* text);
TM_EXPORT void   tm_on_textarea_changed(tm_app*, tm_widget, TmSlot fn, void* user);

TM_EXPORT tm_widget tm_add_frame(tm_app*, tm_widget c, const char* title);
TM_EXPORT void   tm_frame_set_title(tm_app*, tm_widget, const char* title);
TM_EXPORT tm_widget tm_add_scrollview(tm_app*, tm_widget c);
TM_EXPORT void   tm_scrollview_scroll_to(tm_app*, tm_widget, int x, int y);

/* ----- dialogs (async, callback-result) ----- */
TM_EXPORT void tm_dialog_info(tm_app*, tm_widget win, const char* message);
TM_EXPORT void tm_dialog_confirm(tm_app*, tm_widget win, const char* message, TmSlot fn, void* user); /* ev.i: 1=yes */

#ifdef __cplusplus
}
#endif
#endif /* TERMACS_C_TERMACS_H */
