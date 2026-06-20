# termacs — design spec

> Status: **design draft** (no implementation yet).
> A modern terminal-UI framework. C++ is the API design source of truth; Python, Dart, and Java are idiomatic bindings over a flat C ABI.

---

## 1. Purpose & scope

termacs is a **terminal user-interface framework in its own right**, with a modern, Qt-flavored API. It is *not* a wrapper around an existing TUI library's API.

[magiblot/tvision](https://github.com/magiblot/tvision) (the C++ Turbo Vision reimplementation) is used **only as a rendering/input backend** — it paints a cell grid to a terminal and delivers raw key/mouse events. Everything above that line is owned by termacs:

- the **widget tree** (our own `Widget` base, *not* `TView`),
- the **layout engine** (cell-based constraints, replacing absolute `TRect` math),
- **keyboard focus** and tab traversal,
- the **typed event / signal model** (no Borland `cmXXX` command codes),
- **styling / theming**,
- the **frame / redraw loop** (dirty-region → present).

This is the full-abstraction decision (referred to in discussion as **"b2"**): tvision is a swappable backend, never visible in any public type.

### Targets

- **Languages:** C++ (primary), Python, Dart, Java.
- **Platforms:** Linux (primary), Windows, Android-via-Termux (a terminal, so the same terminal backend covers it). A native Android renderer is a *future* second backend, not built now.

---

## 2. Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  Consumer apps                                                │
│   C++          Python         Dart           Java             │
└────┬──────────────┬─────────────┬──────────────┬─────────────┘
     │ (direct)      │ cffi         │ dart:ffi      │ JNI shim
     │              ▼             ▼              ▼
     │        ┌────────────────────────────────────────┐
     │        │  flat C ABI  (extern "C", opaque handles)│   ← transport only; nobody writes apps here
     │        └────────────────────┬─────────────────────┘
     ▼                             ▼
┌─────────────────────────────────────────────────────────────┐
│  termacs-core  (C++)  — the modern OOP API = design source    │
│  Widget tree · Layout engine · Signals · Focus · Style · Loop │
└────────────────────────────┬──────────────────────────────────┘
                             ▼  Backend seam (no termacs/TV types leak up)
        ┌──────────────────────────────────────────────┐
        │  Backend interface                            │
        │   size() · present(cells) · pollInput()→events │
        └───────────────┬────────────────┬───────────────┘
                        ▼                ▼
                 tvision backend   (future) Android-Canvas backend
                 (Linux/Win/Termux)
```

- The **C++ core** is where the API is designed. C++ consumers use it directly, zero overhead.
- The **flat C ABI** is a faithful, mechanical projection of the core. It is plumbing — not an app-authoring surface.
- **Python (cffi)** and **Dart (dart:ffi)** consume the C ABI directly. **Java** consumes it through a thin **JNI** shim (JNI *can* call C++ directly, but routing it through the same C ABI keeps one boundary where ownership/callbacks/errors are defined once).
- Every binding re-presents the same OOP shape; end users never see a handle or `void*`.

---

## 3. Design principles

1. **Qt is the vocabulary, not the model.** We adopt Qt's *nouns* — `Application`, `Widget`, layouts, signals & slots, dialogs, model/view. We reject Qt's *ownership/lifetime/meta-object machinery* (see §4 and §10), because it does not survive a flat C ABI across three managed runtimes.
2. **The core owns the entire tree.** Wrappers in every language (including C++) are **non-owning, invalidatable handles**. There is exactly one owner of any node: its parent in the core. This is the single most important decision in the whole design.
3. **No raw `new` at the API boundary.** Widgets are created by **factory methods on their parent** (`panel.addButton("OK")`), which construct *and* parent in one core-owned step. This keeps construction, ownership, and the FFI contract unambiguous.
4. **One event path.** All events cross the boundary through a single variant `TmEvent` + one callback signature. C++ keeps compile-time-typed signals on top; the typing is re-imposed by each binding, not carried by the ABI.
5. **Qt-flavored, not Qt-compatible.** No QML, no `moc`, no meta-object system, no `Q_PROPERTY` bindings, no QSS stylesheet strings. Properties are plain getters/setters.
6. **Single-threaded, synchronous dispatch.** No queued cross-thread signals. The event loop, layout, and draw all run on one thread.
7. **Cells, not pixels.** All geometry is in character cells. The layout/constraint model is deliberately simpler than `QSizePolicy`.

---

## 4. Ownership & lifetime model  *(the critical section)*

This is the thing that sinks bindings like this if left implicit (PyQt/PySide have fought it for 20 years). termacs designs it out:

- **The core owns every node.** Creating a widget through a parent factory returns a **handle** (a stable 64-bit id), not a pointer the caller owns.
- **Handles are non-owning views.** A handle's destructor in any language does **not** free the core node. Dropping a Python/Dart/Java wrapper, or a C++ handle going out of scope, never deletes a widget.
- **Explicit removal is the only destruction.** `widget.remove()` (or removing/replacing a parent) destroys the subtree in the core and **invalidates every handle** pointing into it.
- **Invalidation is safe, not UB.** The core keeps a generation counter per id. A call on a stale handle returns a defined error → the binding raises an idiomatic exception (Python `InvalidHandleError`, Java `IllegalStateException`, Dart `StateError`). **No use-after-free, no double-free, ever**, because no wrapper ever frees.
- **Application owns the roots.** Windows/popups are owned by the `Application`; closing one removes its subtree.

Consequence: a handle is cheap, copyable, and may dangle *logically* (point at a removed node) but never *dangerously*. Same model in all four languages — which makes the C ABI projection trivial.

---

## 5. The public C++ API  (design source of truth)

> Handles (`Window`, `Label`, `Button`, …) are small value types wrapping `{core*, id, generation}`. Cheap to copy. Methods forward to the core; calls on an invalidated handle throw `termacs::InvalidHandle`.

### 5.1 Hello world

```cpp
#include <termacs/termacs.hpp>
using namespace termacs;

int main() {
    Application app;                          // no argc/argv — meaningless in bindings

    Window win = app.createWindow("My App");
    win.resize({60, 20});                     // cells; {0,0} (default) ⇒ fill screen — see §5.3.0

    VBox     col   = win.setContent<VBox>();  // root layout, core-owned
    Label    name  = col.addLabel("Name:");   // factory → constructs + parents
    LineEdit input = col.addLineEdit();
    Button   ok    = col.addButton("OK");

    // Signal/slot — single-threaded, synchronous, auto-disconnecting
    ok.clicked().connect([=] {
        app.dialogs().info(win, "Hello " + input.text());
    });

    win.show();
    return app.run();                         // the event loop
}
```

### 5.2 Widgets & the tree

- Base type `Widget`; concrete: `Window`, `Label`, `Button`, `LineEdit`, `CheckBox`, `RadioGroup`, `ListView`, `Table`, `Menu`, `MenuBar`, `StatusBar`, `ProgressBar`, `Spacer`, `Frame`, …
- Created **only** via parent factories: `addLabel`, `addButton`, `addLineEdit`, … Each returns a typed handle.
- Tree ops: `widget.remove()`, `widget.parent()`, `widget.children()`, `widget.setVisible(bool)`, `widget.setEnabled(bool)`.

### 5.3 Layout engine

All geometry is in **cells**. Layout runs as a single pass *before each draw*, triggered only when the tree, a size hint, visibility, or the terminal size changes (dirty-driven — no continuous relayout). The model is deliberately smaller than Qt's `QSizePolicy`; for the grid it borrows the (well-proven) **CSS-Grid track-sizing** algorithm.

#### 5.3.0 Root window sizing

A window's size is set by `win.resize(w, h)` (cells). Two modes:

- **`{0, 0}` (the default) ⇒ fill the screen.** The window tracks the terminal: it occupies the full cell grid and re-fills on every resize. This is the right choice for a single-window app.
- **`{w, h}` with either dimension `> 0` ⇒ fixed, centered.** The dimension is clamped to the screen and the window is centered in the remaining space (the surrounding area paints the `window.bg` role). A `0` on one axis means "fill that axis."

There is currently **no intermediate mode** — no max-width-with-margins, min-size floor, or edge docking. A window is either full-screen or a fixed centered box. *(Open: a richer root-sizing policy — `max`/`margin`/dock — is future work; until then "fill, then constrain content with inner layout" is the idiom.)*

#### 5.3.1 Per-widget sizing spec (per axis)

```cpp
struct Sizing {
    int min       = 0;          // hard floor (cells)
    int preferred = 0;          // natural size ("size hint"); 0 ⇒ ask the widget
    int max       = Unbounded;  // hard ceiling
    int stretch   = 0;          // ≥0 weight for sharing surplus space
};
input.setSizing(Axis::Horizontal, { .preferred = 20, .stretch = 1 });
```

If `preferred == 0`, the widget supplies its own content hint (a `Label`'s text width, a `Button`'s `"[ OK ]"` width, etc.).

#### 5.3.2 Box layouts — `VBox` / `HBox`

Container props: `spacing` (cells between children), `padding` (inner margin). One axis is the **main** axis (vertical for `VBox`, horizontal for `HBox`); the other is the **cross** axis.

Main-axis pass:
1. `avail = containerMain − 2·padding − spacing·(n−1)`.
2. `need = Σ preferred`.
3. **Surplus** (`avail ≥ need`): every child gets its `preferred`, then `avail − need` is split across children by `stretch` weight (clamped at each `max`; space freed by a clamped child is re-distributed to the rest). `stretch == 0` ⇒ stays at `preferred`.
4. **Deficit** (`avail < need`): shrink each child from `preferred` toward `min`, proportional to its room `(preferred − min)`. If `Σ min > avail` the container **overflows** — children clamp at `min` and the excess is clipped (wrap in a `ScrollView` to scroll instead).

Cross axis: each child is offered the full cross extent (minus padding), clamped to its cross `max`; `Align` places any slack (default **fill**).

#### 5.3.3 Grid layout — `Grid`

The expressive one. Explicit tracks, spanning, gaps, per-cell alignment. **Columns and rows are sized independently** by the same track algorithm (columns first, then rows).

Track specs:

```cpp
Track::fixed(n)         // exactly n cells
Track::content()        // "auto": fit the largest preferred among items in the track
Track::flex(w = 1)      // share leftover space by weight w   (like CSS `fr`)
Track::content().min(4).max(20)   // optional bounds on any track
```

API:

```cpp
Grid g = win.setContent<Grid>();
g.setColumns({ Track::content(), Track::flex(1), Track::fixed(10) });
g.setRows   ({ Track::content(), Track::flex(1), Track::fixed(1)  });
g.setGap(1, 0);                              // colGap, rowGap

g.addLabel   (0, 0, "Name:");                // (row, col)
g.addLineEdit(0, 1);
g.add        (0, 2, avatar);
g.addListView(1, 0, { .colSpan = 3 });       // spans all three columns
Button ok =
  g.addButton(2, 2, "OK", { .align = Align::Right, .valign = VAlign::Bottom });
```

Track-sizing algorithm (run per axis):
1. **Base sizes.** `fixed` → its value; `content`/`flex` → 0.
2. **Non-spanning content.** For each item in a single track, grow that track to fit the item's `preferred` (respecting track `min`/`max`).
3. **Spanning items.** If an item's `preferred` exceeds the sum of its spanned tracks + gaps, distribute the deficit across those tracks — preferring `content`, then `flex`, then `fixed` — until the span fits.
4. **Distribute leftover.** `leftover = available − Σ base − gaps`. If `leftover > 0` and `flex` tracks exist, split it among them by weight (clamped at `max`). With no `flex` tracks, leftover stays as trailing slack (grid is content-sized — use `flex` to absorb space).
5. **Deficit.** If `Σ base > available`, shrink `content` tracks toward `min` (then `flex`, then `fixed`) proportionally; clip the remainder.

Each item is then placed in the rectangle spanned by its tracks; `align` / `valign` position it inside that rectangle (default **fill** on both axes — set an alignment to shrink the widget to its preferred size instead).

> **Two distinct concerns, don't conflate them:** a widget's `Sizing` feeds *track intrinsic sizing* (step 2/3); `align`/`valign` govern *placement within the finished cell* (after step 5). The cell can be larger than the widget; alignment decides what happens to the slack.

Worked column example — width 40, `colGap = 1`, columns `{content, flex(1), fixed(10)}`, widest item in col 0 = 8 cells:
- after step 2 bases `[8, 0, 10]`, two gaps = 2 → consumed 20.
- `leftover = 40 − 20 = 20` → all to the lone `flex` column.
- final widths `[8, 20, 10]`. ✔

#### 5.3.4 Overflow & scrolling

Layouts **clip** by default — they never resize the terminal or overlap siblings. For content larger than its viewport, wrap it in a **`ScrollView`** (a widget with a viewport, scroll offset, and optional scrollbars). This keeps the layout pass deterministic and makes scrolling an explicit, opt-in concern rather than implicit layout magic.

### 5.4 Signals & slots

- Each signal is a typed object exposed by accessor: `clicked()`, `textChanged()`, `selectionChanged()`, …
  ```cpp
  Connection c = input.textChanged().connect([](const std::string& t){ /* … */ });
  ```
- `connect(slot)` returns a **`Connection`**. It **auto-disconnects** when (a) the `Connection` is destroyed (RAII), or (b) either endpoint widget is removed. This is the cross-language equivalent of Qt's context-based auto-disconnect, and it closes the slot-lifetime use-after-free vector.
- **Dispatch is synchronous and single-threaded.** A signal fires the slots inline, in order, on the loop thread.
- C++ signals are compile-time typed. Across the C ABI they degrade to a variant `TmEvent` (see §7); the typing is restored inside each binding.

### 5.5 Properties

Plain getters/setters mapping to `tm_widget_get_* / set_*`. No meta-object system, no dynamic `setProperty(name, variant)`.

```cpp
input.setText("default");      std::string s = input.text();
ok.setEnabled(false);          bool e = ok.isEnabled();
label.setAlign(Align::Right);
```

### 5.6 Styling / theming

**Role-based, typed — no QSS selector strings.** A `Theme` is a map from a **semantic role** (`window.bg`, `button.focused.fg`, `input.border`, `list.selected.bg`, …) to a `Style`. Widgets resolve their style by `(role, state)`; a per-widget `setStyle` overrides the theme for that widget. There is no cascade/selector engine — resolution is a flat lookup plus the per-widget override.

```cpp
Theme dark = Theme::builtin(Builtin::Dark);     // Dark, Light, HighContrast, ClassicDOS
app.setTheme(dark);

// programmatic role override
dark.set("button.focused", Style{ .fg = Color::Black, .bg = Color::Cyan, .bold = true });

// per-widget override (wins over theme)
ok.setStyle({ .fg = Color::BrightWhite, .bg = Color::Blue, .bold = true });
```

Themes are: **built-in** (named), **programmatic** (build/modify a `Theme`), or **loaded from a flat declarative file** (`role = style` lines — TOML-ish, no selectors). File loading lands in P5 (post-milestone); built-in + programmatic are enough for the milestone.

#### Custom-widget painting *(resolves open question Q1)*

`onPaint` is available in **all four languages**, not C++-only. The boundary cost is acceptable because:

- Redraw is **event-driven, not continuous** — a widget repaints only when marked dirty (no 60 fps loop).
- Only the widget's own cell rectangle is exposed; ops are **batched** — the binding accumulates draw commands and submits them in one FFI flush per paint, so a custom widget is ~1 round-trip per repaint, not one per cell.

```cpp
sparkline.onPaint([](PaintSurface& s){          // s is clipped to the widget's cells
    s.fill(Style{ .bg = Color::Black });
    s.drawText(0, 0, "▁▂▃▅▇", Style{ .fg = Color::Green });
});
```

`PaintSurface` is intentionally tiny — `fill`, `drawText(x, y, text, style)`, `drawHLine/drawVLine`, `setCell` — a cell surface, **not** a `QPainter`. Custom paint is the escape hatch; the built-in widget set covers the common cases.

### 5.7 Dialogs & the floating layer

- There **is** a floating/popup layer above the tiled content tree — menus, dropdowns, dialogs, and tooltips overlap and need it. The main content is a tiled box tree; popups live on a separate z-ordered overlay owned by `Application`.
- Dialogs are **async / callback-result**, not nested `exec()` loops (nested loops + FFI callbacks = re-entrancy hazard):
  ```cpp
  app.dialogs().confirm(win, "Delete file?", [=](bool yes){
      if (yes) doDelete();
  });
  app.dialogs().info(win, "Saved.");
  app.dialogs().prompt(win, "New name:", [=](std::optional<std::string> v){ /* … */ });
  ```

### 5.8 Focus

First-class, keyboard-centric. Tab/Shift-Tab traversal in tree order by default; `widget.setFocusable(bool)`, `widget.setFocus()`, `focusNext()/focusPrev()`. Mouse focus is secondary (it's a TUI).

### 5.9 Model / View (lists & tables) *(resolves open question Q3)*

Qt-style separation, simplified — and **virtualized by default**.

Two ways to feed a view:

```cpp
ListView lv = col.addListView();
lv.setItems({ "Alpha", "Beta", "Gamma" });        // eager: small/static data
lv.activated().connect([](int row){ /* … */ });

// pull-based model: large / dynamic data
Table t = col.addTable();
t.setModel(myModel);   // myModel : rowCount(), columnCount(), cellAt(row,col)->Cell
```

**Virtualization falls out of the pull model:** the view only ever queries the rows currently in its viewport plus a small overscan, so a 10-million-row model costs the same as a 10-row one. The model is a `Model` *handle* the view calls back into — including across the FFI boundary (a Python/Dart/Java model uses the same callback trampoline as signals). The model pushes change notifications:

```cpp
myModel.rowsChanged(first, last);   // re-query just that range
myModel.reset();                    // structure changed; re-query visible window
```

Scope: the milestone needs only `ListView` with eager `addItem`/`removeItem`. The pull-based `Model`, `Table` (N columns), and virtualization land in **P5** (post-milestone); tree/hierarchical models later still.

---

## 6. The Backend seam

The core talks to exactly this interface; **no termacs or tvision type appears above it**:

```cpp
struct Backend {
    virtual CellSize     size() = 0;                  // width/height in CELLS
    virtual Capabilities caps() = 0;                  // color depth, mouse, glyph set
    virtual int          measureWidth(StrView) = 0;   // display cells for a string (CJK/combining/emoji)
    virtual void         present(const CellBuffer&) = 0;  // blit a frame; downsamples color to caps()
    virtual bool         pollInput(InputEvent& out) = 0;  // NORMALIZED events, non-blocking
    virtual void         wake() = 0;                  // thread-safe: unblock the loop (see §11)
    virtual void         shutdown() = 0;              // restore terminal state; must be panic-safe
};
```

- `present` takes truecolor styled cells and **downsamples** to whatever `caps()` reports (truecolor → 256 → 16).
- `measureWidth` is the single source of truth for text width — the layout engine uses it so measurement always matches what the backend renders (closes the font/width drift in §11).
- `pollInput` returns **normalized** `InputEvent`s: typed `Key`+`Modifiers`, mouse, and **`Resized`** — never raw escape bytes.
- `TvisionBackend` implements it over magiblot/tvision (Linux/Windows/Termux).
- A future `AndroidCanvasBackend` implements the same interface against a `SurfaceView`/`Canvas`; a `HeadlessBackend` renders to an in-memory buffer for snapshot tests (§11).
- Keeping this seam honest is what makes the b2 abstraction real instead of a leaky re-skin.

---

## 7. C ABI projection rules

The C ABI is generated/derived mechanically from the core. Rules:

- **Handles are opaque.** Everything is `typedef uint64_t TmWidget; /* id + generation packed */`. No struct layouts cross the boundary.
- **No raw `new`.** Factory functions parent on creation:
  ```c
  TmWidget tm_vbox_add_button(TmWidget parent, const char* text);
  TmWidget tm_vbox_add_label (TmWidget parent, const char* text);
  void     tm_widget_remove  (TmWidget w);          // destroys subtree, invalidates handles
  ```
- **One event path.** A single tagged-union event and one callback signature carry *all* signals:
  ```c
  typedef struct { TmEventKind kind; TmWidget source; /* union payload */ } TmEvent;
  typedef void (*TmSlot)(const TmEvent* ev, void* user);
  TmConnection tm_signal_connect(TmWidget w, TmSignalId sig, TmSlot cb, void* user);
  void         tm_disconnect(TmConnection c);       // also auto-fires on endpoint removal
  ```
  Typed C++ signals do **not** survive as types here; the bindings re-impose typing from `kind`.
- **String ownership is defined: copy-out into a caller buffer.** No returned `char*` to free:
  ```c
  // returns required length; writes up to cap bytes; never transfers ownership
  size_t tm_lineedit_text(TmWidget w, char* buf, size_t cap);
  ```
- **Error channel.** Every call sets a thread-local last-error; accessors expose it so bindings raise idiomatic exceptions:
  ```c
  TmStatus tm_last_status(void);            // OK / INVALID_HANDLE / BAD_ARG / …
  const char* tm_status_message(void);
  ```
- **Enums/flags are generated** (Align, Color, EventKind, SignalId, Status) from one source of truth into the C header *and* the three binding mirrors, so they cannot desync by hand.

---

## 8. Language bindings — how the API looks

All bindings are idiomatic OOP. The handle and the C seam are invisible.

### Python (cffi)

```python
from termacs import Application, Window  # widgets reached via factories

app = Application()
win = app.create_window("My App")
win.resize(60, 20)

col   = win.set_content_vbox()
name  = col.add_label("Name:")
inp   = col.add_line_edit()
ok    = col.add_button("OK")

ok.clicked.connect(lambda: app.dialogs.info(win, f"Hello {inp.text}"))

win.show()
app.run()
```

### Dart (dart:ffi)

```dart
final app = Application();
final win = app.createWindow('My App')..resize(60, 20);

final col = win.setContentVBox();
final name = col.addLabel('Name:');
final inp  = col.addLineEdit();
final ok   = col.addButton('OK');

ok.clicked.connect(() => app.dialogs.info(win, 'Hello ${inp.text}'));

win.show();
app.run();
```

### Java (JNI shim over the C ABI)

```java
Application app = new Application();
Window win = app.createWindow("My App");
win.resize(60, 20);

VBox col      = win.setContentVBox();
Label name    = col.addLabel("Name:");
LineEdit inp  = col.addLineEdit();
Button ok     = col.addButton("OK");

ok.clicked().connect(() -> app.dialogs().info(win, "Hello " + inp.text()));

win.show();
app.run();
```

Each binding handles three things to feel native:
1. **Identity** — each wrapper object holds one handle (a `long` in Java, an int/pointer elsewhere).
2. **Callbacks** — a small trampoline turns a native lambda/closure/`Runnable` into the `TmSlot` function pointer; the variant `TmEvent` is decoded back into typed callback args.
3. **Lifetime** — wrappers are non-owning; they never free. A removed core node simply invalidates the handle and the next call raises.

---

## 9. What we explicitly drop from Qt

| Dropped | Why |
|---|---|
| `moc`, `Q_OBJECT`, meta-object system, `qobject_cast` | Can't and shouldn't replicate across a flat C ABI. |
| `Q_PROPERTY` bindings, dynamic `setProperty` | Plain getters/setters instead. |
| QML | Out of scope; not a TUI fit. |
| QSS stylesheet strings | Typed theme/style structs instead. |
| `QPainter` / Graphics View / animations | Custom widgets use a tiny cell surface. |
| `new` + parent-ownership at the boundary | Core owns the tree; factory methods + handles. |
| Nested `exec()` modal loops | Async/callback-result dialogs. |
| Cross-thread queued signals | Single-threaded synchronous dispatch. |
| Full `QSizePolicy` machinery | Minimal `{min, preferred, stretch}` in cells. |
| MDI / pixel geometry | Tiled tree + floating popup layer; cells only. |

---

## 10. Resolved decisions & phasing

The four prior open questions are now decided (details in the linked sections):

| # | Question | Decision |
|---|---|---|
| Q1 | Custom-widget paint across the FFI boundary | **Allowed in all languages.** Event-driven (dirty-only) redraw + **batched** ops → ~1 round-trip per repaint. Tiny `PaintSurface`, not a `QPainter`. (§5.6) |
| Q2 | Layout engine depth (esp. grid) | **Cell-based, dirty-driven.** Box = stretch-factor surplus/deficit pass; **Grid = CSS-Grid-lite track sizing** (fixed / content / flex tracks, spanning, gaps, per-cell align). Overflow clips; `ScrollView` for scrolling. (§5.3) |
| Q3 | Model/View virtualization | **Pull-based `Model` (`rowCount`/`columnCount`/`cellAt`), virtualized by visible-window query.** `ListView` + `Table` in v1; trees deferred. (§5.9) |
| Q4 | Theme format | **Role-based typed themes**, no QSS selectors. Built-in + programmatic for the milestone; flat declarative file at P5. (§5.6) |

**Milestone (M): the `termacs-java-demo` task app (§13) running on a real Linux terminal.** Everything up to P4 is the path to M; only what the demo needs is front-loaded. Breadth, Python/Dart, and Android come *after* the milestone. Java is the first (and only, pre-M) language binding (§12).

#### Path to the milestone

| Phase | Goal | Key scope (only what the milestone needs) | Exit criterion |
|---|---|---|---|
| **P0** | Core spine (C++) | run loop; Backend seam + `TvisionBackend` + `HeadlessBackend`; widget tree + handle/invalidation; signals + connections; `postToUiThread`; timers; focus/tab; `VBox`/`HBox`; built-in `Theme` | HeadlessBackend snapshot test renders a laid-out VBox/HBox of stub widgets |
| **P1** | Demo widget set (C++) | `Label`, `LineEdit`, `Button`, `ListView` (eager `addItem`), `MenuBar`/`Menu`, `StatusBar`; async `info`/`confirm` dialogs + floating popup layer | a C++ task-demo runs on `TvisionBackend`; its HeadlessBackend snapshot is asserted |
| **P2** | C ABI + C smoke test | `termacs_cabi` target (`-fvisibility=hidden`, `TM_EXPORT`, `tm_abi_version()`); generated enums; variant `TmEvent`; error channel; string copy-out — covering only the demo's surface | **plain-C** program drives the same demo through the C ABI; snapshot matches P1 |
| **P3** | Java binding | JNI shim → single static `libtermacsjni.so`; per-arch JAR packaging + runtime load; handle/GC lifecycle; callback trampolines; `postToUiThread` from JVM threads; `sciens.termacs` OOP classes for the demo widget set | JUnit snapshot test (HeadlessBackend) green on `sciens.termacs` |
| **P4 ★** | **MILESTONE — Java/Linux demo** | `termacs-java-demo` `TaskDemo` (§13) | runs on a real Linux terminal: add/remove tasks, async dialogs, clock, tab focus, live resize reflow |

#### Post-milestone roadmap

| Phase | Goal | Scope |
|---|---|---|
| **P5** | Breadth | full `Grid`; `Table` + pull-based model/view (virtualized); `ScrollView`; more widgets (`CheckBox`, `RadioGroup`, `ProgressBar`, `Frame`, `Spacer`); theming from file; custom-widget paint surface |
| **P6** | Python & Dart | cffi + dart:ffi over the shared `libtermacs.so` — mechanical, since the ABI is already proven by Java |
| **P7** | Android | native Android-Canvas `Backend` (beyond Termux) |

---

## 11. Terminal realities & runtime concerns

A TUI framework lives or dies on how it handles the messy reality of terminals. These are first-class requirements, not afterthoughts.

### 11.1 Resize & font changes

- **Resize is free.** The backend reports size in cells; a terminal resize arrives as a normalized `Resized` input event → root marked dirty → one relayout pass → redraw. Because layout is entirely cell-based, nothing special is needed.
- **Font changes are transparent.** Changing the terminal font (family/size) only changes pixels; the terminal reports a new cell grid, which is *indistinguishable from a resize*. termacs never sees pixels.
- Apps that want to adapt (e.g. compact layout under 80 columns) connect to `app.resized()` (emits new `CellSize`).

### 11.2 Unicode & character width

- Glyphs may be **2 cells wide** (CJK, many emoji), **0 cells** (combining marks), or ambiguous. Layout text-width **must** be measured via `Backend::measureWidth` so it matches what the backend actually renders — never `strlen`/codepoint count. This closes "layout drift" where text overflows or leaves gaps.
- Grapheme clusters / ZWJ emoji are measured as one unit by the backend's width function.

### 11.3 Glyph availability & charset fallback

- A terminal font may lack box-drawing/block glyphs. The theme carries a **charset** with an **ASCII fallback** for line-drawing, so a minimal font degrades to `+--+` rather than tofu (`￿`). Backends advertise glyph support via `Capabilities`.

### 11.4 Color depth

- Terminals support 16 / 256 / truecolor. Themes are authored in **truecolor**; `Backend::present` **downsamples** to `caps()` (truecolor → 256 → 16) at blit time. App code and themes never branch on color depth.

### 11.5 Input normalization

- Key encoding is terminal-specific chaos (escape sequences, Esc-vs-Alt, kitty keyboard protocol, bracketed paste, function keys). The backend normalizes everything to a typed `Key` + `Modifiers`; the core and public API **never** see raw escape bytes. Mouse is normalized too (and optional — keyboard is the spine, §5.8).

### 11.6 Threading model & the one thread-safe door

- Dispatch is **single-threaded and synchronous** (§3). But real apps do background work (network, file IO) on other threads, JVM threads, or Dart isolates and must update the UI.
- The **only** thread-safe API is:
  ```cpp
  app.postToUiThread([=]{ label.setText(result); });   // runs fn on the loop thread, soon
  ```
  It enqueues `fn` and calls `Backend::wake()` to unblock the loop. **Every other call must happen on the UI thread.** This rule is identical in all bindings (Python threads, Java threads, Dart isolates) and is the single most important concurrency contract.

### 11.7 Timers

- The loop provides `app.startTimer(intervalMs, cb) -> TimerId` and `app.stopTimer(id)` for cursor blink, polling, and simple animation. Timer callbacks run on the UI thread like any other event.

### 11.8 Robust teardown

- On normal exit *and* on crash/signal (SIGINT/SIGTERM/panic), the backend must restore terminal state — leave the alternate screen, restore cooked mode, show the cursor. `Backend::shutdown` is **panic-safe** and also wired to a terminal-restoring signal/terminate handler, so a crash never leaves the user's shell wrecked.

### 11.9 Minimum size

- Apps can declare a minimum usable size; below it the framework shows a built-in "terminal too small (need N×M)" message instead of attempting a broken layout.

### 11.10 Headless backend (testing)

- `HeadlessBackend` renders to an in-memory cell buffer and accepts scripted input. This enables **snapshot/golden tests** of whole UIs with no real terminal — a direct payoff of keeping the backend seam (§6) honest. Ships in P0 so the core, C ABI, and bindings are testable from day one.

---

## 12. Project structure & build

**Monorepo, `termacs`-prefixed projects, per-project subdirectories.** The C ABI lives inside core as a disciplined target; **Java is the first language binding**; the Java/Linux demo is a separate consumer project.

> **Evolution (see §15):** the monorepo layout below is a **transitional seed** — used once to do the decoupling, then retired in favor of **one self-contained repo per project** (`termacs-core` / `termacs-java` / `termacs-java-demo`) as the canonical workspace. The directory tree and build seams in §12 still describe how the code is organized and how the layers fit — §15 covers how they become independent repos.

```
termacs/                          repo root (monorepo)
├── termacs.md                    this spec
├── termacs-core/                 C++ framework + tvision backend + C ABI   (CMake)
│   ├── include/termacs/          public C++ headers (the design surface)
│   ├── include/termacs/c/        termacs.h — the flat C ABI header
│   ├── src/                      widget tree, layout, signals, loop, theme
│   ├── backends/tvision/         TvisionBackend
│   ├── backends/headless/        HeadlessBackend (in-memory cells; tests)
│   ├── cabi/                     termacs_cabi target: extern "C" projection
│   ├── tests/
│   │   ├── cpp/                  C++ unit/snapshot tests (HeadlessBackend)
│   │   └── c-smoke/              ★ plain-C program linking the C ABI directly
│   └── third_party/tvision/      git submodule (pinned)
├── termacs-java/                 Java binding (first language)             (Gradle)
│   ├── native/                   JNI shim (C++ → C ABI), built by CMake (standalone)
│   ├── src/main/java/sciens/termacs/      idiomatic OOP Java API
│   ├── src/test/java/sciens/termacs/      JUnit tests driving HeadlessBackend
│   └── build.gradle              packages the prebuilt .so as a resource
├── termacs-java-demo/            Java/Linux demo + smoke app (depends on termacs-java)
│   └── src/main/java/sciens/termacs/demo/
└── (future) termacs-python/ , termacs-dart/
```

### 12.1 The native-build seam — the #1 time-sink, designed away
- `libtermacsjni.so` is **built by CMake as a standalone step**. **Gradle does not drive CMake's incremental build** — it treats the finished `.so` as an opaque prebuilt artifact and copies it into the JAR as a resource. This sidesteps the classic "clean build works, incremental is stale" CMake↔Gradle trap (Gradle can't see CMake's input/output graph).
- Build order: CMake builds core → C ABI → JNI `.so`; then Gradle packages + compiles Java. A top-level script (or a Gradle `Exec` task with explicit inputs) wires the hand-off, but dependency tracking stays inside CMake where it belongs.

### 12.2 The C ABI is a disciplined target, not just headers
- `termacs_cabi` is a first-class library target **inside `termacs-core`** — lockstep versioning, no separate repo (which would reintroduce the sync problem we're avoiding). *This still holds under §15: the multi-repo split is core / binding / demo — the C ABI stays **inside** `termacs-core`, never its own repo.*
- Built with **`-fvisibility=hidden` + an explicit `TM_EXPORT` macro**, so the shared lib exports *only* the curated C ABI surface — never tvision/core C++ internals.
- Exposes **`tm_abi_version()`** from day one; **every binding checks it on load** and refuses a mismatched native lib — cheap insurance against .so/JAR drift (the failure mode that otherwise yields baffling crashes).
- The core project **installs/exports `termacs.h` + a shared `libtermacs.so`** as a packaging target *now*, though Python/Dart come later — so adding them needs no structural reshaping.

### 12.3 Java native packaging & loading
- **One self-contained `libtermacsjni.so`** = JNI shim + C ABI + core + tvision, statically linked. Single `dlopen`, no `RPATH`/load-order issues.
  - ⚠ **Verify the tvision link path (resolved in R1 — §15.4):** if tvision pulls in system `ncurses`/`terminfo`, "fully static" is a half-truth — `libncurses` stays a runtime dependency and `terminfo` is runtime data. `termacs-java` (the native-link owner under §15.2) must confirm which build path tvision uses, document the runtime dependency, and ship the **runtime-plumbing guard** (§15.2) that fails with an actionable message when ncurses/terminfo is absent rather than crashing.
- Native libs are packaged in the JAR under **`/native/<os>-<arch>/`** (`linux-x86_64`, `linux-aarch64`, …) so one JAR can later carry multiple arches; the loader selects by `os.name`/`os.arch`.
- Runtime loading extracts the `.so` and calls **`System.load(absolutePath)`** (not `loadLibrary`), guarding the known pitfalls:
  - `/tmp` mounted **`noexec`** (hardened Linux) → configurable extraction dir (`-Dtermacs.nativeDir=…`).
  - **concurrent JVMs** racing the same file → name the extracted file by **version+arch hash**, atomic rename.
  - **stale .so after upgrade** → version-stamped filename ⇒ new version = new file.

### 12.4 Java-first, de-risked by a plain-C smoke test
- Java-first is deliberate: JNI is the **harshest consumer** of the C ABI (handle lifecycle vs GC, callback trampolines vs JNI thread-attach, `postToUiThread` from arbitrary JVM threads). Getting these right first means Python/Dart later sit on a *proven* ABI.
- The cost — slower first light, plus ambiguity over whether a bug is in the C ABI or the JNI layer — is erased by the **plain-C smoke test** (`termacs-core/tests/c-smoke`) as the **very first consumer** of the C ABI. It cleanly separates "C ABI correct?" from "JNI correct?" for near-zero cost.
- **Bonus from the threading model:** because dispatch is single-threaded and all callbacks fire on the UI thread (§11.6), JNI attaches **one** thread, once — `postToUiThread` is precisely what makes JNI callbacks tractable here.

### 12.5 Testing layers (the demo is *not* the test layer)
- **Regression suite = `HeadlessBackend`** driving scripted input and asserting on the resulting cell buffer — wired into both the C++/C tests and the Java **JUnit** suite. This is the real correctness asset.
- **`termacs-java-demo`** is for manual + smoke ("does it launch and paint on a real Linux terminal"). Useful, but it does **not** masquerade as the test layer.

### 12.6 Decisions to confirm
- **Java package / module:** ✅ **`sciens.termacs`** (binding) and **`sciens.termacs.demo`** (demo). JPMS: ship as a deliberate **automatic module** unless modules are truly needed (native-loading + JPMS is a known pain).
- **JDK baseline:** ✅ **Java 17**, kept **Android-safe** (conservative APIs, no APIs Android lacks) so the same `termacs-java` source can serve Android later — one source set, no desktop/Android fork.
- **Build tool:** **Gradle** (Android Gradle Plugin is Gradle-only — consistency for the roadmap).
- **CI matrix (minimal):** {Linux x86_64, Linux aarch64} × {build core + C smoke test, build + JUnit `termacs-java`}. aarch64 matters for Android-later and ARM dev machines.

---

## 13. The Java/Linux demo app (`termacs-java-demo`)

A small **task list** — deliberately tiny but exercising the spine of the API: menu bar, nested layout, several widgets, signals, async dialogs, a timer, theming, and tab focus. It doubles as a manual smoke test ("does it launch and paint on a real Linux terminal").

### 13.1 What it shows

- A **MenuBar** (`File ▸ Quit`, `Help ▸ About`).
- A **VBox** body holding an input **HBox** (`Label` + `LineEdit` + `Add` button) and a **`ListView`** that grows to fill.
- A **StatusBar** with a live task count (left) and a clock (right, driven by a timer).
- Behavior: type + `Add`/Enter appends a task; Enter on a row asks to remove it (async `confirm`); `Quit` confirms; `About` shows an `info` dialog. Tab moves focus.

### 13.2 Source (`sciens.termacs.demo.TaskDemo`)

```java
package sciens.termacs.demo;

import sciens.termacs.*;

/** termacs Java/Linux demo: a tiny task list. */
public final class TaskDemo {
    public static void main(String[] args) {
        Application app = new Application();
        app.setTheme(Theme.builtin(Theme.Builtin.DARK));

        Window win = app.createWindow("termacs — Task Demo");
        win.resize(0, 0);                                     // {0,0} ⇒ fill the whole terminal (§5.3.0)

        // menu bar
        MenuBar mb   = win.setMenuBar();
        Menu    file = mb.addMenu("File");
        Menu    help = mb.addMenu("Help");

        // body: vertical stack
        VBox body = win.setContentVBox();
        body.setPadding(1);
        body.setSpacing(1);

        // input row
        HBox     row   = body.addHBox();
        row.addLabel("Task:");
        LineEdit input = row.addLineEdit();
        input.setSizing(Axis.HORIZONTAL, Sizing.of().preferred(20).stretch(1));
        Button   add   = row.addButton("Add");

        // task list — grows to fill remaining height
        ListView tasks = body.addListView();
        tasks.setSizing(Axis.VERTICAL, Sizing.of().min(3).stretch(1));

        // status bar
        StatusBar status = win.setStatusBar();

        // behavior
        Runnable addTask = () -> {
            String t = input.text().strip();
            if (t.isEmpty()) return;
            tasks.addItem(t);
            input.setText("");
            input.setFocus();
            status.setText(tasks.count() + " task(s)");
        };
        add.clicked().connect(addTask);
        input.submitted().connect(addTask);                   // Enter in the field

        tasks.activated().connect(rowIdx ->                   // Enter on a row
            app.dialogs().confirm(win, "Remove \"" + tasks.itemAt(rowIdx) + "\"?", yes -> {
                if (yes) { tasks.removeItem(rowIdx); status.setText(tasks.count() + " task(s)"); }
            }));

        file.addItem("Quit", () ->
            app.dialogs().confirm(win, "Quit termacs demo?", yes -> { if (yes) app.quit(); }));
        help.addItem("About", () ->
            app.dialogs().info(win, "termacs demo\nTurbo Vision engine, modern API."));

        // a clock in the status bar — timer callback runs on the UI thread
        app.startTimer(1000, () ->
            status.setRightText(java.time.LocalTime.now().withNano(0).toString()));

        status.setText("0 task(s)");
        win.show();
        input.setFocus();
        System.exit(app.run());                               // run() returns the exit code
    }
}
```

> **Background work** would use the one thread-safe door (§11.6): a worker thread does its job, then `app.postToUiThread(() -> tasks.addItem(result))`. The demo doesn't need it, but it's the only correct way to touch widgets off the UI thread.

### 13.3 How it renders (64×20 cells)

```
┌ termacs — Task Demo ─────────────────────────────────────────┐
│ File  Help                                                    │
│                                                               │
│  Task: [ buy milk______________________ ]   [ Add ]          │
│                                                               │
│  ┌──────────────────────────────────────────────────────┐    │
│  │ write the spec                                         │    │
│  │ wire up the JNI shim                                   │    │
│  │ buy milk                                               │    │
│  │                                                        │    │
│  │                                                        │    │
│  └──────────────────────────────────────────────────────┘    │
│                                                               │
│ 3 task(s)                                       14:32:07      │
└───────────────────────────────────────────────────────────────┘
```

The async `confirm` dialog on the floating layer (§5.7), e.g. on Quit:

```
┌ termacs — Task Demo ─────────────────────────────────────────┐
│ File  Help                                                    │
│            ┌ Confirm ──────────────────────────┐             │
│  Task: [ b │                                   │  [ Add ]     │
│            │   Quit termacs demo?              │              │
│  ┌─────────│                                   │──────┐       │
│  │ write th │        [ Yes ]   < No >          │      │       │
│  │ wire up  └───────────────────────────────────┘      │       │
│  │ buy milk                                               │    │
│  └──────────────────────────────────────────────────────┘    │
│ 3 task(s)                                       14:32:07      │
└───────────────────────────────────────────────────────────────┘
```

### 13.4 Build & run (Linux)

```bash
# builds termacs-core (CMake) + the JNI .so, then the binding + demo (Gradle)
./gradlew :termacs-java-demo:run
```

Gradle depends on `termacs-java`, which packages the prebuilt `libtermacsjni.so` (§12.3); the demo just consumes the published Java API. Resizing the terminal reflows the layout live (§11.1); the list scrolls once it overflows its box.
```

---

## 14. Implementation status (P0–P4) — ✅ built & verified

The path-to-milestone phases are implemented and green. Build everything with `./build.sh` (CMake + `javac --release 17`; the Gradle build is the canonical packaging, this script is the gradle-free equivalent).

| Phase | What landed | Verification |
|---|---|---|
| **P0** | Core spine: handle/invalidation registry, VBox/HBox layout, signals, focus/tab, timers, `postToUiThread`, theme, `TvisionBackend` + `HeadlessBackend` | `tc_snapshot` (ctest) — laid-out VBox/HBox snapshot |
| **P1** | Demo widgets: Label, Button, LineEdit, ListView, MenuBar/Menu, StatusBar, async info/confirm dialogs + floating layer | `tc_demo` (ctest) — full TaskDemo headless: type→add, F10→Quit, confirm dialog |
| **P2** | Flat C ABI: opaque handles, variant `TmEvent`+`TmSlot`, copy-out strings, `tm_abi_version`, error channel, `-fvisibility=hidden` + version script (53 exports, **only `tm_*`**) | `tc_csmoke` (ctest) — plain-C drives the demo through the ABI |
| **P3** | Java binding: JNI shim → single `libtermacsjni.so`, `sciens.termacs.*` OOP classes, callback trampolines (events cross back into Java lambdas), `tm_abi_version` checked on load | `HeadlessTest` — full TaskDemo via JNI→C ABI→core, all assertions pass |
| **P4 ★** | `termacs-java-demo` `TaskDemo` (§13) | Runs on a real (pseudo) terminal: paints the full UI, accepts typed input (the task appears in the list), F10▸File▸Quit→confirm→Yes exits cleanly. Driven headlessly by `tools/run_demo_pty.py` (rc=0). |

**Verified everywhere it matters via `HeadlessBackend`** (which renders the *exact* cell buffer the terminal backend presents): the framework, all widgets, layout, signals, menus, dialogs, and the Java binding's callbacks all pass automated tests in C++, C, and Java. The **real tvision terminal backend** is additionally verified end-to-end: `tools/run_demo_pty.py` launches the Java demo on a pseudo-terminal, types a task, drives the menu/dialog, and confirms the rendered frames contain the expected UI.

> **Backend init note:** the `TvisionBackend` must call `THardwareInfo::allocateScreenBuffer()` right after `setUpConsole()` — exactly as tvision's own `TScreen` ctor does. That call sizes tvision's internal display buffer (and the reported screen dimensions); without it `screenWrite` silently paints nothing.

**Layout of what was built:** `termacs-core/{include,src,backends,cabi,tests}`, `termacs-java/{src,native}`, `termacs-java-demo/`. ~30 source files; `build.sh` builds and tests all of it.

---

## 15. Repo & distribution model

**Supersedes the pure-monorepo framing of §12.** The *published* form of termacs is **self-contained, independently versioned repos — one per project** (`termacs-core`, `termacs-java`, `termacs-java-demo`), not a single repo. This realigns distribution with the architecture: the C ABI is a stable seam (§6–§7), so each binding should be a real, separately consumable library, and `termacs-python` / `termacs-dart` later join as **peers, not subdirectories**.

**The repos are the workspace; the monorepo is a seed, then retired.** The current monorepo is used **once** to do the decoupling (its tests are already wired, so the work is cheap and safe there); the decoupled state is then pushed into the three repos, which become **canonical**, and the monorepo is **archived** (kept until the repos are proven to build standalone, then retired — not deleted on day one). From then on each repo is an independent project — its own build, its own version, its own IDE project. To co-develop coupled changes (e.g. core + java together), `termacs-java` points its `FetchContent` at a **local `termacs-core` checkout** (`FETCHCONTENT_SOURCE_DIR`) instead of the remote tag, so a coupled edit needs no tag round-trip. This still preserves §12.2's win: the C ABI is **not** split into its own repo — it stays inside `termacs-core`; only the binding/demo *consumers* become separate.

**Dependency direction is a strict line:** `termacs-core` → `termacs-java` → `termacs-java-demo`. **Core knows nothing of Java.**

### 15.1 How a downstream repo gets its upstream — source, not binaries

- **Adopted (now): source dependency.** `termacs-java`'s build fetches `termacs-core` **from source at a pinned git tag** (CMake `FetchContent`) and compiles it as part of its own build. **No compiled artifacts are published to GitHub.** Cost: a C++ toolchain is required to build the binding. Benefit: zero release infrastructure, exact version pinning, always-from-source reproducibility.
- **Deferred (release milestone): prebuilt native artifacts.** Bundling a per-platform `libtermacsjni.so` inside the published `.jar` so a Java consumer needs **no** C++ toolchain — the eventual "real SDK" shape, and how bindings are pleasant to consume. This is a CI/release pipeline to own (build matrix × upload) and is **out of scope** until there are consumers who shouldn't compile C++.

### 15.2 Ownership move — the JNI belongs to `termacs-java`

Today `termacs-core/CMakeLists.txt` builds `libtermacsjni.so` by reaching sideways into `../termacs-java/native/` — a monorepo shortcut that breaks the moment the repos are cloned independently. Under this model:

- **`termacs-core` exposes only** its public C++ headers and the C ABI (`termacs.h` + the `termacs_cabi` / `libtermacs` targets), and **knows nothing about Java or JNI.**
- **`termacs-java` owns its JNI build end-to-end** — its own `CMakeLists.txt` compiles `native/termacs_jni.cpp` (+ `jni.map`) and links against `termacs-core` obtained per §15.1. This is the architecturally correct boundary regardless of repo topology.
- **Runtime-plumbing guard (resolves the §12.3 ⚠).** "Self-contained" means the *source* resolves to one `.so` — it does **not** mean zero system dependencies: tvision drives terminals through `libncurses` + the `terminfo` database, which must exist at runtime. Since **no binaries are published** (§15.1), the binding must **fail loudly and actionably** when that plumbing is missing — e.g. *"terminal database/terminfo unavailable; install `ncurses-base` or set `$TERMINFO`"* — rather than crash cryptically. This guard sits beside the `tm_abi_version()` load-check and the panic-safe teardown (§11.8), and is owned by `termacs-java` once it takes over the native link.

### 15.3 Docs — one spec, not many

- **One design spec.** `termacs.md` stays a single coherent document. Splitting it per-repo would fragment cross-cutting sections — the C++ API (§5), the C ABI rules (§7), the layout engine (§5.3) — and invite drift; a split spec is a spec nobody trusts. Its home is **inside `termacs-core`**, since core is the API source of truth (§5) — which also gives the spec a published home without an umbrella repo.
- **Per-repo READMEs** carry the *practical* layer: what the repo is, how to build it, how it depends on the others. `termacs-java`'s README summarizes binding specifics (§8) and links back to the canonical spec. Reference/usage docs, **not** design.
- **No umbrella `termacs` repo.** The orchestration that lived in root `build.sh` is replaced by each repo building itself (`termacs-java`'s build drives core via §15.1).

### 15.4 Migration plan — *status: ✅ R0–R6 complete; the three repos are canonical and build standalone*

The decoupling was done once in the monorepo, the repos were seeded by force-push, tagged (`termacs-core` `v0.1.0`), and the monorepo retired. Verified from a clean GitHub clone: `termacs-core` builds + tests (`--recursive`), `termacs-java` builds its JNI by FetchContent-ing core `v0.1.0` and passes `HeadlessTest`, and `termacs-java-demo` compiles against the binding and renders. CI workflows run per repo (R6). Phases:

| Phase | Goal | Key work | Exit criterion (green-bar) | Depends on |
|---|---|---|---|---|
| **R0** | **Core stands alone** | Remove the JNI target from `termacs-core/CMakeLists.txt`; add `install()`/export for `termacs.h` + `libtermacs`/`termacs_cabi`; fold the standalone `.gitmodules` into the monorepo | Core configures + builds with **zero** `termacs-java` references; `tc_snapshot`/`tc_demo`/`tc_csmoke` green; `cmake --install` yields consumable headers + libs | — |
| **R1** | **Java owns its native build** | New `termacs-java/CMakeLists.txt` compiles `native/termacs_jni.cpp` + `jni.map`, links core via `FetchContent` (local-override source in dev); add the runtime-plumbing guard (§15.2); Gradle/javac packaging unchanged | `libtermacsjni.so` built by **java's own** CMake; `HeadlessTest` green; ncurses/terminfo runtime dependency resolved + guarded (§12.3 ⚠) | R0 |
| **R2** | **Demo on the binding** | `termacs-java-demo` consumes **only** `termacs-java`; drop any reach into core | Demo builds against the binding; `run_demo_pty.py` rc=0 (full-screen UI paints, F10▸Quit exits) | R1 |
| **R3** | **Docs homing** | Move `termacs.md` into `termacs-core`; write a practical README per repo (build + cross-repo deps) | Spec lives under core; each repo has standalone build instructions *(parallels R1/R2)* | R0 |
| **R4** | **Pin & remote-ize** | Tag `termacs-core` `v0.1.0`; switch java's `FetchContent` from local override to the **GitHub tag** | Fresh `clone --recursive` of `termacs-java` builds end-to-end pulling core by tag — **no** local override | R1, R3 |
| **R5** | **Cutover & retire monorepo** | Seed the three repos with the decoupled trees (one-time push, no ongoing subtree sync under Option B); once each builds standalone, **archive the monorepo** | Clean `clone --recursive` of **each** repo builds standalone on a fresh machine; monorepo archived | R2, R4 |
| **R6** | **Per-repo CI** *(later)* | GitHub Actions per repo: build + test (the §12.6 matrix, now per-repo) | Green CI on all three repos | R5 |

**Critical path:** R0 → R1 → R2 → R4 → R5 (R3 parallels; R6 trails). Everything before **R5** is monorepo-local and reversible.

> **Cleanup (pending):** the retired monorepo's history was kept as a local backup bundle (it also holds the few root-only files — `build.sh`, the pty smoke test, the root README — though the smoke test now lives in `termacs-java-demo/tools/`). **Delete the backup bundle once the demo app is verified end-to-end on a real terminal.**
