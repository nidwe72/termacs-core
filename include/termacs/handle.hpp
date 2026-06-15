// termacs — the opaque, invalidatable node handle (§4). Core owns the tree; a handle
// is a non-owning {index, generation} reference. A call on a stale handle throws.
#pragma once
#include <cstdint>
#include <stdexcept>

namespace termacs {

struct NodeId {
    uint32_t index = 0;
    uint32_t gen   = 0;   // 0 ⇒ null handle
    bool null() const { return gen == 0; }
    bool operator==(const NodeId& o) const { return index == o.index && gen == o.gen; }
};

class InvalidHandle : public std::runtime_error {
public:
    InvalidHandle() : std::runtime_error("termacs: use of an invalidated widget handle") {}
};

} // namespace termacs
