#include "termacs/widget.hpp"
#include "internal.hpp"
#include <algorithm>

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

// ----- P5 widget factories (§5.10) --------------------------------------------
CheckBox Container::addCheckBox(const std::string& text) {
    auto [id, p] = core_->create<CheckBoxNode>();
    p->text = text;
    core_->addChild(id_, id);
    return CheckBox(core_, id);
}
OptionGroup Container::addOptionGroup(SelectMode mode) {
    auto [id, p] = core_->create<OptionGroupNode>();
    p->mode = (mode == SelectMode::Many) ? 1 : 0;
    core_->addChild(id_, id);
    return OptionGroup(core_, id);
}
OptionGroup Container::addRadioGroup() { return addOptionGroup(SelectMode::One); }
OptionGroup Container::addCheckGroup() { return addOptionGroup(SelectMode::Many); }
ComboBox Container::addComboBox() {
    auto [id, p] = core_->create<ComboBoxNode>(); (void)p;
    core_->addChild(id_, id);
    return ComboBox(core_, id);
}
ProgressBar Container::addProgressBar() {
    auto [id, p] = core_->create<ProgressBarNode>(); (void)p;
    core_->addChild(id_, id);
    return ProgressBar(core_, id);
}
TextArea Container::addTextArea() {
    auto [id, p] = core_->create<TextAreaNode>(); (void)p;
    core_->addChild(id_, id);
    return TextArea(core_, id);
}
Frame Container::addFrame(const std::string& title) {
    auto [id, p] = core_->create<FrameNode>();
    p->title = title;
    core_->addChild(id_, id);
    return Frame(core_, id);
}
ScrollView Container::addScrollView() {
    auto [id, p] = core_->create<ScrollViewNode>(); (void)p;
    core_->addChild(id_, id);
    return ScrollView(core_, id);
}

// ----- Label ------------------------------------------------------------------
void        Label::setText(const std::string& t) { reqAs<LabelNode>(core_, id_).text = t; }
std::string Label::text() const { return reqAs<LabelNode>(core_, id_).text; }

// ----- Button -----------------------------------------------------------------
void      Button::setText(const std::string& t) { reqAs<ButtonNode>(core_, id_).text = t; }
void      Button::setVariant(ButtonVariant v)   { reqAs<ButtonNode>(core_, id_).variant = (int)v; }
void      Button::setDefault(bool d)            { reqAs<ButtonNode>(core_, id_).isDefault = d; }
Signal<>& Button::clicked() { return reqAs<ButtonNode>(core_, id_).clicked; }

// ----- CheckBox ---------------------------------------------------------------
void CheckBox::setText(const std::string& t) { reqAs<CheckBoxNode>(core_, id_).text = t; }
void CheckBox::setChecked(bool c)            { reqAs<CheckBoxNode>(core_, id_).checked = c; }
bool CheckBox::isChecked() const             { return reqAs<CheckBoxNode>(core_, id_).checked; }
Signal<bool>& CheckBox::toggled()            { return reqAs<CheckBoxNode>(core_, id_).toggled; }

// ----- OptionGroup ------------------------------------------------------------
void OptionGroup::setOptions(const std::vector<std::string>& opts) {
    auto& n = reqAs<OptionGroupNode>(core_, id_);
    n.options = opts; n.on.assign(opts.size(), 0); n.cursor = 0;
}
void OptionGroup::setOrientation(Axis a) { reqAs<OptionGroupNode>(core_, id_).orient = a; }
void OptionGroup::setSelectedIndex(int i) {
    auto& n = reqAs<OptionGroupNode>(core_, id_);
    std::fill(n.on.begin(), n.on.end(), 0);
    if (i >= 0 && i < (int)n.on.size()) { n.on[i] = 1; n.cursor = i; }
}
int OptionGroup::selectedIndex() const {
    auto& n = reqAs<OptionGroupNode>(core_, id_);
    for (int i = 0; i < (int)n.on.size(); ++i) if (n.on[i]) return i;
    return -1;
}
void OptionGroup::setSelected(int i, bool on) {
    auto& n = reqAs<OptionGroupNode>(core_, id_);
    if (i < 0 || i >= (int)n.on.size()) return;
    if (n.mode == 0) std::fill(n.on.begin(), n.on.end(), 0);
    n.on[i] = on ? 1 : 0;
}
bool OptionGroup::isSelected(int i) const {
    auto& n = reqAs<OptionGroupNode>(core_, id_);
    return (i >= 0 && i < (int)n.on.size()) && n.on[i];
}
std::vector<int> OptionGroup::selectedIndices() const {
    auto& n = reqAs<OptionGroupNode>(core_, id_);
    std::vector<int> r;
    for (int i = 0; i < (int)n.on.size(); ++i) if (n.on[i]) r.push_back(i);
    return r;
}
Signal<int>& OptionGroup::selectionChanged() { return reqAs<OptionGroupNode>(core_, id_).selectionChanged; }

// ----- ComboBox ---------------------------------------------------------------
void ComboBox::setOptions(const std::vector<std::string>& opts) {
    auto& n = reqAs<ComboBoxNode>(core_, id_); n.options = opts; n.sel = -1; n.cursor = 0;
}
void ComboBox::setSelectedIndex(int i) {
    auto& n = reqAs<ComboBoxNode>(core_, id_);
    if (i >= -1 && i < (int)n.options.size()) { n.sel = i; if (i >= 0) n.cursor = i; }
}
int ComboBox::selectedIndex() const { return reqAs<ComboBoxNode>(core_, id_).sel; }
std::string ComboBox::selectedText() const {
    auto& n = reqAs<ComboBoxNode>(core_, id_);
    return (n.sel >= 0 && n.sel < (int)n.options.size()) ? n.options[n.sel] : std::string();
}
void ComboBox::setPlaceholder(const std::string& p) { reqAs<ComboBoxNode>(core_, id_).placeholder = p; }
Signal<int>& ComboBox::selectionChanged() { return reqAs<ComboBoxNode>(core_, id_).selectionChanged; }

// ----- ProgressBar ------------------------------------------------------------
void ProgressBar::setValue(int v) { reqAs<ProgressBarNode>(core_, id_).value = std::max(0, std::min(100, v)); }
int  ProgressBar::value() const   { return reqAs<ProgressBarNode>(core_, id_).value; }

// ----- TextArea ---------------------------------------------------------------
void TextArea::setText(const std::string& t) { reqAs<TextAreaNode>(core_, id_).buf.setText(t); }
std::string TextArea::text() const { return reqAs<TextAreaNode>(core_, id_).buf.text(); }
void TextArea::appendLine(const std::string& t) {
    auto& n = reqAs<TextAreaNode>(core_, id_);
    std::string cur = n.buf.text();
    n.buf.setText(cur.empty() ? t : cur + "\n" + t);
}
void TextArea::setReadOnly(bool r) { reqAs<TextAreaNode>(core_, id_).readOnly = r; }
void TextArea::setWordWrap(bool w) { reqAs<TextAreaNode>(core_, id_).wrap = w; }
void TextArea::setPlaceholder(const std::string& p) { reqAs<TextAreaNode>(core_, id_).placeholder = p; }
Signal<const std::string&>& TextArea::textChanged() { return reqAs<TextAreaNode>(core_, id_).textChanged; }
// §5.11 editing surface
void        TextArea::selectAll()          { reqAs<TextAreaNode>(core_, id_).buf.selectAll(); }
std::string TextArea::selectedText() const { return reqAs<TextAreaNode>(core_, id_).buf.selectedText(); }
void        TextArea::copy()  { auto& n = reqAs<TextAreaNode>(core_, id_); core_->clipboard = n.buf.copy(); }
void        TextArea::cut()   { auto& n = reqAs<TextAreaNode>(core_, id_); core_->clipboard = n.buf.cut(); n.textChanged.emit(n.buf.text()); }
void        TextArea::paste() { auto& n = reqAs<TextAreaNode>(core_, id_); n.buf.paste(core_->clipboard); n.textChanged.emit(n.buf.text()); }
Signal<>&   TextArea::selectionChanged() { return reqAs<TextAreaNode>(core_, id_).selectionChanged; }

// ----- Frame / ScrollView -----------------------------------------------------
void Frame::setTitle(const std::string& t) { reqAs<FrameNode>(core_, id_).title = t; }
void ScrollView::scrollTo(int x, int y) {
    auto& n = reqAs<ScrollViewNode>(core_, id_); n.offX = std::max(0, x); n.offY = std::max(0, y);
}

// ----- LineEdit ---------------------------------------------------------------
void LineEdit::setText(const std::string& t) { reqAs<LineEditNode>(core_, id_).buf.setText(t); }
std::string LineEdit::text() const { return reqAs<LineEditNode>(core_, id_).buf.text(); }
Signal<const std::string&>& LineEdit::textChanged() { return reqAs<LineEditNode>(core_, id_).textChanged; }
Signal<>&                   LineEdit::submitted()   { return reqAs<LineEditNode>(core_, id_).submitted; }
// §5.11 editing surface
void        LineEdit::selectAll()            { reqAs<LineEditNode>(core_, id_).buf.selectAll(); }
std::string LineEdit::selectedText() const   { return reqAs<LineEditNode>(core_, id_).buf.selectedText(); }
void        LineEdit::copy()  { auto& n = reqAs<LineEditNode>(core_, id_); core_->clipboard = n.buf.copy(); }
void        LineEdit::cut()   { auto& n = reqAs<LineEditNode>(core_, id_); core_->clipboard = n.buf.cut(); n.textChanged.emit(n.buf.text()); }
void        LineEdit::paste() { auto& n = reqAs<LineEditNode>(core_, id_); n.buf.paste(core_->clipboard); n.textChanged.emit(n.buf.text()); }
Signal<>&   LineEdit::selectionChanged() { return reqAs<LineEditNode>(core_, id_).selectionChanged; }

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
