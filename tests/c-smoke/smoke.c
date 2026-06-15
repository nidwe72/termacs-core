/* P2 exit: a plain-C program that drives the demo surface through the flat C ABI.
 * Isolates "C ABI correct?" from any binding (no Java/JNI here). */
#include "termacs/c/termacs.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int clicked = 0;
static void on_click(const TmEvent* ev, void* u)  { (void)ev; (void)u; clicked++; }
static int menu_fired = 0;
static void on_quit(const TmEvent* ev, void* u)   { (void)ev; (void)u; menu_fired = 1; }
static int confirmed = -1;
static void on_confirm(const TmEvent* ev, void* u){ (void)u; confirmed = ev->i; }

static int row_has(tm_app* a, const char* s) {
    char buf[256];
    int w = 0, h = 0;
    tm_app_screen_size(a, &w, &h);
    for (int y = 0; y < h; ++y) {
        tm_app_row_text(a, y, buf, sizeof buf);
        if (strstr(buf, s)) return 1;
    }
    return 0;
}

int main(void) {
    assert(tm_abi_version() == TM_ABI_VERSION);

    tm_app* a = tm_app_new_headless(64, 20);
    assert(a);
    tm_app_set_theme(a, TM_THEME_DARK);

    tm_widget win = tm_create_window(a, "termacs — C smoke");
    tm_window_resize(a, win, 64, 20);

    tm_widget mb = tm_window_set_menubar(a, win);
    tm_widget file = tm_menubar_add_menu(a, mb, "File");
    tm_menu_add_item(a, file, "Quit", on_quit, NULL);

    tm_widget body = tm_window_set_content_vbox(a, win);
    tm_container_set_padding(a, body, 1);
    tm_container_set_spacing(a, body, 1);

    tm_widget row = tm_add_hbox(a, body);
    tm_add_label(a, row, "Task:");
    tm_widget input = tm_add_lineedit(a, row);
    tm_widget_set_sizing(a, input, TM_AXIS_H, 0, 20, 0, 1);
    tm_widget add = tm_add_button(a, row, "Add");
    tm_on_clicked(a, add, on_click, NULL);

    tm_widget tasks = tm_add_listview(a, body);
    tm_widget_set_sizing(a, tasks, TM_AXIS_V, 3, 0, 0, 1);

    tm_widget status = tm_window_set_statusbar(a, win);
    tm_statusbar_set_text(a, status, "0 task(s)");

    tm_window_show(a, win);
    tm_widget_set_focus(a, input);

    /* type "milk" and read it back via copy-out */
    const char* word = "milk";
    for (const char* p = word; *p; ++p) tm_app_feed_key(a, TM_KEY_CHAR, (uint32_t)*p);
    char buf[64];
    size_t n = tm_lineedit_text(a, input, buf, sizeof buf);
    assert(n == 4 && strcmp(buf, "milk") == 0);

    /* focus the Add button + Enter -> clicked signal fires across the ABI */
    tm_widget_set_focus(a, add);
    tm_app_feed_key(a, TM_KEY_ENTER, 0);
    assert(clicked == 1);

    /* list operations */
    tm_listview_add_item(a, tasks, buf);
    tm_statusbar_set_text(a, status, "1 task(s)");
    assert(tm_listview_count(a, tasks) == 1);
    char it[64];
    tm_listview_item_at(a, tasks, 0, it, sizeof it);
    assert(strcmp(it, "milk") == 0);

    /* render + inspect cells */
    tm_app_render(a);
    assert(row_has(a, "Task:"));
    assert(row_has(a, "[ Add ]"));
    assert(row_has(a, "milk"));
    assert(row_has(a, "1 task(s)"));
    assert(row_has(a, "File"));

    /* File -> Quit via keyboard (variant TmEvent menu callback) */
    tm_app_feed_key(a, TM_KEY_F10, 0);
    tm_app_feed_key(a, TM_KEY_ENTER, 0);
    assert(menu_fired == 1);

    /* async confirm dialog */
    tm_dialog_confirm(a, win, "Remove \"milk\"?", on_confirm, NULL);
    tm_app_render(a);
    assert(row_has(a, "Remove \"milk\"?"));
    tm_app_feed_key(a, TM_KEY_ENTER, 0);   /* Yes */
    assert(confirmed == 1);

    /* error channel: a stale handle sets last_status, doesn't crash */
    tm_button_set_text(a, (tm_widget)0xDEADBEEFu, "x");
    assert(tm_last_status() == TM_ERR_INVALID_HANDLE);

    tm_app_free(a);
    printf("C smoke OK\n");
    return 0;
}
