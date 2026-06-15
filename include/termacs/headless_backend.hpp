// termacs — HeadlessBackend: in-memory cell buffer + scripted input. For snapshot
// tests and headless runs; no terminal required (§11.10).
#pragma once
#include <deque>
#include "backend.hpp"

namespace termacs {

class HeadlessBackend : public Backend {
public:
    HeadlessBackend(int w, int h) : size_{w, h} {}

    Size         size() override { return size_; }
    Capabilities caps() override { return {ColorDepth::TrueColor, true, true}; }
    int          measureWidth(std::string_view utf8) override { return textWidth(utf8); }
    void         present(const CellBuffer& b) override { last_ = b; ++frames_; }
    bool         pollInput(InputEvent& out, int) override {
        if (queue_.empty()) return false;
        out = queue_.front();
        queue_.pop_front();
        return true;
    }
    void wake() override {}
    void shutdown() override {}

    // ---- test helpers ----
    void resizeScreen(int w, int h) { size_ = {w, h}; queue_.push_back(InputEvent{InputKind::Resize, {}, {}, {w, h}}); }
    void push(const InputEvent& ev) { queue_.push_back(ev); }
    const CellBuffer& lastFrame() const { return last_; }
    int  frames() const { return frames_; }

private:
    Size                  size_;
    CellBuffer            last_;
    std::deque<InputEvent> queue_;
    int                   frames_ = 0;
};

} // namespace termacs
