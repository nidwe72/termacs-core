// termacs — public widget facades. Each is a lightweight value handle {Core*, NodeId}
// that forwards to the core-owned node. Created via parent factory methods (no `new`).
#pragma once
#include <string>
#include <vector>
#include "handle.hpp"
#include "signal.hpp"
#include "sizing.hpp"

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

// ----- container (VBox/HBox) factories ---------------------------------------
class Container : public Widget {
public:
    using Widget::Widget;
    void setPadding(int cells);
    void setSpacing(int cells);

    Label    addLabel(const std::string& text);
    Button   addButton(const std::string& text);
    LineEdit addLineEdit();
    ListView addListView();
    HBox     addHBox();
    VBox     addVBox();
};

class VBox : public Container { public: using Container::Container; };
class HBox : public Container { public: using Container::Container; };

// ----- leaf widgets -----------------------------------------------------------
class Label : public Widget {
public:
    using Widget::Widget;
    void        setText(const std::string& t);
    std::string text() const;
};

class Button : public Widget {
public:
    using Widget::Widget;
    void        setText(const std::string& t);
    Signal<>&   clicked();
};

class LineEdit : public Widget {
public:
    using Widget::Widget;
    void                        setText(const std::string& t);
    std::string                 text() const;
    Signal<const std::string&>& textChanged();
    Signal<>&                   submitted();   // Enter pressed in the field
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
