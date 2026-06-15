// termacs — typed signals & connections (§5.4).
// C++ keeps compile-time-typed signals; the C ABI later flattens these to one
// variant TmEvent callback. Connections are Qt-style non-owning tokens: discarding
// one does NOT disconnect; a slot lives until explicitly disconnected or its owning
// widget is removed (which clears the widget's signals).
#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <vector>

namespace termacs {

class Connection {
public:
    Connection() = default;
    explicit Connection(uint64_t id) : id_(id) {}
    uint64_t id() const { return id_; }
    bool valid() const { return id_ != 0; }
private:
    uint64_t id_ = 0;
};

template <class... Args>
class Signal {
public:
    Connection connect(std::function<void(Args...)> fn) {
        slots_.push_back({++last_, std::move(fn)});
        return Connection(last_);
    }
    void disconnect(Connection c) {
        slots_.erase(std::remove_if(slots_.begin(), slots_.end(),
                                    [&](const Slot& s) { return s.id == c.id(); }),
                     slots_.end());
    }
    void emit(Args... a) const {
        // Copy first: a slot may mutate the widget tree (and thus this signal).
        auto snapshot = slots_;
        for (auto& s : snapshot) s.fn(a...);
    }
    void clear() { slots_.clear(); }
    bool empty() const { return slots_.empty(); }

private:
    struct Slot { uint64_t id; std::function<void(Args...)> fn; };
    std::vector<Slot> slots_;
    uint64_t          last_ = 0;
};

} // namespace termacs
