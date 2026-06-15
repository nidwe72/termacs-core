#include "termacs/widget.hpp"
#include "internal.hpp"

namespace termacs {

static Node& req(Core* c, NodeId id) {
    if (!c) throw InvalidHandle();
    return c->require(id);
}
template <class T> static T& reqAs(Core* c, NodeId id) {
    T* p = dynamic_cast<T*>(&req(c, id));
    if (!p) throw InvalidHandle();
    return *p;
}

// ----- Widget -----------------------------------------------------------------
bool Widget::valid() const { return core_ && core_->resolve(id_) != nullptr; }
void Widget::setVisible(bool v)   { req(core_, id_).visible = v; }
void Widget::setEnabled(bool v)   { req(core_, id_).enabled = v; }
void Widget::setFocusable(bool v) { req(core_, id_).focusable = v; }
void Widget::setFocus()           { req(core_, id_); core_->setFocus(id_); }
void Widget::setSizing(Axis a, Sizing s) { req(core_, id_).sizing(a) = s; }
void Widget::remove()             { if (core_) core_->destroy(id_); }

// ----- Container --------------------------------------------------------------
void Container::setPadding(int c) { reqAs<ContainerNode>(core_, id_).padding = c; }
void Container::setSpacing(int c) { reqAs<ContainerNode>(core_, id_).spacing = c; }

Label Container::addLabel(const std::string& text) {
    auto [id, p] = core_->create<LabelNode>();
    p->text = text;
    core_->addChild(id_, id);
    return Label(core_, id);
}
Button Container::addButton(const std::string& text) {
    auto [id, p] = core_->create<ButtonNode>();
    p->text = text;
    core_->addChild(id_, id);
    return Button(core_, id);
}
LineEdit Container::addLineEdit() {
    auto [id, p] = core_->create<LineEditNode>();
    (void)p;
    core_->addChild(id_, id);
    return LineEdit(core_, id);
}
ListView Container::addListView() {
    auto [id, p] = core_->create<ListViewNode>();
    (void)p;
    core_->addChild(id_, id);
    return ListView(core_, id);
}
HBox Container::addHBox() {
    auto [id, p] = core_->create<ContainerNode>(NodeKind::HBox);
    (void)p;
    core_->addChild(id_, id);
    return HBox(core_, id);
}
VBox Container::addVBox() {
    auto [id, p] = core_->create<ContainerNode>(NodeKind::VBox);
    (void)p;
    core_->addChild(id_, id);
    return VBox(core_, id);
}

// ----- Label ------------------------------------------------------------------
void        Label::setText(const std::string& t) { reqAs<LabelNode>(core_, id_).text = t; }
std::string Label::text() const { return reqAs<LabelNode>(core_, id_).text; }

// ----- Button -----------------------------------------------------------------
void      Button::setText(const std::string& t) { reqAs<ButtonNode>(core_, id_).text = t; }
Signal<>& Button::clicked() { return reqAs<ButtonNode>(core_, id_).clicked; }

// ----- LineEdit ---------------------------------------------------------------
void LineEdit::setText(const std::string& t) {
    auto& n = reqAs<LineEditNode>(core_, id_);
    n.text = t; n.cursor = (int)t.size();
}
std::string LineEdit::text() const { return reqAs<LineEditNode>(core_, id_).text; }
Signal<const std::string&>& LineEdit::textChanged() { return reqAs<LineEditNode>(core_, id_).textChanged; }
Signal<>&                   LineEdit::submitted()   { return reqAs<LineEditNode>(core_, id_).submitted; }

// ----- ListView ---------------------------------------------------------------
void ListView::addItem(const std::string& t) { reqAs<ListViewNode>(core_, id_).items.push_back(t); }
void ListView::removeItem(int row) {
    auto& n = reqAs<ListViewNode>(core_, id_);
    if (row >= 0 && row < (int)n.items.size()) {
        n.items.erase(n.items.begin() + row);
        if (n.sel >= (int)n.items.size()) n.sel = std::max(0, (int)n.items.size() - 1);
    }
}
void ListView::setItems(const std::vector<std::string>& items) {
    auto& n = reqAs<ListViewNode>(core_, id_);
    n.items = items; n.sel = 0; n.top = 0;
}
std::string ListView::itemAt(int row) const {
    auto& n = reqAs<ListViewNode>(core_, id_);
    return (row >= 0 && row < (int)n.items.size()) ? n.items[row] : std::string();
}
int          ListView::count() const    { return (int)reqAs<ListViewNode>(core_, id_).items.size(); }
int          ListView::selected() const { return reqAs<ListViewNode>(core_, id_).sel; }
Signal<int>& ListView::activated()      { return reqAs<ListViewNode>(core_, id_).activated; }

// ----- Menu / MenuBar / StatusBar ---------------------------------------------
void Menu::addItem(const std::string& label, std::function<void()> onActivate) {
    reqAs<MenuNode>(core_, id_).items.push_back({label, std::move(onActivate)});
}
Menu MenuBar::addMenu(const std::string& title) {
    auto [id, p] = core_->create<MenuNode>();
    p->title = title;
    auto& bar = reqAs<MenuBarNode>(core_, id_);
    core_->addChild(id_, id);
    bar.menus.push_back(id);
    return Menu(core_, id);
}
void StatusBar::setText(const std::string& left)       { reqAs<StatusBarNode>(core_, id_).left = left; }
void StatusBar::setRightText(const std::string& right) { reqAs<StatusBarNode>(core_, id_).right = right; }

// ----- Window -----------------------------------------------------------------
void Window::setTitle(const std::string& t) { reqAs<WindowNode>(core_, id_).title = t; }
void Window::resize(int w, int h)           { reqAs<WindowNode>(core_, id_).desired = {w, h}; }
MenuBar Window::setMenuBar() {
    auto [id, p] = core_->create<MenuBarNode>();
    (void)p;
    core_->addChild(id_, id);
    reqAs<WindowNode>(core_, id_).menuBar = id;
    return MenuBar(core_, id);
}
StatusBar Window::setStatusBar() {
    auto [id, p] = core_->create<StatusBarNode>();
    (void)p;
    core_->addChild(id_, id);
    reqAs<WindowNode>(core_, id_).statusBar = id;
    return StatusBar(core_, id);
}
VBox Window::setContentVBox() {
    auto [id, p] = core_->create<ContainerNode>(NodeKind::VBox);
    (void)p;
    core_->addChild(id_, id);
    reqAs<WindowNode>(core_, id_).content = id;
    return VBox(core_, id);
}
HBox Window::setContentHBox() {
    auto [id, p] = core_->create<ContainerNode>(NodeKind::HBox);
    (void)p;
    core_->addChild(id_, id);
    reqAs<WindowNode>(core_, id_).content = id;
    return HBox(core_, id);
}
void Window::show() {
    // give focus to the first focusable widget if nothing is focused yet
    if (!core_->resolve(core_->focused)) {
        auto order = core_->focusOrder(id_);
        if (!order.empty()) core_->setFocus(order.front());
    }
}

} // namespace termacs
