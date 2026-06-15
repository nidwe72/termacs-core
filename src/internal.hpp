// termacs — INTERNAL: the node tree + Core registry. Not an installed header.
// Public facades (widget.hpp) resolve to these nodes through Core.
#pragma once
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "termacs/backend.hpp"
#include "termacs/cell.hpp"
#include "termacs/handle.hpp"
#include "termacs/signal.hpp"
#include "termacs/sizing.hpp"
#include "termacs/theme.hpp"

namespace termacs {

class Core;

enum class NodeKind { Window, VBox, HBox, Label, Button, LineEdit, ListView, MenuBar, Menu, StatusBar, Dialog };

// ---------------------------------------------------------------------------
class Node {
public:
    explicit Node(NodeKind k) : kind(k) {}
    virtual ~Node() = default;

    NodeKind            kind;
    NodeId              self{};
    NodeId              parent{};
    std::vector<NodeId> children;
    Rect                bounds{};
    Sizing              hsize{}, vsize{};
    bool                visible = true, enabled = true, focusable = false;

    Sizing& sizing(Axis a) { return a == Axis::Horizontal ? hsize : vsize; }

    virtual Size contentSize(Core&, Backend&)          { return {0, 0}; }
    virtual void arrange(Core&, Backend&)              {}
    virtual void draw(Core&, Canvas&, const Theme&)    {}
    virtual bool handleKey(Core&, const KeyEvent&)     { return false; }
    virtual void onRemoved()                           {}
};

// preferred extent of a node along one axis (clamped to its sizing)
int prefAlong(Node& n, Core& core, Backend& be, Axis ax);

// ---------------------------------------------------------------------------
class ContainerNode : public Node {
public:
    explicit ContainerNode(NodeKind k) : Node(k) {}   // VBox or HBox
    int  padding = 0;
    int  spacing = 0;
    Axis mainAxis() const { return kind == NodeKind::VBox ? Axis::Vertical : Axis::Horizontal; }

    Size contentSize(Core&, Backend&) override;
    void arrange(Core&, Backend&) override;
    void draw(Core&, Canvas&, const Theme&) override;
};

class LabelNode : public Node {
public:
    LabelNode() : Node(NodeKind::Label) {}
    std::string text;
    Size contentSize(Core&, Backend&) override;
    void draw(Core&, Canvas&, const Theme&) override;
};

class ButtonNode : public Node {
public:
    ButtonNode() : Node(NodeKind::Button) { focusable = true; }
    std::string text;
    Signal<>    clicked;
    Size contentSize(Core&, Backend&) override;
    void draw(Core&, Canvas&, const Theme&) override;
    bool handleKey(Core&, const KeyEvent&) override;
    void onRemoved() override { clicked.clear(); }
};

class LineEditNode : public Node {
public:
    LineEditNode() : Node(NodeKind::LineEdit) { focusable = true; }
    std::string                text;
    int                        cursor = 0;   // codepoint index (ASCII-simple)
    Signal<const std::string&> textChanged;
    Signal<>                   submitted;
    Size contentSize(Core&, Backend&) override;
    void draw(Core&, Canvas&, const Theme&) override;
    bool handleKey(Core&, const KeyEvent&) override;
    void onRemoved() override { textChanged.clear(); submitted.clear(); }
};

class ListViewNode : public Node {
public:
    ListViewNode() : Node(NodeKind::ListView) { focusable = true; }
    std::vector<std::string> items;
    int                      sel = 0;
    int                      top = 0;   // scroll offset
    Signal<int>              activated;
    Size contentSize(Core&, Backend&) override;
    void draw(Core&, Canvas&, const Theme&) override;
    bool handleKey(Core&, const KeyEvent&) override;
    void onRemoved() override { activated.clear(); }
};

struct MenuItem { std::string label; std::function<void()> onActivate; };

class MenuNode : public Node {
public:
    MenuNode() : Node(NodeKind::Menu) {}
    std::string           title;
    std::vector<MenuItem> items;
    int                   sel = 0;
    Rect                  popup{};   // dropdown rect, set during render (for hit-testing)
    // drawn as a popup when open; see Core popup handling
    void draw(Core&, Canvas&, const Theme&) override;
};

class MenuBarNode : public Node {
public:
    MenuBarNode() : Node(NodeKind::MenuBar) {}
    std::vector<NodeId> menus;   // MenuNode ids
    int                 open = -1;   // index of open menu, -1 = none
    int                 hot  = 0;
    void draw(Core&, Canvas&, const Theme&) override;
};

class StatusBarNode : public Node {
public:
    StatusBarNode() : Node(NodeKind::StatusBar) {}
    std::string left, right;
    void draw(Core&, Canvas&, const Theme&) override;
};

class WindowNode : public Node {
public:
    WindowNode() : Node(NodeKind::Window) {}
    std::string title;
    Size        desired{0, 0};   // {0,0} ⇒ fill screen
    NodeId      menuBar{}, statusBar{}, content{};
    void arrange(Core&, Backend&) override;
    void draw(Core&, Canvas&, const Theme&) override;
};

// A modal dialog on the floating layer (info / confirm / prompt).
class DialogNode : public Node {
public:
    DialogNode() : Node(NodeKind::Dialog) { focusable = true; }
    std::string                            title, message;
    std::vector<std::string>               buttons;   // e.g. {"Yes","No"} or {"OK"}
    int                                    focusBtn = 0;
    bool                                   hasInput = false;
    std::string                            input;
    std::function<void(int, std::string)>  onClose;   // (buttonIndex, inputText)
    void draw(Core&, Canvas&, const Theme&) override;
    bool handleKey(Core&, const KeyEvent&) override;
};

// ---------------------------------------------------------------------------
class Core {
public:
    std::vector<std::unique_ptr<Node>> slots;
    std::vector<uint32_t>              gens;
    std::vector<uint32_t>              freeList;

    Backend* backend = nullptr;
    Theme    theme = Theme::builtin(Theme::Builtin::Dark);

    std::vector<NodeId> windows;   // root windows (z-order)
    std::vector<NodeId> popups;    // floating layer: open menu / dialogs (top = last)
    NodeId              focused{};

    template <class T, class... A>
    std::pair<NodeId, T*> create(A&&... args) {
        auto node = std::make_unique<T>(std::forward<A>(args)...);
        T* raw = node.get();
        uint32_t idx;
        if (!freeList.empty()) { idx = freeList.back(); freeList.pop_back(); slots[idx] = std::move(node); }
        else { idx = static_cast<uint32_t>(slots.size()); slots.push_back(std::move(node)); gens.push_back(1); }
        NodeId id{idx, gens[idx]};
        raw->self = id;
        return {id, raw};
    }

    Node* resolve(NodeId id) {
        if (id.gen == 0 || id.index >= slots.size()) return nullptr;
        if (gens[id.index] != id.gen || !slots[id.index]) return nullptr;
        return slots[id.index].get();
    }
    Node& require(NodeId id) {
        Node* n = resolve(id);
        if (!n) throw InvalidHandle();
        return *n;
    }
    template <class T> T* as(NodeId id) { return dynamic_cast<T*>(resolve(id)); }

    void addChild(NodeId parent, NodeId child);
    void destroy(NodeId id);

    // focus
    void                setFocus(NodeId id);
    NodeId              activeRoot();                 // top popup, else top window
    std::vector<NodeId> focusOrder(NodeId root);
    void                focusStep(int dir);

    // layout + render
    void layoutAll(Size screen);
    void drawNode(NodeId id, Canvas& cv);
    void render(CellBuffer& out);

    // input
    bool   dispatchKey(const KeyEvent& ev);
    bool   dispatchMouse(const MouseEvent& ev);
    NodeId hitTest(NodeId root, int x, int y);

private:
    void collectFocusable(NodeId id, std::vector<NodeId>& out);
};

} // namespace termacs
