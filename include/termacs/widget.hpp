// termacs — public widget facades. Each is a lightweight value handle {Core*, NodeId}
// that forwards to the core-owned node. Created via parent factory methods (no `new`).
#pragma once
#include <string>
#include <vector>
#include "handle.hpp"
#include "signal.hpp"
#include "sizing.hpp"
#include "style.hpp"   // ControlStyle

namespace termacs {

class Core;

// ----- base -------------------------------------------------------------------
class Widget {
public:
    Widget() = default;
    Widget(Core* c, NodeId id) : core_(c), id_(id) {}

    bool   valid() const;
    NodeId id()    const { return id_; }
    Core*  core()  const { return core_; }

    void setVisible(bool v);
    void setEnabled(bool v);
    void setFocusable(bool v);
    void setFocus();
    void setSizing(Axis axis, Sizing s);
    void setControlStyle(ControlStyle s);   // §5.12; per-widget override of the app default
    void remove();   // destroys this subtree; invalidates handles into it

protected:
    Core*  core_ = nullptr;
    NodeId id_{};
};

class Label;
class Button;
class LineEdit;
class ListView;
class HBox;
class VBox;
class CheckBox;
class OptionGroup;
class ComboBox;
class ProgressBar;
class Frame;
class ScrollView;
class TextArea;

enum class SelectMode { One, Many };   // OptionGroup: single (radio) vs multi (check)

// ----- container (VBox/HBox) factories ---------------------------------------
class Container : public Widget {
public:
    using Widget::Widget;
    void setPadding(int cells);
    void setSpacing(int cells);

    Label       addLabel(const std::string& text);
    Button      addButton(const std::string& text);
    LineEdit    addLineEdit();
    ListView    addListView();
    HBox        addHBox();
    VBox        addVBox();
    // P5 selection & input widgets (§5.10)
    CheckBox    addCheckBox(const std::string& text);
    OptionGroup addOptionGroup(SelectMode mode);
    OptionGroup addRadioGroup();   // = addOptionGroup(One)
    OptionGroup addCheckGroup();   // = addOptionGroup(Many)
    ComboBox    addComboBox();
    ProgressBar addProgressBar();
    TextArea    addTextArea();
    Frame       addFrame(const std::string& title);
    ScrollView  addScrollView();
};

class VBox : public Container { public: using Container::Container; };
class HBox : public Container { public: using Container::Container; };

// A bordered, titled container (decoration only).
class Frame : public Container { public: using Container::Container; void setTitle(const std::string& t); };

// A clipping viewport that scrolls content larger than itself.
class ScrollView : public Container { public: using Container::Container; void scrollTo(int x, int y); };

// ----- leaf widgets -----------------------------------------------------------
class Label : public Widget {
public:
    using Widget::Widget;
    void        setText(const std::string& t);
    std::string text() const;
};

enum class ButtonVariant { Normal, Primary, Danger, Quiet };

class Button : public Widget {
public:
    using Widget::Widget;
    void        setText(const std::string& t);
    void        setVariant(ButtonVariant v);
    void        setDefault(bool d);   // fires on unhandled Enter in its window
    Signal<>&   clicked();
};

class LineEdit : public Widget {
public:
    using Widget::Widget;
    void                        setText(const std::string& t);
    std::string                 text() const;
    Signal<const std::string&>& textChanged();
    Signal<>&                   submitted();   // Enter pressed in the field
    // §5.11 editing surface
    void                        selectAll();
    std::string                 selectedText() const;
    void                        copy(); void cut(); void paste();
    Signal<>&                   selectionChanged();
};

class ListView : public Widget {
public:
    using Widget::Widget;
    void        addItem(const std::string& t);
    void        removeItem(int row);
    void        setItems(const std::vector<std::string>& items);
    std::string itemAt(int row) const;
    int         count() const;
    int         selected() const;
    Signal<int>& activated();   // Enter on a row
};

// ----- P5 selection & input widgets (§5.10) -----------------------------------
class CheckBox : public Widget {
public:
    using Widget::Widget;
    void          setText(const std::string& t);
    void          setChecked(bool c);
    bool          isChecked() const;
    Signal<bool>& toggled();
};

class OptionGroup : public Widget {
public:
    using Widget::Widget;
    void setOptions(const std::vector<std::string>& opts);
    void setOrientation(Axis a);
    void setSelectedIndex(int i);          // single (One) mode
    int  selectedIndex() const;
    void setSelected(int i, bool on);      // multi (Many) mode
    bool isSelected(int i) const;
    std::vector<int> selectedIndices() const;
    Signal<int>& selectionChanged();       // i = the item that changed
};

class ComboBox : public Widget {
public:
    using Widget::Widget;
    void        setOptions(const std::vector<std::string>& opts);
    void        setSelectedIndex(int i);
    int         selectedIndex() const;
    std::string selectedText() const;
    void        setPlaceholder(const std::string& p);
    Signal<int>& selectionChanged();
};

class ProgressBar : public Widget {
public:
    using Widget::Widget;
    void setValue(int percent);   // clamped 0..100
    int  value() const;
};

class TextArea : public Widget {
public:
    using Widget::Widget;
    void        setText(const std::string& t);
    std::string text() const;
    void        appendLine(const std::string& t);
    void        setReadOnly(bool r);
    void        setWordWrap(bool w);
    void        setPlaceholder(const std::string& p);
    Signal<const std::string&>& textChanged();
    // §5.11 editing surface
    void        selectAll();
    std::string selectedText() const;
    void        copy(); void cut(); void paste();
    Signal<>&   selectionChanged();
};

// ----- menus & status bar -----------------------------------------------------
class Menu : public Widget {
public:
    using Widget::Widget;
    void addItem(const std::string& label, std::function<void()> onActivate);
};

class MenuBar : public Widget {
public:
    using Widget::Widget;
    Menu addMenu(const std::string& title);
};

class StatusBar : public Widget {
public:
    using Widget::Widget;
    void setText(const std::string& left);
    void setRightText(const std::string& right);
};

// ----- window -----------------------------------------------------------------
class Window : public Widget {
public:
    using Widget::Widget;
    void      setTitle(const std::string& t);
    void      resize(int w, int h);   // cells; {0,0} ⇒ fill screen
    MenuBar   setMenuBar();
    StatusBar setStatusBar();
    VBox      setContentVBox();
    HBox      setContentHBox();
    void      show();
};

} // namespace termacs
