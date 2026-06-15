// termacs — factory for the real terminal backend (magiblot/tvision under the hood).
// Header is tvision-free; all tvision includes live in tvision_backend.cpp.
#pragma once
#include <memory>
#include "backend.hpp"

namespace termacs {

std::unique_ptr<Backend> makeTerminalBackend();

} // namespace termacs
